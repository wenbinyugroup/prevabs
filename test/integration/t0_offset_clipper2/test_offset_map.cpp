// Standalone harness for iterating on Clipper2-offset →
// staircase-base-offset-map construction.
//
// See CMakeLists.txt for coupling principle. This TU only depends on
// `offset_clipper2.hpp` (pure-logic bridge layer) + std + Clipper2.
// No PDCELVertex / no gmsh / no spdlog. Pure prototype harness.

#include "offset_clipper2.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using prevabs::geo::OffsetPolygon;
using prevabs::geo::offsetWithClipper2;
using prevabs::geo::ReverseMatchPlan;
using prevabs::geo::planReverseMatch;
using prevabs::geo::planReverseMatchByNearest;
using prevabs::geo::planReverseMatchByDP;
using prevabs::geo::PairingAlgo;
using prevabs::geo::ThinRun;
using prevabs::geo::extractSingleInteriorRun;
using prevabs::geo::buildTrimmedOpenPolyline;
using prevabs::geo::remapBaseSegToOriginal;
using prevabs::geo::JoinTypeChoice;

const char* joinName(JoinTypeChoice j) {
  switch (j) {
    case JoinTypeChoice::Miter:  return "miter";
    case JoinTypeChoice::Square: return "square";
    case JoinTypeChoice::Bevel:  return "bevel";
    case JoinTypeChoice::Round:  return "round";
  }
  return "?";
}

// ---------------------------------------------------------------------------
// Scenario definition.
// ---------------------------------------------------------------------------

struct Scenario {
  std::string          name;
  std::vector<SPoint2> base;
  bool                 closed;
  int                  side;       // +1 left / -1 right (closed CCW: -1=inward)
  double               dist;       // offset distance (magnitude)
  // Optional pre-trim α to apply (0.0 = no pre-trim, e.g. 1.2 = Stage E α).
  double               pretrim_alpha;
};

// ---------------------------------------------------------------------------
// Synthetic builders.
// ---------------------------------------------------------------------------

Scenario makeClosedSquare() {
  return {"closed_square",
          {SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
           SPoint2(1.0, 1.0), SPoint2(0.0, 1.0)},
          /*closed*/ true, /*side*/ -1, /*dist*/ 0.1, /*alpha*/ 0.0};
}

Scenario makeOpenStraight() {
  return {"open_straight",
          {SPoint2(0.0, 0.0), SPoint2(1.0, 0.0),
           SPoint2(2.0, 0.0), SPoint2(3.0, 0.0)},
          false, +1, 0.05, 0.0};
}

// Open V with a wide apex (shallow cusp). Should be handled cleanly
// without pre-trim.
Scenario makeOpenVCuspShallow() {
  std::vector<SPoint2> pts;
  for (int i = 0; i <= 4; ++i) {
    const double x = -1.0 + 0.25 * i;
    const double y = 0.1 + 0.05 * i;       // gentle descent
    pts.emplace_back(x, y);
  }
  pts.emplace_back(0.0, 0.0);              // apex
  for (int i = 1; i <= 4; ++i) {
    const double x = 0.25 * i;
    const double y = 0.30 - 0.05 * (4 - i); // matching ascent
    pts.emplace_back(x, y);
  }
  return {"open_v_cusp_shallow", std::move(pts), false, +1, 0.05, 0.0};
}

// Open V with a sharp cusp — generic valley shape. signedHalfThickness
// at the apex returns INF because the inward ray (perpendicular to
// tangent) doesn't hit any segment of THIS polyline (it points away
// from the polyline body, not toward it).
//
// Useful regression for: "we should NOT pre-trim a valley shape" —
// pre-trim is only valid for TE-style fold-back cusps where the upper
// and lower sides of the polyline approach each other.
Scenario makeOpenVCuspSharp(double pretrim_alpha = 1.2) {
  std::vector<SPoint2> pts = {
      SPoint2(-1.00, 0.50),
      SPoint2(-0.75, 0.30),
      SPoint2(-0.50, 0.15),
      SPoint2(-0.30, 0.06),
      SPoint2(-0.15, 0.02),
      SPoint2( 0.00, 0.00),
      SPoint2( 0.15, 0.02),
      SPoint2( 0.30, 0.06),
      SPoint2( 0.50, 0.15),
      SPoint2( 0.75, 0.30),
      SPoint2( 1.00, 0.50),
  };
  return {"open_v_cusp_sharp", std::move(pts), false, +1, 0.05,
          pretrim_alpha};
}

// Open TE-style cusp — U-shape pointing right. Mimics mh104 sg_te
// baseline topology: upper surface from (-1, +ε) descending to the TE
// at (1, 0), then lower surface returning to (-1, -ε). signedHalfThickness
// at points near the TE returns finite (inward ray hits the opposite
// surface), so pre-trim α=1.2 triggers and removes the cusp interior.
Scenario makeOpenTECuspLike(double dist = 0.02,
                            double pretrim_alpha = 1.2) {
  std::vector<SPoint2> pts;
  // Upper surface: x descends from -1 to ~0.95 with thinning thickness.
  for (int i = 0; i <= 6; ++i) {
    const double x = -1.0 + (1.95) * (i / 6.0);   // -1 → 0.95
    const double y =  0.10 - 0.013 * i;            // 0.10 → 0.022
    pts.emplace_back(x, y);
  }
  pts.emplace_back(1.0, 0.0);                     // TE cusp
  // Lower surface mirrored.
  for (int i = 6; i >= 0; --i) {
    const double x = -1.0 + (1.95) * (i / 6.0);
    const double y = -(0.10 - 0.013 * i);          // -0.10 → -0.022
    pts.emplace_back(x, y);
  }
  return {"open_te_cusp_like", std::move(pts), false, +1, dist,
          pretrim_alpha};
}

// Closed long thin rectangle with one rounded end — caricature of a
// closed airfoil profile where one end (the "TE") is thinner than dist
// only at the very tip.
Scenario makeClosedThinTE() {
  std::vector<SPoint2> pts = {
      SPoint2(0.00,  0.05),  // LE top
      SPoint2(0.25,  0.10),
      SPoint2(0.50,  0.10),
      SPoint2(0.75,  0.06),
      SPoint2(1.00,  0.00),  // TE
      SPoint2(0.75, -0.06),
      SPoint2(0.50, -0.10),
      SPoint2(0.25, -0.10),
      SPoint2(0.00, -0.05),
      SPoint2(-0.05, 0.00),  // LE
  };
  return {"closed_thin_te", std::move(pts), true, -1, 0.04, 0.0};
}

// Load airfoil .dat (Selig format: header line, then "x y" pairs).
std::vector<SPoint2> loadAirfoilDat(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "failed to open %s\n", path.c_str());
    return {};
  }
  std::string line;
  std::getline(in, line);  // skip header
  std::vector<SPoint2> pts;
  double x, y;
  while (in >> x >> y) pts.emplace_back(x, y);
  return pts;
}

// Use mh104.dat — full closed airfoil contour. The CCW orientation
// expected by `offsetWithClipper2` matches a Selig-style file (TE → top
// → LE → bottom → TE) iff side=-1 produces an inward inset.
Scenario makeAirfoilMh104Closed(double dist = 0.02,
                                double pretrim_alpha = 1.2) {
  auto pts = loadAirfoilDat(std::string(T0_DATA_DIR) + "/mh104.dat");
  // Drop the trailing-duplicate vertex if present (Selig files often
  // end with a copy of the first point).
  if (pts.size() >= 2
      && pts.front().x() == pts.back().x()
      && pts.front().y() == pts.back().y()) {
    pts.pop_back();
  }
  return {"airfoil_mh104_closed", std::move(pts), true, -1, dist,
          pretrim_alpha};
}

// mh104 TE-only segment — chord-wise excerpt around the cusp. Open.
// Vertex range chosen to mimic `mh104_te_only.xml`'s sg_te baseline
// (cusp interior at the middle).
Scenario makeAirfoilMh104TEOpen(double dist = 0.02,
                                double pretrim_alpha = 1.2) {
  auto pts = loadAirfoilDat(std::string(T0_DATA_DIR) + "/mh104.dat");
  if (pts.empty()) return {"airfoil_mh104_te_open", {}, false, +1, dist, 0.0};
  // Slice ~9 vertices on each side of the TE (idx 0). Wrap from the
  // back of the file (lower surface, ascending toward TE) and continue
  // through the top side from index 0..8.
  const int n = static_cast<int>(pts.size());
  std::vector<SPoint2> open_te;
  for (int k = n - 9; k < n; ++k) open_te.push_back(pts[k]);
  for (int k = 0; k <= 8; ++k)    open_te.push_back(pts[k]);
  return {"airfoil_mh104_te_open", std::move(open_te), false, +1, dist,
          pretrim_alpha};
}

// ---------------------------------------------------------------------------
// Driver: run one scenario through all 3 matchers (+ optional pre-trim).
// ---------------------------------------------------------------------------

struct RunResult {
  std::string          label;
  bool                 ok            = false;
  std::size_t          pair_count    = 0;
  std::size_t          offset_count  = 0;
  std::size_t          dropped_count = 0;
  std::string          dropped_ranges;       // "[lo..hi],[lo..hi]"
  bool                 pretrimmed    = false;
  ThinRun              pretrim_run   = {0, 0};
  std::vector<SPoint2> trimmed_input;        // empty iff no pre-trim
  ReverseMatchPlan     plan;
};

std::string formatDroppedRanges(const ReverseMatchPlan& plan) {
  std::ostringstream oss;
  for (std::size_t k = 0; k < plan.dropped_base_ranges_lo.size(); ++k) {
    if (k) oss << ",";
    oss << "[" << plan.dropped_base_ranges_lo[k] << ".."
        << plan.dropped_base_ranges_hi[k] << "]";
  }
  return oss.str();
}

// `signedHalfThickness(base, i, side, closed)` ported from
// src/geo/offset.cpp:90 (PDCELVertex → SPoint2). Inward-normal ray
// distance from base[i] to the nearest non-adjacent base segment.
// Endpoint vertices on an open polyline always return INF (the local
// half-thickness is geometrically undefined there).
double signedHalfThicknessHarness(const std::vector<SPoint2>& base,
                                  int i, int side, bool closed) {
  const double INF_VAL = std::numeric_limits<double>::infinity();
  constexpr double kTol = 1e-12;
  const int n = static_cast<int>(base.size());
  if (n < 3) return INF_VAL;
  if (!closed && (i <= 0 || i >= n - 1)) return INF_VAL;

  const int i_prev = closed ? (i - 1 + n) % n : i - 1;
  const int i_next = closed ? (i + 1) % n     : i + 1;

  const SPoint2 p_i    = base[i];
  const SPoint2 p_prev = base[i_prev];
  const SPoint2 p_next = base[i_next];

  // Local tangent: centred diff (p_next - p_prev) — robust at cusps.
  const double tx     = p_next.x() - p_prev.x();
  const double ty     = p_next.y() - p_prev.y();
  const double t_norm = std::sqrt(tx * tx + ty * ty);
  if (t_norm < kTol) return INF_VAL;
  const double tx_n = tx / t_norm;
  const double ty_n = ty / t_norm;

  // Inward normal in yz: n × t with n = SVector3(side, 0, 0) gives
  //   (dir_x, dir_y) = (-side * ty_n,  side * tx_n).
  const double dir_x = -side * ty_n;
  const double dir_y =  side * tx_n;

  const int n_seg = closed ? n : (n - 1);
  double min_t = INF_VAL;
  for (int j = 0; j < n_seg; ++j) {
    if (j == i || j == i_prev) continue;       // skip segments at base[i]
    const int j_next = closed ? (j + 1) % n : j + 1;
    const SPoint2 a = base[j];
    const SPoint2 b = base[j_next];
    const double ex = b.x() - a.x();
    const double ey = b.y() - a.y();
    // Solve p_i + t * dir = a + s * (b - a).
    const double det = -dir_x * ey + dir_y * ex;
    if (std::fabs(det) < kTol) continue;        // parallel / collinear
    const double rx = a.x() - p_i.x();
    const double ry = a.y() - p_i.y();
    const double t = (-rx * ey + ry * ex) / det;
    const double sp = (-dir_x * ry + dir_y * rx) / -det;
    if (t > kTol && sp >= -kTol && sp <= 1.0 + kTol) {
      if (t < min_t) min_t = t;
    }
  }
  return min_t;
}

// Apply Stage E pre-trim (open base only). Returns (trimmed input,
// thin run). Uses the ported signedHalfThickness above + INF-neighbour
// rule (INF only counts as thin when a neighbour is finite-thin) to
// distinguish a real cusp apex from a healthy short open segment whose
// inward rays simply miss everything.
bool tryPreTrim(const Scenario& s,
                std::vector<SPoint2>* out_trimmed,
                ThinRun* out_run) {
  if (s.closed || !(s.pretrim_alpha > 0.0)) return false;
  const int n = static_cast<int>(s.base.size());
  if (n < 4) return false;
  const double threshold = s.pretrim_alpha * s.dist;

  std::vector<double> h(n, std::numeric_limits<double>::infinity());
  for (int i = 1; i < n - 1; ++i) {
    h[i] = signedHalfThicknessHarness(s.base, i, s.side, /*closed*/ false);
  }

  // Build finite_thin mask, then upgrade INF entries whose neighbour
  // is finite_thin.
  constexpr double kTol = 1e-12;
  std::vector<bool> finite_thin(n, false);
  for (int i = 1; i < n - 1; ++i) {
    if (h[i] < std::numeric_limits<double>::infinity() * 0.5
        && h[i] < threshold - kTol) {
      finite_thin[i] = true;
    }
  }
  std::vector<bool> mask(n, false);
  for (int i = 1; i < n - 1; ++i) {
    if (finite_thin[i]) {
      mask[i] = true;
    } else if (h[i] >= std::numeric_limits<double>::infinity() * 0.5) {
      const bool L = (i > 0)     && finite_thin[i - 1];
      const bool R = (i < n - 1) && finite_thin[i + 1];
      mask[i] = L || R;
    }
  }
  if (!extractSingleInteriorRun(mask, out_run)) return false;
  *out_trimmed = buildTrimmedOpenPolyline(s.base, *out_run);
  return out_trimmed->size() >= 2;
}

RunResult runOnce(const Scenario& s, PairingAlgo algo, bool with_pretrim,
                  JoinTypeChoice join     = JoinTypeChoice::Miter,
                  double         miter_lim = 2.0,
                  bool           resample_open = true) {
  RunResult r;
  switch (algo) {
    case PairingAlgo::SegmentProjection: r.label = "segment"; break;
    case PairingAlgo::Nearest:           r.label = "nearest"; break;
    case PairingAlgo::DP:                r.label = "dp";      break;
  }
  if (with_pretrim)              r.label += "+pretrim";
  if (join != JoinTypeChoice::Miter) {
    r.label += std::string("+") + joinName(join);
  }
  // For open inputs, "+raw" means the extractOpenRuns step-5 resample
  // was skipped. For closed inputs the flag has no effect (closed never
  // resamples) but we still tag the row "+raw" so the CSV / table row
  // is disambiguated from the default-resample row of the same matcher.
  if (!resample_open) r.label += "+raw";

  std::vector<SPoint2> clipper_input = s.base;
  if (with_pretrim) {
    if (tryPreTrim(s, &r.trimmed_input, &r.pretrim_run)) {
      r.pretrimmed   = true;
      clipper_input  = r.trimmed_input;
    }
  }

  // Clipper2 sign convention: closed CCW + PreVABS side=-1 (inward)
  // → Clipper2 δ < 0 → flip sign on side for closed only.
  const int clipper_side = s.closed ? -s.side : s.side;
  auto polygons = offsetWithClipper2(
      clipper_input, s.closed, clipper_side, s.dist, join, miter_lim,
      resample_open);
  if (r.pretrimmed) {
    remapBaseSegToOriginal(polygons, r.pretrim_run);
  }

  switch (algo) {
    case PairingAlgo::SegmentProjection:
      r.plan = planReverseMatch(s.base, s.closed, s.side, s.dist, polygons);
      break;
    case PairingAlgo::Nearest:
      r.plan =
          planReverseMatchByNearest(s.base, s.closed, s.side, s.dist, polygons);
      break;
    case PairingAlgo::DP:
      r.plan =
          planReverseMatchByDP(s.base, s.closed, s.side, s.dist, polygons);
      break;
  }

  // Union pre-trim thin_run into dropped ranges if not already covered.
  if (r.pretrimmed) {
    bool covered = false;
    for (std::size_t k = 0; k < r.plan.dropped_base_ranges_lo.size(); ++k) {
      if (r.plan.dropped_base_ranges_lo[k] <= r.pretrim_run.lo
          && r.plan.dropped_base_ranges_hi[k] >= r.pretrim_run.hi) {
        covered = true; break;
      }
    }
    if (!covered) {
      r.plan.dropped_base_ranges_lo.push_back(r.pretrim_run.lo);
      r.plan.dropped_base_ranges_hi.push_back(r.pretrim_run.hi);
    }
  }

  r.ok             = r.plan.ok;
  r.pair_count     = r.plan.id_pairs.size();
  r.offset_count   = r.plan.offset_points.size();
  r.dropped_count  = r.plan.dropped_base_ranges_lo.size();
  r.dropped_ranges = formatDroppedRanges(r.plan);
  return r;
}

// ---------------------------------------------------------------------------
// Minimal SVG dump — multi-panel comparison.
// ---------------------------------------------------------------------------

struct Bbox {
  double xmin, ymin, xmax, ymax;
  double w() const { return std::max(xmax - xmin, 1e-12); }
  double h() const { return std::max(ymax - ymin, 1e-12); }
};

Bbox computeBbox(const std::vector<SPoint2>& base,
                 const std::vector<SPoint2>& offset) {
  Bbox b{1e30, 1e30, -1e30, -1e30};
  auto extend = [&](const SPoint2& p) {
    if (p.x() < b.xmin) b.xmin = p.x();
    if (p.x() > b.xmax) b.xmax = p.x();
    if (p.y() < b.ymin) b.ymin = p.y();
    if (p.y() > b.ymax) b.ymax = p.y();
  };
  for (const auto& p : base)   extend(p);
  for (const auto& p : offset) extend(p);
  return b;
}

void writeSvgMultiPanel(const std::string& path,
                        const Scenario& s,
                        const std::vector<RunResult>& runs) {
  // Layout: 2-column grid (cols = min(N, 2)), N/2 rows. Each panel has
  // a multi-line monospace label header (key = value, "=" aligned) sitting
  // tight above the drawing area — no extra centring padding inside the
  // panel so the label-to-content gap stays small.
  const int n_panels = static_cast<int>(runs.size());
  if (n_panels == 0) return;
  const int n_cols = std::min(n_panels, 2);
  const int n_rows = (n_panels + n_cols - 1) / n_cols;

  // Per-panel geometry (px). Drawing area sized so its diagonal is
  // ~5x the label-block height — text reads comfortably alongside the
  // figure without dominating it.
  constexpr double kCellW   = 600.0;   // panel column width
  constexpr double kDrawH   = 400.0;   // drawing area height per panel
  constexpr double kLabelH  = 132.0;   // 6 lines * 16 px line-height + cushion
  constexpr double kMargin  =  24.0;   // outer + inter-panel margin
  constexpr double kTitleH  =  36.0;   // top scenario-title strip

  const double cell_h = kLabelH + kDrawH;
  const double W = n_cols * kCellW + (n_cols + 1) * kMargin;
  const double H = kTitleH + n_rows * cell_h + (n_rows + 1) * kMargin;

  // Aggregate all OK runs' offsets into the bbox so every panel fits.
  std::vector<SPoint2> agg_offset;
  for (const auto& r : runs) {
    if (!r.ok) continue;
    agg_offset.insert(agg_offset.end(),
                      r.plan.offset_points.begin(),
                      r.plan.offset_points.end());
  }
  const Bbox bb = computeBbox(s.base, agg_offset);
  const double dx = bb.w(), dy = bb.h();
  const double scale = std::min(kCellW / dx, kDrawH / dy) * 0.92;

  // Per-panel (col, row) → drawing-area top-left (px, py).
  auto panelOrigin = [&](int idx) {
    const int col = idx % n_cols;
    const int row = idx / n_cols;
    const double px = kMargin + col * (kCellW + kMargin);
    const double py = kTitleH + kMargin
                    + row * (cell_h + kMargin)
                    + kLabelH;
    return std::pair<double, double>(px, py);
  };

  auto xform = [&](int idx, const SPoint2& p) {
    const auto [px, py] = panelOrigin(idx);
    const double tx = px - scale * bb.xmin + 0.5 * (kCellW - scale * dx);
    const double ty = py + scale * bb.ymax + 0.5 * (kDrawH - scale * dy);
    return SPoint2(scale * p.x() + tx, -scale * p.y() + ty);
  };

  std::ofstream f(path);
  if (!f) {
    std::fprintf(stderr, "WARN: cannot open %s for writing\n",
                 path.c_str());
    return;
  }
  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  f << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
    << W << " " << H << "\" width=\"" << W << "\" height=\"" << H
    << "\">\n";
  f << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";

  // Scenario title bar (single line at the top of the SVG).
  {
    std::ostringstream title;
    title << "scenario = " << s.name
          << "  |  N = "          << s.base.size()
          << "  closed = "        << (s.closed ? "true" : "false")
          << "  side = "          << s.side
          << "  dist = "          << s.dist
          << "  pretrim_α = "     << s.pretrim_alpha;
    f << "<text x=\"" << kMargin << "\" y=\"26\" font-family=\"monospace\" "
         "font-size=\"20\" fill=\"#111\">"
      << title.str() << "</text>\n";
  }

  for (int idx = 0; idx < n_panels; ++idx) {
    const auto& r = runs[idx];
    const auto [px, py] = panelOrigin(idx);
    // Label block top-left = same x as drawing area, top of cell.
    const double lx = px;
    const double ly = py - kLabelH + 14;        // 14px = first baseline

    // Multi-line label, monospace, "key = value" with key padded to 7
    // chars so "=" aligns vertically across rows.
    constexpr int kKeyW = 7;
    auto padKey = [&](const std::string& k) {
      std::string out = k;
      if (static_cast<int>(out.size()) < kKeyW)
        out.append(kKeyW - out.size(), ' ');
      return out;
    };
    auto emit_line = [&](std::ostream& out,
                         const std::string& key, const std::string& val) {
      out << "<tspan x=\"" << lx << "\" dy=\"20\">"
          << padKey(key) << "= " << val << "</tspan>";
    };

    f << "<text x=\"" << lx << "\" y=\"" << ly
      << "\" font-family=\"monospace\" font-size=\"20\" fill=\"#222\""
         " xml:space=\"preserve\">";
    f << "<tspan x=\"" << lx << "\" dy=\"0\">"
      << padKey("label") << "= " << r.label << "</tspan>";
    emit_line(f, "ok",    r.ok ? "Y" : "N");
    emit_line(f, "M",     std::to_string(r.offset_count));
    emit_line(f, "pairs", std::to_string(r.pair_count));
    emit_line(f, "drops", r.dropped_ranges.empty() ? "—" : r.dropped_ranges);
    if (r.pretrimmed) {
      std::ostringstream pr;
      pr << "[" << r.pretrim_run.lo << ".." << r.pretrim_run.hi << "]";
      emit_line(f, "trim",  pr.str());
    }
    f << "</text>\n";

    // Light cell border so panel boundaries are visible.
    f << "<rect x=\"" << px << "\" y=\"" << py
      << "\" width=\""  << kCellW << "\" height=\"" << kDrawH
      << "\" fill=\"none\" stroke=\"#ccc\" stroke-width=\"0.5\"/>\n";

    if (!r.ok) continue;

    // Pair lines (gray).
    f << "<g stroke=\"#888\" stroke-width=\"0.6\" opacity=\"0.7\">\n";
    const int N = static_cast<int>(s.base.size());
    const int M = static_cast<int>(r.plan.offset_points.size());
    for (const auto& pr : r.plan.id_pairs) {
      if (pr.base < 0 || pr.base >= N) continue;
      if (pr.offset < 0 || pr.offset >= M) continue;
      const auto a = xform(idx, s.base[pr.base]);
      const auto b = xform(idx, r.plan.offset_points[pr.offset]);
      f << "<line x1=\"" << a.x() << "\" y1=\"" << a.y() << "\" "
           "x2=\"" << b.x() << "\" y2=\"" << b.y() << "\"/>\n";
    }
    f << "</g>\n";

    // Trimmed input polyline (dashed magenta, pre-trim panels only).
    if (r.pretrimmed && !r.trimmed_input.empty()) {
      f << "<polyline fill=\"none\" stroke=\"#c0c\" stroke-width=\"1.0\" "
           "stroke-dasharray=\"4,3\" points=\"";
      for (const auto& p : r.trimmed_input) {
        const auto q = xform(idx, p);
        f << q.x() << "," << q.y() << " ";
      }
      f << "\"/>\n";
    }

    // Base polyline (blue).
    f << "<polyline fill=\"none\" stroke=\"#1f77b4\" stroke-width=\"1.4\" "
         "points=\"";
    for (const auto& p : s.base) {
      const auto q = xform(idx, p);
      f << q.x() << "," << q.y() << " ";
    }
    if (s.closed && !s.base.empty()) {
      const auto q = xform(idx, s.base.front());
      f << q.x() << "," << q.y();
    }
    f << "\"/>\n";

    // Offset polyline (orange).
    f << "<polyline fill=\"none\" stroke=\"#ff7f0e\" stroke-width=\"1.4\" "
         "points=\"";
    for (const auto& p : r.plan.offset_points) {
      const auto q = xform(idx, p);
      f << q.x() << "," << q.y() << " ";
    }
    if (s.closed && !r.plan.offset_points.empty()) {
      const auto q = xform(idx, r.plan.offset_points.front());
      f << q.x() << "," << q.y();
    }
    f << "\"/>\n";

    // Base vertices (blue dots; labelled on first panel only).
    for (int k = 0; k < N; ++k) {
      const auto q = xform(idx, s.base[k]);
      f << "<circle cx=\"" << q.x() << "\" cy=\"" << q.y()
        << "\" r=\"1.4\" fill=\"#1f77b4\" stroke=\"#063e6b\" "
           "stroke-width=\"0.4\"/>\n";
      if (idx == 0) {
        f << "<text x=\"" << (q.x() + 3) << "\" y=\"" << (q.y() - 3)
          << "\" font-family=\"monospace\" font-size=\"10\" "
             "fill=\"#063e6b\">" << k << "</text>\n";
      }
    }
    // Offset vertices (orange dots).
    for (int k = 0; k < M; ++k) {
      const auto q = xform(idx, r.plan.offset_points[k]);
      f << "<circle cx=\"" << q.x() << "\" cy=\"" << q.y()
        << "\" r=\"1.4\" fill=\"#ff7f0e\" stroke=\"#7a3d05\" "
           "stroke-width=\"0.4\"/>\n";
    }

    // Dropped base indices (red rings).
    for (std::size_t rg = 0; rg < r.plan.dropped_base_ranges_lo.size(); ++rg) {
      for (int b = r.plan.dropped_base_ranges_lo[rg];
           b <= r.plan.dropped_base_ranges_hi[rg]; ++b) {
        if (b < 0 || b >= N) continue;
        const auto q = xform(idx, s.base[b]);
        f << "<circle cx=\"" << q.x() << "\" cy=\"" << q.y()
          << "\" r=\"3\" fill=\"none\" stroke=\"#d62728\" "
             "stroke-width=\"1\"/>\n";
      }
    }
  }

  f << "</svg>\n";
}

// ---------------------------------------------------------------------------
// Per-scenario summary line + CSV row.
// ---------------------------------------------------------------------------

void printHeader() {
  std::printf("%-26s %-16s %3s %3s %3s %4s %4s %s\n",
              "scenario", "matcher", "N", "M", "pa", "ok", "drp", "ranges");
}

void printRow(const Scenario& s, const RunResult& r) {
  std::printf("%-26s %-16s %3d %3d %3d %4s %4d %s\n",
              s.name.c_str(),
              r.label.c_str(),
              static_cast<int>(s.base.size()),
              static_cast<int>(r.offset_count),
              static_cast<int>(r.pair_count),
              r.ok ? "Y" : "N",
              static_cast<int>(r.dropped_count),
              r.dropped_ranges.c_str());
}

void appendCsvRow(std::ofstream& csv, const Scenario& s, const RunResult& r) {
  csv << s.name << "," << r.label << ","
      << s.base.size() << "," << r.offset_count << "," << r.pair_count
      << "," << (r.ok ? 1 : 0) << "," << r.dropped_count
      << "," << "\"" << r.dropped_ranges << "\""
      << "," << (r.pretrimmed ? 1 : 0)
      << "," << s.dist << "," << s.pretrim_alpha << "\n";
}

// ---------------------------------------------------------------------------
// Main.
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  (void)argc; (void)argv;

  const std::string out_dir = T0_OUT_DIR;
  // Make sure the output directory exists — CMake `file(MAKE_DIRECTORY)`
  // only runs at configure time, so an incremental rebuild after a
  // build_msvc/ wipe may leave it missing.  `ofstream` silently
  // fails-to-open into a nonexistent dir, so we mkdir explicitly +
  // fail loudly if even that doesn't work.
  {
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
      std::fprintf(stderr, "FATAL: cannot create out dir %s: %s\n",
                   out_dir.c_str(), ec.message().c_str());
      return 2;
    }
  }

  std::vector<Scenario> scenarios;
  scenarios.push_back(makeClosedSquare());
  scenarios.push_back(makeOpenStraight());
  scenarios.push_back(makeOpenVCuspShallow());
  scenarios.push_back(makeOpenVCuspSharp(/*alpha*/ 1.2));
  scenarios.push_back(makeOpenTECuspLike(/*dist*/ 0.02, /*alpha*/ 1.2));
  scenarios.push_back(makeClosedThinTE());
  scenarios.push_back(makeAirfoilMh104Closed(/*dist*/ 0.02,
                                             /*alpha*/ 1.2));
  scenarios.push_back(makeAirfoilMh104TEOpen(/*dist*/ 0.02,
                                             /*alpha*/ 1.2));

  const std::string csv_path = out_dir + "/summary.csv";
  std::ofstream csv(csv_path);
  if (!csv) {
    std::fprintf(stderr, "FATAL: cannot open %s for writing\n",
                 csv_path.c_str());
    return 2;
  }
  csv << "scenario,matcher,N,M,pairs,ok,n_drops,drops,pretrimmed,dist,"
         "pretrim_alpha\n";

  printHeader();

  for (const auto& s : scenarios) {
    // ---- matcher-compare runs (all use Miter join, default) ----
    std::vector<RunResult> runs;
    runs.push_back(runOnce(s, PairingAlgo::SegmentProjection, false));
    runs.push_back(runOnce(s, PairingAlgo::Nearest,           false));
    runs.push_back(runOnce(s, PairingAlgo::DP,                false));
    // Pre-trim variant only for open base with α > 0.
    if (!s.closed && s.pretrim_alpha > 0.0) {
      runs.push_back(runOnce(s, PairingAlgo::SegmentProjection, true));
    }
    for (const auto& r : runs) {
      printRow(s, r);
      appendCsvRow(csv, s, r);
    }

    // ---- join-compare runs (fixed segment matcher, sweep 4 JoinTypes) ----
    std::vector<RunResult> join_runs;
    for (auto jt : {JoinTypeChoice::Miter, JoinTypeChoice::Square,
                    JoinTypeChoice::Bevel, JoinTypeChoice::Round}) {
      auto r = runOnce(s, PairingAlgo::SegmentProjection, /*pretrim*/ false,
                       jt);
      // For the join-compare SVG, override the label to just the join
      // name (matcher is implicit = segment for all 4 panels).
      r.label = joinName(jt);
      join_runs.push_back(std::move(r));
    }
    for (const auto& r : join_runs) {
      printRow(s, r);
      appendCsvRow(csv, s, r);
    }

    // ---- raw control group ----
    // Same 3 matchers as the matcher-compare runs, but `resample_open =
    // false` so the open-input `extractOpenRuns` step-5 resample is
    // skipped. Tests the hypothesis "vertex-match works directly on raw
    // Clipper2 corners, no foot-of-perpendicular resampling needed".
    // For closed inputs the flag has no effect (closed never resamples)
    // — `+raw` rows on closed scenarios duplicate the matcher-compare
    // rows and serve as no-op sanity.
    std::vector<RunResult> raw_runs;
    raw_runs.push_back(runOnce(s, PairingAlgo::SegmentProjection,
                               /*pretrim*/ false,
                               JoinTypeChoice::Miter, /*ml*/ 2.0,
                               /*resample_open*/ false));
    raw_runs.push_back(runOnce(s, PairingAlgo::Nearest,
                               false, JoinTypeChoice::Miter, 2.0, false));
    raw_runs.push_back(runOnce(s, PairingAlgo::DP,
                               false, JoinTypeChoice::Miter, 2.0, false));
    if (!s.closed && s.pretrim_alpha > 0.0) {
      raw_runs.push_back(runOnce(s, PairingAlgo::SegmentProjection,
                                 /*pretrim*/ true,
                                 JoinTypeChoice::Miter, 2.0, false));
    }
    for (const auto& r : raw_runs) {
      printRow(s, r);
      appendCsvRow(csv, s, r);
    }
    std::printf("\n");

    writeSvgMultiPanel(out_dir + "/" + s.name + ".svg",       s, runs);
    writeSvgMultiPanel(out_dir + "/" + s.name + "_joins.svg", s, join_runs);
    writeSvgMultiPanel(out_dir + "/" + s.name + "_raw.svg",   s, raw_runs);

    // ---- Paired resample-vs-raw compare SVG ----
    // Interleave default + raw runs in matcher-paired order so adjacent
    // panels show the same matcher with/without resample. For open
    // scenarios this is the primary A/B view for the control group.
    // For closed scenarios the pairs print identical (no resample step
    // on closed inputs) — still useful as a sanity check.
    std::vector<RunResult> pair_runs;
    // Pair count = matcher rows in `runs` (3 base matchers, +1 if
    // pretrim was added). We take the same prefix from `raw_runs`.
    const std::size_t n_pairs = std::min(runs.size(), raw_runs.size());
    for (std::size_t k = 0; k < n_pairs; ++k) {
      pair_runs.push_back(runs[k]);
      pair_runs.push_back(raw_runs[k]);
    }
    writeSvgMultiPanel(out_dir + "/" + s.name + "_compare.svg",
                       s, pair_runs);
  }

  std::printf("done. SVG + summary.csv written to %s\n", out_dir.c_str());
  return 0;
}
