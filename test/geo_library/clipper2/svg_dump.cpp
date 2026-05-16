#include "svg_dump.hpp"

#include "geom_assertions.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

#ifndef CLIPPER2_AIRFOIL_OUT_DIR
#define CLIPPER2_AIRFOIL_OUT_DIR "."
#endif

namespace clipper2_airfoil {

namespace {

void ensureDir(const std::string& path) {
  MKDIR(path.c_str());  // ignore failure; existing dir is fine
}

std::string sanitizeTag(const std::string& tag) {
  std::string s;
  s.reserve(tag.size());
  for (char c : tag) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_' || c == '-') {
      s.push_back(c);
    } else {
      s.push_back('_');
    }
  }
  return s;
}

void writePolyAttr(std::ostringstream& svg,
                   const Clipper2Lib::PathD& path,
                   double sx, double sy, double ox, double oy) {
  svg << "points=\"";
  for (std::size_t i = 0; i < path.size(); ++i) {
    if (i > 0) svg << ' ';
    svg << (path[i].x * sx + ox) << ',' << (path[i].y * sy + oy);
  }
  svg << "\"";
}

}  // namespace

std::string dumpOffsetSvg(const std::string& tag,
                          const Clipper2Lib::PathD&  base,
                          const Clipper2Lib::PathsD& solution) {
  ensureDir(CLIPPER2_AIRFOIL_OUT_DIR);

  const std::string out_path =
      std::string(CLIPPER2_AIRFOIL_OUT_DIR) + "/" + sanitizeTag(tag) + ".svg";

  // Union bbox of base + solution.
  Bbox bb = bboxOf(base);
  for (const auto& p : solution) {
    Bbox s = bboxOf(p);
    bb.xmin = std::min(bb.xmin, s.xmin);
    bb.ymin = std::min(bb.ymin, s.ymin);
    bb.xmax = std::max(bb.xmax, s.xmax);
    bb.ymax = std::max(bb.ymax, s.ymax);
  }

  // Uniform scale so the airfoil's true aspect ratio is preserved —
  // critical for visual inspection of offset distance, which would
  // otherwise look anisotropic for a typical chord:thickness ≈ 8:1
  // contour. The canvas height is sized from the data so airfoils
  // don't get a huge band of empty space above and below; canvas
  // width is fixed at 1000 px.
  const double margin   = 20.0;
  const double canvas_w = 1000.0;
  const double bb_w     = std::max(bb.width(),  1e-12);
  const double bb_h     = std::max(bb.height(), 1e-12);
  const double s        = (canvas_w - 2 * margin) / bb_w;  // px per unit
  // Canvas height: enough to fit bb_h at the uniform scale, but never
  // less than 200 px (so very thin airfoils still get a legible band).
  const double canvas_h = std::max(200.0, bb_h * s + 2 * margin);
  // World -> SVG transform. y is flipped so +y points up in the render.
  // Center the bbox in the canvas so the airfoil sits visually balanced
  // even when the canvas is taller than strictly needed.
  const double bb_center_x = 0.5 * (bb.xmin + bb.xmax);
  const double bb_center_y = 0.5 * (bb.ymin + bb.ymax);
  const double sx =  s;
  const double sy = -s;
  const double ox = 0.5 * canvas_w - bb_center_x * sx;
  const double oy = 0.5 * canvas_h - bb_center_y * sy;

  std::ostringstream svg;
  svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
      << "width=\"" << canvas_w << "\" height=\"" << canvas_h << "\" "
      << "viewBox=\"0 0 " << canvas_w << ' ' << canvas_h << "\">\n";

  svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
  svg << "<text x=\"10\" y=\"15\" font-family=\"monospace\" "
      << "font-size=\"12\" fill=\"black\">" << tag << "</text>\n";

  // Base contour: thin blue polyline.
  svg << "<polygon ";
  writePolyAttr(svg, base, sx, sy, ox, oy);
  svg << " fill=\"none\" stroke=\"#1976D2\" stroke-width=\"1\"/>\n";

  // Solution: red filled with low alpha.
  for (std::size_t i = 0; i < solution.size(); ++i) {
    svg << "<polygon ";
    writePolyAttr(svg, solution[i], sx, sy, ox, oy);
    svg << " fill=\"#D32F2F\" fill-opacity=\"0.15\" "
        << "stroke=\"#D32F2F\" stroke-width=\"1\"/>\n";
  }

  // Legend.
  svg << "<text x=\"10\" y=\"30\" font-family=\"monospace\" "
      << "font-size=\"11\" fill=\"#1976D2\">--- base contour</text>\n";
  svg << "<text x=\"10\" y=\"45\" font-family=\"monospace\" "
      << "font-size=\"11\" fill=\"#D32F2F\">--- offset solution "
      << "(" << solution.size() << " polygons)</text>\n";

  svg << "</svg>\n";

  std::ofstream f(out_path);
  if (f) {
    f << svg.str();
  }
  return out_path;
}

}  // namespace clipper2_airfoil
