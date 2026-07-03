// t0_dcel.cpp
// ---------------------------------------------------------------------------
// Standalone harness exercising PDCEL topology primitives — focused on
// PDCEL::splitFaceByPolyline (and the repeated-split "band subdivision"
// pattern used by buildLayeredOffsetAreas / splitLayerBandIntoCells).
//
// For every case it
//   1. builds a DCEL by hand,
//   2. runs the split(s),
//   3. validates the result (dcel.validate()),
//   4. dumps the DCEL state to  <out>/<case>.dcel_dump.txt,
//   5. renders an SVG to        <out>/<case>.svg.
//
// 2D convention (matches production cross-sections and test_dcel): geometry
// lives in the y-z plane, x == 0. A vertex at 2D (a,b) is built as
// PDCELVertex(0, a, b); SVG uses (y, z).
//
// Coupling: mirrors the test_dcel target's non-test source list (PDCEL pulls
// the geo + utilities closure, which is header-coupled to cs/Material). Same
// closure the unit DCEL tests already accept. No Gmsh required.
// ---------------------------------------------------------------------------

#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"

#include <cmath>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Global variable definitions (normally in src/main.cpp). Exactly once.
// ---------------------------------------------------------------------------
bool         scientific_format = false;
PConfig      config;
RuntimeState runtime;

// ---------------------------------------------------------------------------
// Stubs for PModel.cpp-resident free functions referenced by the cs sources
// in our link closure (CrossSection / PBuildSegmentAreas). PModel.cpp itself
// is gmsh-coupled and intentionally excluded; this harness never plots.
// ---------------------------------------------------------------------------
class PModel;
void plotGeoSnapshotImpl(PModel *, const std::string &, bool) {}

// Register a stderr logger so validate()/split warnings are visible.
#include "plog.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel
namespace {
struct LoggerSetup {
  LoggerSetup() {
    if (!spdlog::get("prevabs")) {
      auto sink   = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
      logger->set_level(spdlog::level::warn);
      spdlog::register_logger(logger);
    }
  }
} _logger_setup;
}  // namespace

#ifndef T0_DCEL_OUT_DIR
#define T0_DCEL_OUT_DIR "."
#endif

namespace {

using P2 = std::pair<double, double>;  // (y, z)

struct Seg2 {
  double y1, z1, y2, z2;
};

int g_failures = 0;

// ---- DCEL construction helpers -------------------------------------------

// Build a single bounded face from a CCW (y,z) ring and wire its loop.
// Returns the face; `out_verts` receives the registered boundary vertices in
// input order (so callers can reuse them as split endpoints).
PDCELFace *buildPolygonFace(PDCEL &dcel, const std::vector<P2> &ring,
                            std::vector<PDCELVertex *> *out_verts = nullptr) {
  std::vector<PDCELVertex *> vs;
  vs.reserve(ring.size());
  for (const P2 &p : ring) {
    vs.push_back(new PDCELVertex(0.0, p.first, p.second));
  }
  std::list<PDCELVertex *> vloop(vs.begin(), vs.end());
  vloop.push_back(vs.front());  // close the ring

  PDCELFace *f = dcel.addFace(vloop);
  dcel.addHalfEdgeLoop(f->outer());
  if (out_verts) *out_verts = vs;
  return f;
}

// Does `face`'s outer boundary pass through vertex `v`?
bool faceHasVertex(PDCELFace *face, PDCELVertex *v) {
  if (face == nullptr || face->outer() == nullptr || v == nullptr) return false;
  PDCELHalfEdge *start = face->outer();
  PDCELHalfEdge *he = start;
  int cap = 0;
  do {
    if (he->source() == v) return true;
    he = he->next();
  } while (he != nullptr && he != start && ++cap < 100000);
  return false;
}

// ---- SVG rendering --------------------------------------------------------

std::vector<P2> faceLoopPoints(PDCELFace *f) {
  std::vector<P2> pts;
  if (f == nullptr || f->outer() == nullptr) return pts;
  PDCELHalfEdge *start = f->outer();
  PDCELHalfEdge *he = start;
  int cap = 0;
  do {
    PDCELVertex *s = he->source();
    pts.emplace_back(s->y(), s->z());
    he = he->next();
  } while (he != nullptr && he != start && ++cap < 100000);
  return pts;
}

const char *kPalette[] = {"#9ecae1", "#a1d99b", "#fdae6b", "#bcbddc",
                          "#fdd0a2", "#c7e9c0", "#dadaeb", "#d9d9d9"};

void writeSvg(const std::string &path, PDCEL &dcel,
              const std::vector<Seg2> &highlights, const std::string &title) {
  // Collect bounded faces and bounding box (ignore the unbounded background).
  std::vector<std::vector<P2>> face_polys;
  double miny = 1e30, minz = 1e30, maxy = -1e30, maxz = -1e30;
  for (PDCELFace *f : dcel.faces()) {
    if (f == nullptr || !f->isBounded()) continue;
    std::vector<P2> poly = faceLoopPoints(f);
    if (poly.size() < 3) continue;
    bool finite = true;
    for (const P2 &p : poly) {
      if (std::abs(p.first) > 1e6 || std::abs(p.second) > 1e6) finite = false;
    }
    if (!finite) continue;  // background / huge face
    for (const P2 &p : poly) {
      miny = std::min(miny, p.first);  maxy = std::max(maxy, p.first);
      minz = std::min(minz, p.second); maxz = std::max(maxz, p.second);
    }
    face_polys.push_back(std::move(poly));
  }
  if (face_polys.empty()) { miny = minz = 0; maxy = maxz = 1; }

  const double pad = 0.12 * std::max(maxy - miny, maxz - minz) + 0.2;
  miny -= pad; minz -= pad; maxy += pad; maxz += pad;
  const double w = maxy - miny, h = maxz - minz;
  const double scale = 600.0 / std::max(w, h);
  const double W = w * scale, H = h * scale;

  // map (y,z) -> svg pixel; flip z so +z points up
  auto X = [&](double y) { return (y - miny) * scale; };
  auto Y = [&](double z) { return H - (z - minz) * scale; };

  std::ofstream o(path);
  if (!o) { ++g_failures; return; }
  o << "<svg xmlns='http://www.w3.org/2000/svg' width='" << (W + 20)
    << "' height='" << (H + 40) << "' font-family='sans-serif'>\n";
  o << "<rect width='100%' height='100%' fill='white'/>\n";
  o << "<text x='10' y='20' font-size='14' fill='#333'>" << title
    << "</text>\n";
  o << "<g transform='translate(10,30)'>\n";

  // filled faces
  for (std::size_t i = 0; i < face_polys.size(); ++i) {
    o << "<polygon points='";
    for (const P2 &p : face_polys[i]) o << X(p.first) << "," << Y(p.second) << " ";
    o << "' fill='" << kPalette[i % 8]
      << "' fill-opacity='0.55' stroke='#444' stroke-width='1'/>\n";
  }

  // all active half-edges as thin edges (deduped by drawing source->target)
  for (PDCELHalfEdge *he : dcel.halfedges()) {
    if (he == nullptr || he->source() == nullptr || he->twin() == nullptr) continue;
    PDCELVertex *a = he->source();
    PDCELVertex *b = he->target();
    if (a == nullptr || b == nullptr) continue;
    if (std::abs(a->y()) > 1e6 || std::abs(b->y()) > 1e6) continue;
    o << "<line x1='" << X(a->y()) << "' y1='" << Y(a->z()) << "' x2='"
      << X(b->y()) << "' y2='" << Y(b->z())
      << "' stroke='#888' stroke-width='0.6'/>\n";
  }

  // highlighted split path
  for (const Seg2 &s : highlights) {
    o << "<line x1='" << X(s.y1) << "' y1='" << Y(s.z1) << "' x2='" << X(s.y2)
      << "' y2='" << Y(s.z2)
      << "' stroke='#e6194b' stroke-width='2.4'/>\n";
  }

  // vertices + labels
  for (PDCELVertex *v : dcel.vertices()) {
    if (v == nullptr || std::abs(v->y()) > 1e6) continue;
    o << "<circle cx='" << X(v->y()) << "' cy='" << Y(v->z())
      << "' r='2.6' fill='#222'/>\n";
    o << "<text x='" << (X(v->y()) + 4) << "' y='" << (Y(v->z()) - 4)
      << "' font-size='10' fill='#c00'>" << v->label() << "</text>\n";
  }

  o << "</g>\n</svg>\n";
}

// ---- per-case reporting ---------------------------------------------------

std::string outPath(const std::string &name, const char *ext) {
  return std::string(T0_DCEL_OUT_DIR) + "/" + name + ext;
}

void emit(const std::string &name, PDCEL &dcel,
          const std::vector<Seg2> &highlights, const std::string &title) {
  dcel.dumpToFile(outPath(name, ".dcel_dump.txt"));
  writeSvg(outPath(name, ".svg"), dcel, highlights, title);
}

void report(const std::string &name, bool ok, const std::string &detail) {
  if (!ok) ++g_failures;
  std::printf("[%s] %-26s  %s\n", ok ? "PASS" : "FAIL", name.c_str(),
              detail.c_str());
}

Seg2 segOf(const P2 &a, const P2 &b) { return {a.first, a.second, b.first, b.second}; }

using Ring = std::vector<P2>;

bool nearP2(const P2 &a, const P2 &b) {
  return std::abs(a.first - b.first) < 1e-6 && std::abs(a.second - b.second) < 1e-6;
}

// Does face's boundary loop contain exactly the expected vertex set (order /
// rotation / orientation independent)?
bool faceHasVertexSet(PDCELFace *f, const Ring &expected) {
  std::vector<P2> got = faceLoopPoints(f);
  if (got.size() != expected.size()) return false;
  for (const P2 &e : expected) {
    bool found = false;
    for (const P2 &g : got) if (nearP2(e, g)) { found = true; break; }
    if (!found) return false;
  }
  return true;
}

// Verify the set of bounded faces matches `expected` exactly (each expected
// ring matched by exactly one bounded face, and no extras). Counts into
// g_failures and prints a per-step line.
bool checkFaces(PDCEL &dcel, const std::vector<Ring> &expected,
                const char *step) {
  std::vector<PDCELFace *> bounded;
  for (PDCELFace *f : dcel.faces()) if (f && f->isBounded()) bounded.push_back(f);

  bool ok = bounded.size() == expected.size();
  int matched = 0;
  for (const Ring &e : expected) {
    int hits = 0;
    for (PDCELFace *f : bounded) if (faceHasVertexSet(f, e)) ++hits;
    if (hits == 1) ++matched; else ok = false;
  }
  ok = ok && matched == (int)expected.size();
  if (!ok) ++g_failures;
  std::printf("   [%s] %-8s expected %zu faces, got %zu, matched %d/%zu\n",
              ok ? "OK" : "MISMATCH", step, expected.size(), bounded.size(),
              matched, expected.size());
  return ok;
}

// Subdivide a band face into cells by connectors, mirroring the production
// splitLayerBandIntoCells "tail-endpoint" remaining-face strategy.
// connectors: interior (inner,outer) vertex pairs in order; tail: last pair.
std::vector<PDCELFace *> subdivideBand(
    PDCEL &dcel, PDCELFace *band,
    const std::vector<std::pair<PDCELVertex *, PDCELVertex *>> &connectors,
    PDCELVertex *tail_inner, PDCELVertex *tail_outer, bool &ok,
    const char *verbose_tag = nullptr) {
  std::vector<PDCELFace *> cells;
  PDCELFace *remaining = band;
  ok = true;
  for (std::size_t i = 0; i < connectors.size(); ++i) {
    const auto &c = connectors[i];
    std::vector<PDCELVertex *> conn = {c.first, c.second};
    std::list<PDCELFace *> faces = dcel.splitFaceByPolyline(remaining, conn);
    if (faces.size() != 2) {
      if (verbose_tag)
        std::printf("     %s connector %zu (%s->%s): split FAILED (faces=%zu)\n",
                    verbose_tag, i + 1, c.first->label().c_str(),
                    c.second->label().c_str(), faces.size());
      ok = false; return cells;
    }
    PDCELFace *f0 = faces.front(), *f1 = faces.back();
    bool f0_tail = faceHasVertex(f0, tail_inner) && faceHasVertex(f0, tail_outer);
    bool f1_tail = faceHasVertex(f1, tail_inner) && faceHasVertex(f1, tail_outer);
    if (verbose_tag)
      std::printf("     %s connector %zu (%s->%s): f0_has_tail=%d f1_has_tail=%d%s\n",
                  verbose_tag, i + 1, c.first->label().c_str(),
                  c.second->label().c_str(), f0_tail, f1_tail,
                  (f0_tail == f1_tail) ? "  <-- AMBIGUOUS" : "");
    if (f0_tail == f1_tail) { ok = false; return cells; }  // ambiguous
    cells.push_back(f0_tail ? f1 : f0);
    remaining = f0_tail ? f0 : f1;
  }
  cells.push_back(remaining);
  return cells;
}

// =====================  CASES  =============================================

// Tier 1: single straight chord (equivalent to splitFace).
void case_straight_chord() {
  const std::string name = "case1_straight_chord";
  PDCEL dcel; dcel.initialize();
  buildPolygonFace(dcel, {{0, 0}, {6, 0}, {6, 4}, {0, 4}});
  PDCELFace *f = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) f = fc;

  std::vector<PDCELVertex *> path = {new PDCELVertex(0.0, 3, 0),
                                     new PDCELVertex(0.0, 3, 4)};
  auto faces = dcel.splitFaceByPolyline(f, path);
  bool ok = faces.size() == 2 && dcel.validate();
  emit(name, dcel, {segOf({3, 0}, {3, 4})}, "case1: straight chord -> 2 faces");
  report(name, ok, "faces=" + std::to_string(faces.size()));
}

// Tier 2: one interior bend (3-vertex path).
void case_bent_path() {
  const std::string name = "case2_bent_path";
  PDCEL dcel; dcel.initialize();
  buildPolygonFace(dcel, {{0, 0}, {6, 0}, {6, 4}, {0, 4}});
  PDCELFace *f = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) f = fc;

  std::vector<PDCELVertex *> path = {new PDCELVertex(0.0, 2, 0),
                                     new PDCELVertex(0.0, 3, 2),
                                     new PDCELVertex(0.0, 4, 4)};
  auto faces = dcel.splitFaceByPolyline(f, path);
  bool ok = faces.size() == 2 && dcel.validate();
  emit(name, dcel, {segOf({2, 0}, {3, 2}), segOf({3, 2}, {4, 4})},
       "case2: bent path -> 2 faces");
  report(name, ok, "faces=" + std::to_string(faces.size()));
}

// Tier 3: multi-bend zigzag interior polyline (5-vertex path).
void case_zigzag_multibend() {
  const std::string name = "case3_zigzag_multibend";
  PDCEL dcel; dcel.initialize();
  buildPolygonFace(dcel, {{0, 0}, {10, 0}, {10, 5}, {0, 5}});
  PDCELFace *f = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) f = fc;

  std::vector<P2> pp = {{1, 0}, {3, 2}, {5, 1.2}, {7, 3.4}, {9, 5}};
  std::vector<PDCELVertex *> path;
  for (const P2 &p : pp) path.push_back(new PDCELVertex(0.0, p.first, p.second));
  auto faces = dcel.splitFaceByPolyline(f, path);
  std::vector<Seg2> hl;
  for (std::size_t i = 1; i < pp.size(); ++i) hl.push_back(segOf(pp[i - 1], pp[i]));
  bool ok = faces.size() == 2 && dcel.validate();
  emit(name, dcel, hl, "case3: zigzag multi-bend -> 2 faces");
  report(name, ok, "faces=" + std::to_string(faces.size()));
}

// Tier 4: layered-offset band subdivided into 3 cells (healthy arch).
void case_layer_band() {
  const std::string name = "case4_layer_band";
  PDCEL dcel; dcel.initialize();
  // ring: I0 I1 I2 I3 O3 O2 O1 O0  (inner bottom, outer top)
  std::vector<PDCELVertex *> vs;
  buildPolygonFace(dcel,
                   {{0, 0}, {2, 1}, {4, 1}, {6, 0},        // inner I0..I3
                    {6, 1.2}, {4, 2.2}, {2, 2.2}, {0, 1.2}},  // outer O3..O0
                   &vs);
  PDCELFace *band = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) band = fc;

  PDCELVertex *I1 = vs[1], *I2 = vs[2], *I3 = vs[3];
  PDCELVertex *O3 = vs[4], *O2 = vs[5], *O1 = vs[6];
  bool ok = false;
  auto cells = subdivideBand(dcel, band, {{I1, O1}, {I2, O2}}, I3, O3, ok);
  ok = ok && cells.size() == 3 && dcel.validate();
  emit(name, dcel, {segOf({2, 1}, {2, 2.2}), segOf({4, 1}, {4, 2.2})},
       "case4: layer band -> 3 cells");
  report(name, ok, "cells=" + std::to_string(cells.size()));
}

// Tier 5: path endpoints coincide with existing boundary corners.
void case_endpoints_on_vertices() {
  const std::string name = "case5_endpoints_on_vertices";
  PDCEL dcel; dcel.initialize();
  std::vector<PDCELVertex *> vs;
  buildPolygonFace(dcel, {{0, 0}, {6, 0}, {6, 4}, {0, 4}}, &vs);
  PDCELFace *f = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) f = fc;

  // endpoints are the existing corners vs[0]=(0,0) and vs[2]=(6,4)
  std::vector<PDCELVertex *> path = {vs[0], new PDCELVertex(0.0, 3, 2.5), vs[2]};
  auto faces = dcel.splitFaceByPolyline(f, path);
  bool ok = faces.size() == 2 && dcel.validate();
  emit(name, dcel, {segOf({0, 0}, {3, 2.5}), segOf({3, 2.5}, {6, 4})},
       "case5: endpoints on existing corners -> 2 faces");
  report(name, ok, "faces=" + std::to_string(faces.size()));
}

// Tier 6 (stress): thin/pinched band — degenerate-class geometry from the
// Z concave-corner diagnosis. Reported, not asserted: it may validate or it
// may trip the ambiguous/near-degenerate path. The DCEL dump + SVG document
// whichever happens.
void case_thin_pinch_band() {
  const std::string name = "case6_thin_pinch_band";
  PDCEL dcel; dcel.initialize();
  std::vector<PDCELVertex *> vs;
  buildPolygonFace(dcel,
                   {{0, 0}, {2, 0.90}, {4, 0.90}, {6, 0},
                    {6, 1.0}, {4, 0.95}, {2, 0.95}, {0, 1.0}},
                   &vs);
  PDCELFace *band = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) band = fc;

  PDCELVertex *I1 = vs[1], *I2 = vs[2], *I3 = vs[3];
  PDCELVertex *O3 = vs[4], *O2 = vs[5], *O1 = vs[6];
  bool ok = false;
  auto cells = subdivideBand(dcel, band, {{I1, O1}, {I2, O2}}, I3, O3, ok);
  bool valid = dcel.validate();
  emit(name, dcel, {segOf({2, 0.90}, {2, 0.95}), segOf({4, 0.90}, {4, 0.95})},
       "case6: thin pinch band (stress)");
  // stress case: report outcome, do not count as failure
  std::printf("[STRS] %-26s  cells=%zu valid=%s%s\n", name.c_str(),
              cells.size(), valid ? "Y" : "N",
              ok ? "" : "  (subdivision rejected)");
}

// Tier 7 (spec-driven): the t2_z/z 3-layer layered offset, replayed step by
// step against dev-docs/dcel/test_split_face_z_expected.md. Geometry is the
// clean-miter offset from that spec (base + 3 offset curves at 0.02/0.05/0.07).
// Each step's bounded-face set is checked against the expected face topology.
//
//   shell (state) : v1 v2 v3 v4 | v8 v7 v6 v5
//   step 1        : split shell by curve1 (0.02)  -> layer1 band + rest
//   step 2        : split rest  by curve2 (0.05)  -> layer2 band + layer3 band
//   step 3        : subdivide layer1 band          -> 3 cells (face6,7,8)
//   step 4        : subdivide layer2 band          -> 3 cells (face9,10,11)
//   step 5        : subdivide layer3 band          -> 3 cells (face12,13,14)
void case_z_layered_spec() {
  const std::string name = "case8_z_layered_spec";
  std::printf("-- %s : 3-layer t2_z/z replay vs expected spec --\n", name.c_str());

  // Vertices (y,z) from the expected spec (clean-miter offset).
  const P2 v1{-0.5, 0.5},  v2{0.0, 0.5},   v3{0.0, -0.5},   v4{0.5, -0.5};
  const P2 v5{-0.5, 0.57}, v6{0.07, 0.57}, v7{0.07, -0.43}, v8{0.5, -0.43};
  const P2 v9{-0.5, 0.52}, v10{0.02, 0.52}, v11{0.02, -0.48}, v12{0.5, -0.48};
  const P2 v13{-0.5, 0.55}, v14{0.05, 0.55}, v15{0.05, -0.45}, v16{0.5, -0.45};

  PDCEL dcel; dcel.initialize();

  // shell octagon: base forward (v1..v4), shell backward (v8..v5)
  std::vector<PDCELVertex *> vs;
  buildPolygonFace(dcel, {v1, v2, v3, v4, v8, v7, v6, v5}, &vs);
  PDCELVertex *V1 = vs[0], *V2 = vs[1], *V3 = vs[2], *V4 = vs[3];
  PDCELVertex *V8 = vs[4], *V7 = vs[5], *V6 = vs[6], *V5 = vs[7];
  PDCELFace *shell = nullptr;
  for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) shell = fc;
  checkFaces(dcel, {{v1, v2, v3, v4, v8, v7, v6, v5}}, "state");

  auto mkv = [](const P2 &p) { return new PDCELVertex(0.0, p.first, p.second); };

  // ---- step 1: split shell by curve1 (0.02) -------------------------------
  PDCELVertex *c9 = mkv(v9), *c10 = mkv(v10), *c11 = mkv(v11), *c12 = mkv(v12);
  auto s1 = dcel.splitFaceByPolyline(shell, {c9, c10, c11, c12});
  PDCELFace *layer1 = nullptr, *rest = nullptr;
  if (s1.size() == 2) {
    layer1 = faceHasVertex(s1.front(), V1) ? s1.front() : s1.back();
    rest   = (layer1 == s1.front()) ? s1.back() : s1.front();
  } else { ++g_failures; std::printf("   step1 split failed\n"); }
  checkFaces(dcel, {{v1, v2, v3, v4, v12, v11, v10, v9},
                    {v9, v10, v11, v12, v8, v7, v6, v5}}, "step1");
  emit(name + "_step1", dcel, {segOf(v9, v10), segOf(v10, v11), segOf(v11, v12)},
       "step1: split shell by curve1 -> layer1 + rest");

  // ---- step 2: split rest by curve2 (0.05) --------------------------------
  PDCELVertex *c13 = mkv(v13), *c14 = mkv(v14), *c15 = mkv(v15), *c16 = mkv(v16);
  PDCELFace *layer2 = nullptr, *layer3 = nullptr;
  if (rest) {
    auto s2 = dcel.splitFaceByPolyline(rest, {c13, c14, c15, c16});
    if (s2.size() == 2) {
      layer2 = faceHasVertex(s2.front(), c9) ? s2.front() : s2.back();
      layer3 = (layer2 == s2.front()) ? s2.back() : s2.front();
    } else { ++g_failures; std::printf("   step2 split failed\n"); }
  }
  checkFaces(dcel, {{v1, v2, v3, v4, v12, v11, v10, v9},
                    {v9, v10, v11, v12, v16, v15, v14, v13},
                    {v13, v14, v15, v16, v8, v7, v6, v5}}, "step2");
  emit(name + "_step2", dcel,
       {segOf(v13, v14), segOf(v14, v15), segOf(v15, v16)},
       "step2: split rest by curve2 -> layer2 + layer3");

  // ---- step 3: subdivide layer1 band into 3 cells -------------------------
  bool ok3 = false;
  if (layer1)
    subdivideBand(dcel, layer1, {{V2, c10}, {V3, c11}}, V4, c12, ok3);
  checkFaces(dcel, {{v1, v2, v10, v9}, {v2, v3, v11, v10}, {v3, v4, v12, v11},
                    {v9, v10, v11, v12, v16, v15, v14, v13},
                    {v13, v14, v15, v16, v8, v7, v6, v5}}, "step3");
  emit(name + "_step3", dcel, {segOf(v2, v10), segOf(v3, v11)},
       "step3: subdivide layer1 -> 3 cells");

  // ---- step 4: subdivide layer2 band into 3 cells -------------------------
  bool ok4 = false;
  if (layer2)
    subdivideBand(dcel, layer2, {{c10, c14}, {c11, c15}}, c12, c16, ok4);
  checkFaces(dcel, {{v1, v2, v10, v9}, {v2, v3, v11, v10}, {v3, v4, v12, v11},
                    {v9, v10, v14, v13}, {v10, v11, v15, v14}, {v11, v12, v16, v15},
                    {v13, v14, v15, v16, v8, v7, v6, v5}}, "step4");
  emit(name + "_step4", dcel, {segOf(v10, v14), segOf(v11, v15)},
       "step4: subdivide layer2 -> 3 cells");

  // ---- step 5: subdivide layer3 band into 3 cells -------------------------
  bool ok5 = false;
  if (layer3)
    subdivideBand(dcel, layer3, {{c14, V6}, {c15, V7}}, c16, V8, ok5);
  checkFaces(dcel, {{v1, v2, v10, v9}, {v2, v3, v11, v10}, {v3, v4, v12, v11},
                    {v9, v10, v14, v13}, {v10, v11, v15, v14}, {v11, v12, v16, v15},
                    {v13, v14, v6, v5}, {v14, v15, v7, v6}, {v15, v16, v8, v7}},
             "step5");
  emit(name + "_step5", dcel, {segOf(v14, v6), segOf(v15, v7)},
       "step5: subdivide layer3 -> 3 cells (9 cells total)");

  const bool valid = dcel.validate();
  if (!valid) ++g_failures;
  report(name, valid, std::string("dcel.validate=") + (valid ? "Y" : "N"));
}

// Invalid inputs must return {} and leave the DCEL unchanged.
void case_invalid_inputs() {
  const std::string name = "case7_invalid_inputs";
  bool all_ok = true;

  auto fresh = [](PDCEL &dcel, std::vector<PDCELVertex *> &vs) -> PDCELFace * {
    dcel.initialize();
    buildPolygonFace(dcel, {{0, 0}, {6, 0}, {6, 4}, {0, 4}}, &vs);
    for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) return fc;
    return nullptr;
  };
  auto faceCount = [](PDCEL &dcel) {
    std::size_t n = 0;
    for (PDCELFace *fc : dcel.faces()) if (fc->isBounded()) ++n;
    return n;
  };

  // (a) single-vertex path
  { PDCEL d; std::vector<PDCELVertex *> vs; PDCELFace *f = fresh(d, vs);
    std::size_t n = faceCount(d);
    std::vector<PDCELVertex *> p = {new PDCELVertex(0.0, 3, 0)};
    all_ok &= d.splitFaceByPolyline(f, p).empty() && faceCount(d) == n; }

  // (b) endpoint not on boundary (both interior)
  { PDCEL d; std::vector<PDCELVertex *> vs; PDCELFace *f = fresh(d, vs);
    std::size_t n = faceCount(d);
    std::vector<PDCELVertex *> p = {new PDCELVertex(0.0, 2, 2),
                                    new PDCELVertex(0.0, 4, 2)};
    all_ok &= d.splitFaceByPolyline(f, p).empty() && faceCount(d) == n; }

  // (c) interior path vertex lies on the boundary
  { PDCEL d; std::vector<PDCELVertex *> vs; PDCELFace *f = fresh(d, vs);
    std::size_t n = faceCount(d);
    std::vector<PDCELVertex *> p = {new PDCELVertex(0.0, 2, 0),
                                    new PDCELVertex(0.0, 6, 2),   // on right edge
                                    new PDCELVertex(0.0, 4, 4)};
    all_ok &= d.splitFaceByPolyline(f, p).empty() && faceCount(d) == n; }

  // (d) path segment already an existing boundary edge
  { PDCEL d; std::vector<PDCELVertex *> vs; PDCELFace *f = fresh(d, vs);
    std::size_t n = faceCount(d);
    std::vector<PDCELVertex *> p = {vs[0], vs[1]};  // existing bottom edge
    all_ok &= d.splitFaceByPolyline(f, p).empty() && faceCount(d) == n; }

  report(name, all_ok, "4 invalid inputs rejected without mutation");
}

}  // namespace

int main() {
  std::printf("=== t0_dcel: splitFaceByPolyline harness ===\n");
  std::printf("out dir: %s\n\n", T0_DCEL_OUT_DIR);

  case_straight_chord();
  case_bent_path();
  case_zigzag_multibend();
  case_layer_band();
  case_endpoints_on_vertices();
  case_thin_pinch_band();
  case_z_layered_spec();
  case_invalid_inputs();

  std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures == 0 ? 0 : 1;
}
