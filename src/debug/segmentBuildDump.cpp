// segmentBuildDump.cpp — see include/debug/segmentBuildDump.hpp.

#include "debug/segmentBuildDump.hpp"

#include "Material.hpp"
#include "PBaseLine.hpp"
#include "dcel/PDCEL.hpp"
#include "dcel/PDCELFace.hpp"
#include "dcel/PDCELHalfEdge.hpp"
#include "dcel/PDCELVertex.hpp"
#include "PModel.hpp"
#include "PSegment.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel

namespace prevabs {
namespace debug {

namespace {

constexpr int kFaceWalkCap = 100000;
// A face whose |area| falls below this (relative to the segment bbox) or
// whose smallest interior angle is below kMinAngleDeg is flagged as a
// geometric-quality problem the integration pass/fail would miss.
constexpr double kMinAngleDeg = 5.0;

struct FaceInfo {
  std::string name;
  int layer = -1;            // parsed from "<seg>_layer_K_face_I"; -1 if none
  std::string material;
  double theta3 = 0.0;
  std::vector<SPoint2> poly;  // boundary vertices (y, z), CCW or CW
  double signed_area = 0.0;
  double min_angle_deg = 180.0;
  bool inverted = false;      // sign opposite to the segment majority
  bool sliver = false;        // tiny area or a near-degenerate corner
  bool degenerate = false;    // < 3 vertices or ~zero area
};

double signedArea(const std::vector<SPoint2> &p) {
  if (p.size() < 3) return 0.0;
  double a = 0.0;
  for (std::size_t i = 0; i < p.size(); ++i) {
    const auto &u = p[i];
    const auto &v = p[(i + 1) % p.size()];
    a += u.x() * v.y() - v.x() * u.y();
  }
  return 0.5 * a;
}

double minInteriorAngleDeg(const std::vector<SPoint2> &p) {
  const std::size_t n = p.size();
  if (n < 3) return 0.0;
  double best = 180.0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto &prev = p[(i + n - 1) % n];
    const auto &cur = p[i];
    const auto &next = p[(i + 1) % n];
    const double ax = prev.x() - cur.x(), ay = prev.y() - cur.y();
    const double bx = next.x() - cur.x(), by = next.y() - cur.y();
    const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
    if (la < 1e-15 || lb < 1e-15) return 0.0;  // duplicate vertex
    double c = (ax * bx + ay * by) / (la * lb);
    c = std::max(-1.0, std::min(1.0, c));
    const double ang = std::acos(c) * 180.0 / PI;
    best = std::min(best, ang);
  }
  return best;
}

// Walk a face's outer boundary into a (y, z) polygon. Returns empty on a
// broken/over-long loop.
std::vector<SPoint2> facePolygon(PDCELFace *f) {
  std::vector<SPoint2> poly;
  if (f == nullptr || f->outer() == nullptr) return poly;
  PDCELHalfEdge *start = f->outer();
  PDCELHalfEdge *he = start;
  int guard = 0;
  do {
    if (he == nullptr || he->source() == nullptr) return {};
    if (++guard > kFaceWalkCap) return {};
    poly.emplace_back(he->source()->y(), he->source()->z());
    he = he->next();
  } while (he != start);
  return poly;
}

// "<seg>_layer_K_face_I" -> K (1-based). Returns -1 when not a layer face.
int parseLayerIndex(const std::string &name, const std::string &seg) {
  const std::string key = seg + "_layer_";
  if (name.compare(0, key.size(), key) != 0) return -1;
  int k = 0;
  std::size_t i = key.size();
  if (i >= name.size() || !std::isdigit(name[i])) return -1;
  while (i < name.size() && std::isdigit(name[i])) k = k * 10 + (name[i++] - '0');
  return k;
}

// Belongs to this segment iff name == seg OR name starts with seg + "_".
// The trailing "_" separator avoids the sgm_1 / sgm_10 prefix collision.
bool nameBelongsToSegment(const std::string &name, const std::string &seg) {
  if (name == seg) return true;
  return name.size() > seg.size() &&
         name.compare(0, seg.size(), seg) == 0 && name[seg.size()] == '_';
}

// The shell face "<seg>_face" is the leftover un-tiled remnant — exclude it
// from the tiled-cell report (it is not a layer cell).
bool isShellFace(const std::string &name, const std::string &seg) {
  return name == seg + "_face";
}

struct BBox {
  double miny = std::numeric_limits<double>::infinity();
  double maxy = -std::numeric_limits<double>::infinity();
  double minz = std::numeric_limits<double>::infinity();
  double maxz = -std::numeric_limits<double>::infinity();
  void add(double y, double z) {
    miny = std::min(miny, y); maxy = std::max(maxy, y);
    minz = std::min(minz, z); maxz = std::max(maxz, z);
  }
  bool empty() const { return !std::isfinite(miny); }
};

struct Xform {
  double s = 1.0, tx = 0.0, ty = 0.0;
  double X(double y) const { return s * y + tx; }
  double Y(double z) const { return -s * z + ty; }
};

const char *layerFill(int layer) {
  // Soft, distinct fills cycled by layer index. layer<=0 (legacy area /
  // single-layer) uses a neutral grey.
  static const char *pal[] = {"#cfe8ff", "#ffe0b3", "#d7f5d3", "#f5d0e6",
                              "#fff5b3", "#d6d3f5", "#c7eef0", "#f5c7c7"};
  if (layer <= 0) return "#e8e8e8";
  return pal[(layer - 1) % 8];
}

std::string jsonEscape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else o += c;
  }
  return o;
}

void writePolyline(std::ostream &os, const std::vector<PDCELVertex *> &v,
                   const Xform &x, const char *stroke) {
  os << "<polyline fill=\"none\" stroke=\"" << stroke
     << "\" stroke-width=\"1.6\" points=\"";
  for (auto *p : v) {
    if (p) os << x.X(p->y()) << "," << x.Y(p->z()) << " ";
  }
  os << "\" />\n";
}

}  // namespace

bool segmentDumpEnabled() {
  static const bool on = [] {
    const char *raw = std::getenv("PREVABS_SEGMENT_DUMP");
    return raw != nullptr && *raw != '\0';
  }();
  return on;
}

bool segmentTraceEnabled() {
  // The full dump implies the trace; PREVABS_SEGMENT_PATH enables the trace
  // alone (no SVG/JSON), as used by the integration suite.
  static const bool on = [] {
    if (segmentDumpEnabled()) return true;
    const char *raw = std::getenv("PREVABS_SEGMENT_PATH");
    return raw != nullptr && *raw != '\0';
  }();
  return on;
}

namespace {

// Per-segment trace state. Keyed by segment name so the offset phase and the
// area phase — which run in separate all-segments passes — append to the same
// file even though they are not adjacent in time.
struct SegTrace {
  std::string file;                 // resolved <file_base>.<seg>.segment.path.txt
  std::vector<std::string> steps;   // indented step lines (without trailing \n)
  bool started = false;             // file truncated + header written this run
};

std::map<std::string, SegTrace> &traceStore() {
  static std::map<std::string, SegTrace> s;
  return s;
}
// The segment currently being traced (set by segmentTraceBegin); empty when no
// segment is active, in which case pushes are dropped (e.g. offset helpers
// reached from a non-segment context such as a unit test).
std::string &currentSeg() {
  static std::string s;
  return s;
}
// Current call-stack depth (one indent level == 2 spaces).
int &traceDepth() {
  static int d = 0;
  return d;
}

}  // namespace

void segmentTraceBegin(const std::string &segname) {
  if (!segmentTraceEnabled()) return;
  currentSeg() = segname;
  traceDepth() = 0;
  SegTrace &t = traceStore()[segname];
  if (t.file.empty()) {
    t.file = config.file_directory + config.file_base_name + "."
             + segname + ".segment.path.txt";
  }
  // Truncate + header once per segment per process; later phases append.
  if (!t.started) {
    t.started = true;
    t.steps.clear();
    std::ofstream ps(t.file, std::ios::trunc);
    if (ps) ps << "# build path for segment '" << segname << "'\n";
  }
}

void segmentTracePush(const std::string &step) {
  if (!segmentTraceEnabled()) return;
  const std::string &seg = currentSeg();
  if (seg.empty()) return;
  SegTrace &t = traceStore()[seg];
  std::string line(static_cast<std::size_t>(2 * std::max(0, traceDepth())), ' ');
  line += step;
  t.steps.push_back(line);
  // Append + flush immediately (close) so the line survives a later crash.
  if (!t.file.empty()) {
    std::ofstream ps(t.file, std::ios::app);
    if (ps) ps << line << "\n";
  }
}

const std::vector<std::string> &segmentTraceSteps() {
  static const std::vector<std::string> empty;
  const std::string &seg = currentSeg();
  if (seg.empty()) return empty;
  return traceStore()[seg].steps;
}

SegmentTraceScope::SegmentTraceScope(const std::string &label)
    : active_(segmentTraceEnabled()) {
  if (!active_) return;
  segmentTracePush(label);
  ++traceDepth();
}

SegmentTraceScope::~SegmentTraceScope() {
  if (!active_) return;
  if (traceDepth() > 0) --traceDepth();
}

void dumpSegmentBuild(Segment &seg, const BuilderConfig &bcfg) {
  if (!segmentDumpEnabled()) return;
  if (bcfg.dcel == nullptr || bcfg.model == nullptr) return;

  const std::string segname = seg.getName();
  const bool is_closed = seg.closed();

  // 1. Collect this segment's tiled faces (exclude the shell remnant).
  std::vector<FaceInfo> faces;
  for (PDCELFace *f : bcfg.dcel->faces()) {
    const std::string &name = bcfg.model->faceData(f).name;
    if (!nameBelongsToSegment(name, segname)) continue;
    if (isShellFace(name, segname)) continue;
    FaceInfo fi;
    fi.name = name;
    fi.layer = parseLayerIndex(name, segname);
    fi.theta3 = f->theta3();
    if (f->material() != nullptr) fi.material = f->material()->getName();
    fi.poly = facePolygon(f);
    fi.signed_area = signedArea(fi.poly);
    fi.min_angle_deg = minInteriorAngleDeg(fi.poly);
    fi.degenerate = fi.poly.size() < 3 || std::fabs(fi.signed_area) < 1e-14;
    faces.push_back(std::move(fi));
  }
  if (faces.empty()) return;

  // 2. Quality post-pass: majority orientation sign + sliver detection.
  int pos = 0, neg = 0;
  for (const auto &f : faces) {
    if (f.signed_area > 0) ++pos; else if (f.signed_area < 0) ++neg;
  }
  const double majority_sign = (pos >= neg) ? 1.0 : -1.0;
  double max_abs_area = 0.0;
  for (const auto &f : faces) max_abs_area = std::max(max_abs_area, std::fabs(f.signed_area));
  int n_inverted = 0, n_sliver = 0, n_degenerate = 0;
  for (auto &f : faces) {
    if (!f.degenerate && f.signed_area * majority_sign < 0) f.inverted = true;
    if (f.min_angle_deg < kMinAngleDeg ||
        (max_abs_area > 0 && std::fabs(f.signed_area) < 1e-4 * max_abs_area)) {
      f.sliver = true;
    }
    if (f.inverted) ++n_inverted;
    if (f.sliver) ++n_sliver;
    if (f.degenerate) ++n_degenerate;
  }

  // Infer the build route from the face naming scheme.
  bool any_layer = false, any_area = false;
  for (const auto &f : faces) {
    if (f.layer >= 0) any_layer = true;
    if (f.name.find("_area_") != std::string::npos) any_area = true;
  }
  const std::string route = any_layer ? "layered" : (any_area ? "legacy" : "single");
  int n_layers = 0;
  for (const auto &f : faces) n_layers = std::max(n_layers, f.layer);

  const auto &base = seg.curveBase() ? seg.curveBase()->vertices()
                                     : std::vector<PDCELVertex *>{};
  const auto &offs = seg.curveOffset() ? seg.curveOffset()->vertices()
                                       : std::vector<PDCELVertex *>{};

  const std::string stem =
      config.file_directory + config.file_base_name + "." + segname + ".segment";

  // 2b. Build-path trace (which conditional branches this segment took). The
  // .path.txt was written incrementally by segmentTracePush as the build ran
  // (crash-safe); here we only read it back for the JSON `path` array.
  const std::vector<std::string> &path = segmentTraceSteps();

  // 3. JSON (machine-readable).
  {
    std::ofstream js(stem + ".json");
    if (js) {
      js << "{\n";
      js << "  \"segment\": \"" << jsonEscape(segname) << "\",\n";
      js << "  \"closed\": " << (is_closed ? "true" : "false") << ",\n";
      js << "  \"route\": \"" << route << "\",\n";
      js << "  \"base_vertices\": " << base.size() << ",\n";
      js << "  \"offset_vertices\": " << offs.size() << ",\n";
      js << "  \"layers\": " << n_layers << ",\n";
      js << "  \"face_count\": " << faces.size() << ",\n";
      js << "  \"n_inverted\": " << n_inverted << ",\n";
      js << "  \"n_sliver\": " << n_sliver << ",\n";
      js << "  \"n_degenerate\": " << n_degenerate << ",\n";
      js << "  \"ok\": "
         << ((n_inverted == 0 && n_degenerate == 0) ? "true" : "false") << ",\n";
      js << "  \"path\": [";
      for (std::size_t i = 0; i < path.size(); ++i) {
        js << (i ? ", " : "") << "\"" << jsonEscape(path[i]) << "\"";
      }
      js << "],\n";
      js << "  \"faces\": [\n";
      for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto &f = faces[i];
        js << "    {\"name\": \"" << jsonEscape(f.name) << "\", \"layer\": "
           << f.layer << ", \"material\": \"" << jsonEscape(f.material)
           << "\", \"theta3\": " << f.theta3 << ", \"verts\": " << f.poly.size()
           << ", \"area\": " << f.signed_area << ", \"min_angle_deg\": "
           << f.min_angle_deg << ", \"inverted\": "
           << (f.inverted ? "true" : "false") << ", \"sliver\": "
           << (f.sliver ? "true" : "false") << ", \"degenerate\": "
           << (f.degenerate ? "true" : "false") << "}"
           << (i + 1 < faces.size() ? "," : "") << "\n";
      }
      js << "  ]\n}\n";
    }
  }

  // 4. SVG (human-readable).
  BBox bb;
  for (const auto &f : faces) for (const auto &p : f.poly) bb.add(p.x(), p.y());
  for (auto *v : base) if (v) bb.add(v->y(), v->z());
  for (auto *v : offs) if (v) bb.add(v->y(), v->z());
  if (bb.empty()) return;

  const double W = 1600.0, H = 900.0, margin = 70.0;
  const double dy = std::max(bb.maxy - bb.miny, 1e-12);
  const double dz = std::max(bb.maxz - bb.minz, 1e-12);
  Xform x;
  x.s = std::min((W - 2 * margin) / dy, (H - 2 * margin) / dz);
  x.tx = margin - x.s * bb.miny +
         0.5 * std::max(0.0, (W - 2 * margin) - x.s * dy);
  x.ty = margin + x.s * bb.maxz +
         0.5 * std::max(0.0, (H - 2 * margin) - x.s * dz);

  std::ofstream os(stem + ".svg");
  if (!os) {
    PLOG(error) << "dumpSegmentBuild: cannot open " << stem << ".svg";
    return;
  }
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  os << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << W << " "
     << H << "\" width=\"" << W << "\" height=\"" << H << "\">\n";
  os << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";

  std::ostringstream sub;
  sub << "route=" << route << "  closed=" << (is_closed ? "Y" : "N")
      << "  base=" << base.size() << "  layers=" << n_layers
      << "  faces=" << faces.size() << "  inverted=" << n_inverted
      << "  sliver=" << n_sliver << "  degenerate=" << n_degenerate;
  os << "<text x=\"" << margin << "\" y=\"28\" font-family=\"sans-serif\""
     << " font-size=\"18\" fill=\"#222\">segment build: " << segname
     << "</text>\n";
  os << "<text x=\"" << margin << "\" y=\"48\" font-family=\"sans-serif\""
     << " font-size=\"12\" fill=\"#555\">" << sub.str() << "</text>\n";

  // Tiled faces, filled by layer; problem faces overstroked.
  for (const auto &f : faces) {
    if (f.poly.size() < 2) continue;
    const char *fill = f.degenerate ? "#bbbbbb" : layerFill(f.layer);
    const char *stroke = "#666";
    double sw = 0.6;
    if (f.inverted) { stroke = "#d62728"; sw = 2.2; fill = "#f3b0b0"; }
    else if (f.sliver) { stroke = "#ff7f0e"; sw = 1.6; }
    os << "<polygon points=\"";
    for (const auto &p : f.poly) os << x.X(p.x()) << "," << x.Y(p.y()) << " ";
    os << "\" fill=\"" << fill << "\" fill-opacity=\"0.75\" stroke=\"" << stroke
       << "\" stroke-width=\"" << sw << "\" />\n";
  }

  // Base (blue) and offset/shell (orange) reference polylines on top.
  if (!base.empty()) writePolyline(os, base, x, "#1f77b4");
  if (!offs.empty()) writePolyline(os, offs, x, "#ff7f0e");

  // Legend.
  const double lx = W - 250, ly = 70;
  os << "<g font-family=\"sans-serif\" font-size=\"12\">\n";
  os << "<rect x=\"" << lx - 12 << "\" y=\"" << ly - 18
     << "\" width=\"242\" height=\"118\" fill=\"#fafafa\" stroke=\"#ccc\"/>\n";
  os << "<line x1=\"" << lx << "\" y1=\"" << ly << "\" x2=\"" << lx + 24
     << "\" y2=\"" << ly << "\" stroke=\"#1f77b4\" stroke-width=\"2\"/>\n";
  os << "<text x=\"" << lx + 32 << "\" y=\"" << ly + 4 << "\">base curve</text>\n";
  os << "<line x1=\"" << lx << "\" y1=\"" << ly + 20 << "\" x2=\"" << lx + 24
     << "\" y2=\"" << ly + 20 << "\" stroke=\"#ff7f0e\" stroke-width=\"2\"/>\n";
  os << "<text x=\"" << lx + 32 << "\" y=\"" << ly + 24
     << "\">offset / shell</text>\n";
  os << "<rect x=\"" << lx << "\" y=\"" << ly + 32 << "\" width=\"20\""
     << " height=\"12\" fill=\"#f3b0b0\" stroke=\"#d62728\""
     << " stroke-width=\"2\"/>\n";
  os << "<text x=\"" << lx + 32 << "\" y=\"" << ly + 42
     << "\">inverted face</text>\n";
  os << "<rect x=\"" << lx << "\" y=\"" << ly + 52 << "\" width=\"20\""
     << " height=\"12\" fill=\"#fff5b3\" stroke=\"#ff7f0e\""
     << " stroke-width=\"1.6\"/>\n";
  os << "<text x=\"" << lx + 32 << "\" y=\"" << ly + 62
     << "\">sliver face</text>\n";
  os << "<text x=\"" << lx << "\" y=\"" << ly + 84
     << "\" fill=\"#444\">fills cycle by layer index</text>\n";
  os << "</g>\n";

  os << "</svg>\n";

  PLOG(info) << "segment build dump: " << stem << ".{svg,json}"
             << " route=" << route << " faces=" << faces.size()
             << " inverted=" << n_inverted << " sliver=" << n_sliver;
}

}  // namespace debug
}  // namespace prevabs
