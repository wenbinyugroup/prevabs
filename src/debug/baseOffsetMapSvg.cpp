// baseOffsetMapSvg.cpp — see include/debug/baseOffsetMapSvg.hpp.

#include "debug/baseOffsetMapSvg.hpp"

#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct BBox {
  double min_y = std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  double min_z = std::numeric_limits<double>::infinity();
  double max_z = -std::numeric_limits<double>::infinity();
  bool empty() const { return !std::isfinite(min_y); }
  void add(double y, double z) {
    if (y < min_y) min_y = y;
    if (y > max_y) max_y = y;
    if (z < min_z) min_z = z;
    if (z > max_z) max_z = z;
  }
};

// Project a single PreVABS y-z point into the SVG pixel frame.
struct Xform {
  double s;       // pixels per data unit
  double tx, ty;  // translation in pixel space
  double svgX(double y) const { return s * y + tx; }
  double svgY(double z) const { return -s * z + ty; }
};

// Trim very small magnitudes to zero to keep markup readable.
inline double snap(double v) {
  return std::fabs(v) < 1e-12 ? 0.0 : v;
}

void writeCircle(std::ostream& os, double cx, double cy, double r,
                 const std::string& fill, const std::string& stroke) {
  os << "<circle cx=\"" << snap(cx) << "\" cy=\"" << snap(cy)
     << "\" r=\"" << r
     << "\" fill=\"" << fill << "\" stroke=\"" << stroke
     << "\" stroke-width=\"0.5\" />\n";
}

// Axis-aligned square marker. Centered at (cx, cy), side = 2 * half.
void writeSquare(std::ostream& os, double cx, double cy, double half,
                 const std::string& fill, const std::string& stroke,
                 double stroke_w) {
  os << "<rect x=\"" << snap(cx - half) << "\" y=\"" << snap(cy - half)
     << "\" width=\"" << (2 * half) << "\" height=\"" << (2 * half)
     << "\" fill=\"" << fill << "\" stroke=\"" << stroke
     << "\" stroke-width=\"" << stroke_w << "\" />\n";
}

void writeText(std::ostream& os, double x, double y, double font_px,
               const std::string& fill, const std::string& text) {
  os << "<text x=\"" << snap(x) << "\" y=\"" << snap(y)
     << "\" font-family=\"sans-serif\" font-size=\"" << font_px
     << "\" fill=\"" << fill << "\">" << text << "</text>\n";
}

}  // namespace

void dumpBaseOffsetMapSvg(const std::string& path,
                          const std::string& title,
                          const std::vector<PDCELVertex*>& base,
                          const std::vector<PDCELVertex*>& offset,
                          const BaseOffsetMap& pairs,
                          const std::vector<bool>* offset_resampled,
                          const std::vector<SPoint2>* pre_resample_raw_points) {
  if (base.empty() && offset.empty()) {
    PLOG(warning) << "dumpBaseOffsetMapSvg: nothing to draw for '" << title
                  << "', skipping";
    return;
  }

  // 1. Compute data bbox from all referenced vertices.
  BBox bb;
  for (auto* v : base)   { if (v) bb.add(v->y(), v->z()); }
  for (auto* v : offset) { if (v) bb.add(v->y(), v->z()); }
  if (pre_resample_raw_points) {
    for (const auto& p : *pre_resample_raw_points) bb.add(p.x(), p.y());
  }
  if (bb.empty()) {
    PLOG(warning) << "dumpBaseOffsetMapSvg: all vertices null for '" << title
                  << "', skipping";
    return;
  }

  // 2. Pick a fixed canvas; fit data into it with uniform scaling.
  const double W = 1600.0, H = 800.0, margin = 60.0;
  const double dy = std::max(bb.max_y - bb.min_y, 1e-12);
  const double dz = std::max(bb.max_z - bb.min_z, 1e-12);
  const double s  = std::min((W - 2 * margin) / dy, (H - 2 * margin) / dz);

  // svgX(min_y) = margin (left), svgY(max_z) = margin (top) when the data
  // fully fills one axis; otherwise center the data on the under-used axis.
  Xform x;
  x.s  = s;
  x.tx = margin - s * bb.min_y
       + 0.5 * std::max(0.0, (W - 2 * margin) - s * dy);
  x.ty = margin + s * bb.max_z
       + 0.5 * std::max(0.0, (H - 2 * margin) - s * dz);

  // 3. Open the file and write the header.
  std::ofstream os(path);
  if (!os) {
    PLOG(error) << "dumpBaseOffsetMapSvg: cannot open '" << path
                << "' for write";
    return;
  }
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  os << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
     << W << " " << H << "\" width=\"" << W << "\" height=\"" << H
     << "\">\n";
  os << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";

  // 4. Title bar.
  std::size_t n_raw = 0;
  std::size_t n_resampled = 0;
  if (offset_resampled && offset_resampled->size() == offset.size()) {
    for (std::size_t i = 0; i < offset.size(); ++i) {
      if ((*offset_resampled)[i]) ++n_resampled; else ++n_raw;
    }
  } else {
    n_raw = offset.size();
  }
  const std::size_t n_pre_raw =
      pre_resample_raw_points ? pre_resample_raw_points->size() : 0;
  std::ostringstream subtitle;
  subtitle << "base verts = " << base.size()
           << ",  offset verts = " << offset.size()
           << " (raw " << n_raw << " / resampled " << n_resampled << ")"
           << ",  pre-resample raw = " << n_pre_raw
           << ",  pairs = " << pairs.size()
           << ",  bbox y=[" << bb.min_y << ", " << bb.max_y
           << "]  z=[" << bb.min_z << ", " << bb.max_z << "]";
  writeText(os, margin, 24, 18, "#222", "base-offset map: " + title);
  writeText(os, margin, 44, 12, "#555", subtitle.str());

  // 5. Mapping lines (drawn first so vertex markers cover the endpoints).
  os << "<g stroke=\"#888\" stroke-width=\"0.7\" opacity=\"0.7\">\n";
  for (const auto& p : pairs) {
    if (p.base < 0 || p.offset < 0) continue;
    if (static_cast<std::size_t>(p.base)   >= base.size())   continue;
    if (static_cast<std::size_t>(p.offset) >= offset.size()) continue;
    auto* vb = base[p.base];
    auto* vo = offset[p.offset];
    if (!vb || !vo) continue;
    os << "<line x1=\"" << snap(x.svgX(vb->y()))
       << "\" y1=\"" << snap(x.svgY(vb->z()))
       << "\" x2=\"" << snap(x.svgX(vo->y()))
       << "\" y2=\"" << snap(x.svgY(vo->z()))
       << "\" />\n";
  }
  os << "</g>\n";

  // 6. Base curve polyline.
  os << "<polyline fill=\"none\" stroke=\"#1f77b4\" stroke-width=\"1.4\""
     << " points=\"";
  for (auto* v : base) {
    if (!v) continue;
    os << snap(x.svgX(v->y())) << "," << snap(x.svgY(v->z())) << " ";
  }
  os << "\" />\n";

  // 7. Offset curve polyline.
  os << "<polyline fill=\"none\" stroke=\"#ff7f0e\" stroke-width=\"1.4\""
     << " points=\"";
  for (auto* v : offset) {
    if (!v) continue;
    os << snap(x.svgX(v->y())) << "," << snap(x.svgY(v->z())) << " ";
  }
  os << "\" />\n";

  // 8. Vertex markers + index labels.
  const double R = 3.0;
  for (std::size_t i = 0; i < base.size(); ++i) {
    auto* v = base[i];
    if (!v) continue;
    const double cx = x.svgX(v->y()), cy = x.svgY(v->z());
    writeCircle(os, cx, cy, R, "#1f77b4", "#0b3d61");
    writeText(os, cx + 4, cy - 4, 10, "#0b3d61", "b" + std::to_string(i));
  }
  // Offset vertices: raw Clipper2 = filled circle, resampled (synthesized
  // at a base-vertex perpendicular foot) = hollow square. If no tag
  // vector was supplied, fall back to "all raw".
  const bool have_tag =
      offset_resampled && offset_resampled->size() == offset.size();
  for (std::size_t i = 0; i < offset.size(); ++i) {
    auto* v = offset[i];
    if (!v) continue;
    const double cx = x.svgX(v->y()), cy = x.svgY(v->z());
    const bool resampled = have_tag && (*offset_resampled)[i];
    if (resampled) {
      // Hollow square, slightly larger than the disk marker — easier to
      // pick out when raw + resampled markers overlap on a single base
      // vertex's projection foot.
      writeSquare(os, cx, cy, R + 1.0, "none", "#7a3d05", 1.2);
    } else {
      writeCircle(os, cx, cy, R, "#ff7f0e", "#7a3d05");
    }
    writeText(os, cx + 4, cy + 12, 10, "#7a3d05", "o" + std::to_string(i));
  }

  // 8b. Topmost overlay: Clipper2 raw run vertices captured immediately
  // before the open-path resample step replaced them wholesale. These
  // positions have no counterpart in `offset` — the resample commits
  // `r.points = new_pts` and tags every slot `resampled=true`, so this
  // layer is the only way to see the raw Clipper2 placement that the
  // staircase was originally going to align to.
  if (pre_resample_raw_points && !pre_resample_raw_points->empty()) {
    const double r_pre = R * 0.6;
    for (const auto& p : *pre_resample_raw_points) {
      const double cx = x.svgX(p.x()), cy = x.svgY(p.y());
      writeCircle(os, cx, cy, r_pre, "#e41a1c", "#5a0606");
    }
  }

  // 9. Legend (top-right corner of the canvas).
  const double lx = W - 240, ly = 30;
  os << "<g font-family=\"sans-serif\" font-size=\"12\">\n";
  os << "<rect x=\"" << lx - 10 << "\" y=\"" << ly - 18
     << "\" width=\"230\" height=\"142\" fill=\"#fafafa\""
     << " stroke=\"#ccc\"/>\n";
  // base curve line + filled blue disk marker
  os << "<line x1=\"" << lx << "\" y1=\"" << ly << "\" x2=\"" << lx + 24
     << "\" y2=\"" << ly << "\" stroke=\"#1f77b4\" stroke-width=\"2\"/>\n";
  writeCircle(os, lx + 12, ly, R, "#1f77b4", "#0b3d61");
  writeText(os, lx + 32, ly + 4, 12, "#0b3d61", "base vertex (b_i)");
  // offset curve line
  os << "<line x1=\"" << lx << "\" y1=\"" << ly + 20 << "\" x2=\"" << lx + 24
     << "\" y2=\"" << ly + 20 << "\" stroke=\"#ff7f0e\" stroke-width=\"2\"/>\n";
  writeText(os, lx + 32, ly + 24, 12, "#7a3d05", "offset curve");
  // raw Clipper2 marker
  writeCircle(os, lx + 12, ly + 40, R, "#ff7f0e", "#7a3d05");
  writeText(os, lx + 32, ly + 44, 12, "#7a3d05", "offset: raw Clipper2");
  // resampled marker
  writeSquare(os, lx + 12, ly + 60, R + 1.0, "white", "#7a3d05", 1.2);
  writeText(os, lx + 32, ly + 64, 12, "#7a3d05", "offset: base-vertex resample");
  // pair line
  os << "<line x1=\"" << lx << "\" y1=\"" << ly + 80 << "\" x2=\"" << lx + 24
     << "\" y2=\"" << ly + 80 << "\" stroke=\"#888\" stroke-width=\"0.7\"/>\n";
  writeText(os, lx + 32, ly + 84, 12, "#444", "pair (b_i, o_j)");
  // pre-resample raw vertex marker (smaller red dot)
  writeCircle(os, lx + 12, ly + 100, R * 0.6, "#e41a1c", "#5a0606");
  writeText(os, lx + 32, ly + 104, 12, "#5a0606", "raw vertex (pre-resample)");
  os << "</g>\n";

  os << "</svg>\n";

  PLOG(info) << "wrote base-offset map svg: " << path;
}
