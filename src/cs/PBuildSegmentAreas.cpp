#include "PSegment.hpp"

#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "debug/baseOffsetMapJson.hpp"
#include "debug/baseOffsetMapSvg.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "offset_clipper2.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Phase-2a (plan-20260618-per-layer-offset-within-shell.md): env gate for
// the layered per-layer-offset path. Off by default — the legacy
// interpolated-layer path stays the production default.
bool useLayeredOffsetEnv() {
  static const bool cached = [] {
    const char* raw = std::getenv("PREVABS_LAYERED_OFFSET");
    if (!raw || !*raw) return false;
    std::string s(raw);
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    return s == "1" || s == "true" || s == "on" || s == "yes";
  }();
  return cached;
}

// Foot-of-perpendicular distance from q to a polyline (open or closed).
double footDistToPolylineSeg(const SPoint2& q,
                             const std::vector<SPoint2>& poly, bool closed) {
  const int n = static_cast<int>(poly.size());
  if (n < 2) return std::numeric_limits<double>::infinity();
  const int ns = closed ? n : n - 1;
  double best = std::numeric_limits<double>::infinity();
  for (int i = 0; i < ns; ++i) {
    const SPoint2& a = poly[i];
    const SPoint2& b = poly[(i + 1) % n];
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    double u = 0.0;
    if (len2 > 0.0) {
      u = ((q.x() - a.x()) * dx + (q.y() - a.y()) * dy) / len2;
      u = std::max(0.0, std::min(1.0, u));
    }
    const double fx = a.x() + u * dx, fy = a.y() + u * dy;
    best = std::min(best, std::hypot(q.x() - fx, q.y() - fy));
  }
  return best;
}

// Route-i offset curve of `base` at `dist` — the primary run. Closed →
// largest |signed area| polygon; open → most-vertices side run. Mirrors the
// Phase-0 prototype (test/integration/t0_offset_clipper2/phase0_layered_v.cpp).
std::vector<SPoint2> offsetCurveSeg(const std::vector<SPoint2>& base,
                                    bool closed, int clipper_side,
                                    double dist) {
  auto polys =
      prevabs::geo::offsetWithClipper2(base, closed, clipper_side, dist);
  const prevabs::geo::OffsetPolygon* best = nullptr;
  double best_metric = -1.0;
  for (const auto& p : polys) {
    if (p.points.size() < 2) continue;
    double metric = static_cast<double>(p.points.size());
    if (closed) {
      double a = 0.0;
      const std::size_t m = p.points.size();
      for (std::size_t i = 0; i < m; ++i) {
        const auto& p0 = p.points[i];
        const auto& p1 = p.points[(i + 1) % m];
        a += p0.x() * p1.y() - p1.x() * p0.y();
      }
      metric = std::fabs(0.5 * a);
    }
    if (metric > best_metric) { best_metric = metric; best = &p; }
  }
  return best ? best->points : std::vector<SPoint2>{};
}

}  // namespace

namespace {

void logSkippingSegmentAreasAction(
    const char *caller, const std::string &segment_name,
    const std::string &reason) {
  if (config.debug_level >= DebugLevel::join) PLOG(debug) << "skipping " << caller << " for segment '"
              << segment_name << "': " << reason;
}

void deleteDetachedLineSegment(PGeoLineSegment *segment) {
  if (segment == nullptr) {
    return;
  }
  delete segment->v1();
  delete segment->v2();
  delete segment;
}


std::string faceLabel(PDCELFace *face, PModel *model) {
  if (face == nullptr) {
    return "nullptr";
  }

  (void)model;
  return face->displayLabel();
}




void logVertexEdgeRing(
    PDCELVertex *vertex, PModel *model, const std::string &prefix) {
  if (vertex == nullptr) {
    PLOG(error) << prefix << ": vertex=nullptr";
    return;
  }

  PLOG(error) << prefix << ": vertex=" << vertex->printString();
  if (vertex->edge() == nullptr) {
    PLOG(error) << prefix << ": edge ring is empty";
    return;
  }

  PDCELHalfEdge *start = vertex->edge();
  PDCELHalfEdge *he = start;
  int iter = 0;
  do {
    if (he == nullptr) {
      PLOG(error) << prefix << ": encountered nullptr half-edge in ring";
      return;
    }
    if (++iter > kDCELLoopHardCap) {
      PLOG(error) << prefix
                  << ": edge ring walk exceeded "
                  << kDCELLoopHardCap << " steps";
      return;
    }

    std::ostringstream line;
    line << prefix
         << ": outgoing[" << (iter - 1) << "] "
         << he->source()->printString()
         << " -> " << he->target()->printString()
         << " | face=" << faceLabel(he->face(), model)
         << " | loop=" << (he->loop() ? "set" : "nullptr");
    PLOG(error) << line.str();

    if (he->twin() == nullptr) {
      PLOG(error) << prefix << ": outgoing[" << (iter - 1)
                  << "] has nullptr twin";
      return;
    }
    he = he->twin()->next();
  } while (he != nullptr && he != start);

  if (he == nullptr) {
    PLOG(error) << prefix << ": edge ring terminated at nullptr next";
  }
}




void logMissingHalfEdgeInFace(
    const std::string &caller, const std::string &segment_name,
    PDCELVertex *vertex, PDCELFace *expected_face,
    const BuilderConfig &bcfg) {
  PLOG(error) << caller
              << ": findHalfEdgeInFace returned nullptr"
              << " for segment '" << segment_name << "'"
              << ", vertex=" << (vertex ? vertex->printString() : "nullptr")
              << ", expected_face="
              << faceLabel(expected_face, bcfg.model);
  logVertexEdgeRing(
      vertex, bcfg.model,
      caller + ": vertex edge ring for segment '" + segment_name + "'");
}




void logMissingHalfEdgeBetween(
    const std::string &caller, const std::string &segment_name,
    PDCELVertex *source, PDCELVertex *target, const BuilderConfig &bcfg) {
  PLOG(error) << caller
              << ": findHalfEdgeBetween returned nullptr"
              << " for segment '" << segment_name << "'"
              << ", source="
              << (source ? source->printString() : "nullptr")
              << ", target="
              << (target ? target->printString() : "nullptr");
  logVertexEdgeRing(
      source, bcfg.model,
      caller + ": source edge ring for segment '" + segment_name + "'");
}




// plan-20260522 §3.2: geometric replacement for the legacy
// `usesOffsetAsBaseAtPair(Δbase=0)` signal. The "base edge degenerate"
// condition is no longer derived from the staircase's discrete index
// history — it's read straight off the two base vertices that bound
// the area:
//
//   if |base[i] - base[i-1]| < GEO_TOL → use offset edge as "base"
//
// Under the legacy editor-splice path, Δbase=0 happens exactly when
// the two indexed base vertices are the same PDCELVertex* (so their
// distance is 0). Under the Phase 2 geometry-derived staircase, the
// rebuilt pairs may instead have Δbase=1 with two distinct PDCELVertex
// pointers that nonetheless sit on top of each other after join trim;
// the geometric check still fires.
bool usesOffsetAsBaseAt(
    const PDCELVertex *vb_prev, const PDCELVertex *vb_curr) {
  if (vb_prev == nullptr || vb_curr == nullptr) return false;
  return vb_prev->point().distance(vb_curr->point()) < GEO_TOL;
}




PGeoLineSegment *buildAreaBaseSegmentFromPair(
    Baseline *curve_base, Baseline *curve_offset,
    const BaseOffsetMap &pairs, std::size_t pair_index) {
  const int vbi = pairs[pair_index].base;
  const int voi = pairs[pair_index].offset;
  // vbi == 0 means the current pair sits on the very first base vertex
  // (the legacy Δbase=0 case at step 1, equivalent to "use offset edge").
  // There is no preceding base vertex to form a base edge with, so route
  // straight to the offset branch and skip the geometric distance check.
  const bool use_offset_as_base =
      (vbi <= 0)
          ? true
          : usesOffsetAsBaseAt(curve_base->vertices()[vbi - 1],
                               curve_base->vertices()[vbi]);

  if (!use_offset_as_base) {
    return new PGeoLineSegment(
        curve_base->vertices()[vbi - 1],
        curve_base->vertices()[vbi]);
  }

  return new PGeoLineSegment(
      curve_offset->vertices()[voi - 1],
      curve_offset->vertices()[voi]);
}




bool traversesPrevOnHeadBound(int layup_side) {
  // Open-bound traversal starts from the base vertex and then walks the
  // current face boundary until it meets the offset-side endpoint.
  // The head and tail see opposite boundary orientations, so the correct
  // traversal direction is intentionally inverted between the two ends.
  return layup_side > 0;
}




bool traversesPrevOnTailBound(int layup_side) {
  return layup_side < 0;
}

} // namespace




// Split the bound edge [vb, vo] parametrically by layup into layer vertices.
// Splits DCEL edges in place. Returns the new intermediate vertices.
std::vector<PDCELVertex *> Segment::splitBoundByLayup(
    PDCELVertex *vb, PDCELVertex *vo, const BuilderConfig &bcfg) {

  std::vector<PDCELVertex *> layer_vertices;
  PDCELVertex *v_layer, *v_layer_prev = nullptr;
  double cumu_thk = 0, norm_thk;

  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();
    norm_thk = cumu_thk / _layup->getTotalThickness();
    v_layer = new PDCELVertex(
        getParametricPoint(vb->point(), vo->point(), norm_thk));

  PDCELVertex *v1_tmp = (i == 0) ? vb : v_layer_prev;
  PDCELHalfEdge *he_tmp = bcfg.dcel->findHalfEdgeBetween(v1_tmp, vo);
  if (he_tmp == nullptr) {
    logMissingHalfEdgeBetween(
        "splitBoundByLayup", _name, v1_tmp, vo, bcfg);
    break;
  }
  v_layer = bcfg.dcel->splitEdge(he_tmp, v_layer);

    layer_vertices.push_back(v_layer);
    v_layer_prev = v_layer;
  }

  return layer_vertices;
}




// Search face boundary edges from v_prev for the intersection with the
// infinite support line of ls_offset. Open-bound construction uses offset
// construction lines whose valid hit may lie beyond the finite ls_offset end
// points, so only the face-edge parameter is range-checked here.
// go_prev controls traversal direction (true = prev(), false = next()).
// May split an edge. Returns the intersection vertex, or nullptr if not found.
PDCELVertex *Segment::findLayerIntersectionOnFace(
    PDCELVertex *v_prev, PDCELFace *face,
    PGeoLineSegment *ls_offset, bool go_prev, PDCELVertex *stop_vertex,
    const BuilderConfig &bcfg) {

  PDCELVertex *v_layer = nullptr;
  double u1_tmp, u2_tmp;
  const double endpoint_tol =
      (bcfg.tol > TOLERANCE) ? bcfg.tol : TOLERANCE;

  PDCELHalfEdge *he_tmp = bcfg.dcel->findHalfEdgeInFace(v_prev, face);
  if (he_tmp == nullptr) {
    logMissingHalfEdgeInFace(
        "findLayerIntersectionOnFace", _name, v_prev, face, bcfg);
    return nullptr;
  }
  PDCELHalfEdge *he_base = he_tmp;
  if (go_prev) {
    he_tmp = he_tmp->prev();
  }

  int _iter = 0;
  do {
    if (++_iter > 65536) {
      throw std::runtime_error(
          "DCEL loop walk exceeded 65536 iterations"
          " in findLayerIntersectionOnFace at " +
          he_base->printString());
    }
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "";
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "he_tmp: " + he_tmp->printString();

    // Skip degenerate edges (source and target at the same position).
    // These can be created by splitEdge when the split vertex lands on an
    // existing endpoint, producing a zero-length half-edge.
    if (he_tmp->source()->point().distance(he_tmp->target()->point())
        < TOLERANCE) {
      PLOG(warning) << "degenerate edge, skipping";
    }
    else {
      bool not_parallel = calcLineIntersection2D(
          he_tmp->toLineSegment(), ls_offset, u1_tmp, u2_tmp, TOLERANCE);

            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "u1_tmp = " << u1_tmp;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "u2_tmp = " << u2_tmp;

      if (not_parallel) {
        if (fabs(u1_tmp) <= endpoint_tol) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 1: intersect at source";
          v_layer = he_tmp->source();
          break;
        }
        else if (fabs(1 - u1_tmp) <= endpoint_tol) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 2: intersect at target";
          v_layer = he_tmp->target();
          break;
        }
        else if (u1_tmp > 0 && u1_tmp < 1) {
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  case 3: intersect current edge";
          v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
                    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  v_layer: " + v_layer->printString();
          v_layer = bcfg.dcel->splitEdge(he_tmp, v_layer);
          break;
        }
        // u1 out of range on this edge: fall through to advance
      }
      // parallel: fall through to advance
    }

    // Stop condition: reached the boundary of the offset curve
    if (go_prev && he_tmp->source() == stop_vertex) {
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "reach the last edge";
      break;
    }
    if (!go_prev && he_tmp->target() == stop_vertex) {
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "reach the last edge";
      break;
    }

    // Advance in traversal direction — always valid, no stale pointer
    he_tmp = go_prev ? he_tmp->prev() : he_tmp->next();

  } while (he_tmp != he_base);

  if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "findLayerIntersectionOnFace: skipping remaining search"
              << " for segment '" << _name << "' because no face-boundary"
              << " intersection was found before reaching the stop vertex";
  return v_layer;
}




// Build layer vertices on an open bound by intersecting offset line segments
// with the face boundary. v_start is the seed vertex; go_prev controls
// traversal direction. Returns the new layer vertices.
std::vector<PDCELVertex *> Segment::buildOpenBoundLayerVertices(
    PDCELVertex *v_start, PGeoLineSegment *ls_base,
    bool go_prev, PDCELVertex *stop_vertex, const BuilderConfig &bcfg) {

  std::vector<PDCELVertex *> layer_vertices;
  PDCELVertex *v_layer_prev = v_start;
  double cumu_thk = 0;
  const int offset_dir = layupSide();

  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "layer " + std::to_string(i + 1);

    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();

        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "cumu_thk = " << cumu_thk;

    PGeoLineSegment *ls_offset = offsetLineSegment(ls_base, offset_dir, cumu_thk);
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "ls_offset: " + ls_offset->printString();

    PDCELVertex *v_layer = findLayerIntersectionOnFace(
        v_layer_prev, _face, ls_offset, go_prev, stop_vertex, bcfg);
    deleteDetachedLineSegment(ls_offset);

    if (v_layer == nullptr) {
      PLOG(error) << "cannot find intersection";
      break;
    }

        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "v_layer: " + v_layer->printString();
    layer_vertices.push_back(v_layer);
    v_layer_prev = v_layer;
  }

  return layer_vertices;
}




// Section 1: compute beginning-bound layer vertices.
// For closed segments, also fills first_bound_vertices.
std::vector<PDCELVertex *> Segment::buildBeginningBound(
    std::vector<PDCELVertex *> &first_bound_vertices,
    const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << 
      "1. creating the beginning bound of the first area";

  std::vector<PDCELVertex *> prev_bound_vertices;

  if (closed()) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "closed segment";
    prev_bound_vertices = splitBoundByLayup(
        _curve_base->vertices()[0], _curve_offset->vertices()[0], bcfg);
    first_bound_vertices = prev_bound_vertices;
  }
  else {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "open segment";

    PDCELVertex *vb = _curve_base->vertices()[0];
    PDCELVertex *vo = _curve_offset->vertices()[0];
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) <<
        "first vertex of the base: " + vb->printString();
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) <<
        "first vertex of the offset: " + vo->printString();

    // Head area's base edge runs base[0] -> base[1].
    const bool use_offset_as_base = usesOffsetAsBaseAt(
        _curve_base->vertices()[0], _curve_base->vertices()[1]);
    const int layup_side = layupSide();

    PGeoLineSegment *ls_base;
    bool owns_ls_base_vertices = false;
    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
          _curve_base->vertices()[0], _curve_base->vertices()[1]);
    }
    else {
      // Degenerate head case: the first staircase step stays on base vertex 0,
      // so the physical head thickness stack must be generated from the offset
      // edge and translated back by the total laminate thickness.
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          _curve_offset->vertices()[0], _curve_offset->vertices()[1]);
      const int dir = -layup_side;
      ls_base = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_vertices = true;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "degenerated case";
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_base: " + ls_base->printString();
      delete ls_tmp;
    }

    const bool go_prev = traversesPrevOnHeadBound(layup_side);
    prev_bound_vertices = buildOpenBoundLayerVertices(
        _curve_base->vertices()[0], ls_base, go_prev,
        _curve_offset->vertices().front(), bcfg);
    if (owns_ls_base_vertices) {
      deleteDetachedLineSegment(ls_base);
    }
    else {
      delete ls_base;
    }
  }

  return prev_bound_vertices;
}




// Section 2: create all intermediate areas and update prev_bound_vertices
// and count.
void Segment::createIntermediateAreas(
    std::vector<PDCELVertex *> &prev_bound_vertices,
    int &count, const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "2. creating areas";

  // issue-20260521-skip-dropped-areas: in regions where Clipper2 produced
  // no genuine offset correspondence (h_i < |dist| → base vertex eaten by
  // inset), the staircase forward-fills every base index in the dropped
  // range to the same offset vertex. splitFace'ing through that shared
  // offset vertex generates a fan of slivers that gmsh cannot recover.
  // For OPEN segments we skip those areas outright — geometrically the
  // laminate is locally absent at the endpoints, the staircase already
  // shrinks the covered span, and the user has been warned via the M/N
  // warning + "skin dropped" message.
  // For CLOSED segments the dropped range lives in the middle of a loop;
  // skipping those areas would tear the shell open and break the DCEL
  // face closure. Keep the legacy forward-fill behavior — sliver areas
  // are sometimes the lesser evil there. Until a proper closed-loop
  // dropped-region story exists, the closed branch keeps the gmsh risk.
  const bool skip_dropped_areas =
      !closed() && !_dropped_base_ranges_lo.empty();
  auto inDroppedRange = [&](int base_idx) {
    if (!skip_dropped_areas) return false;
    for (std::size_t r = 0; r < _dropped_base_ranges_lo.size(); ++r) {
      if (base_idx >= _dropped_base_ranges_lo[r]
          && base_idx <= _dropped_base_ranges_hi[r]) {
        return true;
      }
    }
    return false;
  };

  // Boundary-area skip: the area at pair k uses the base edge
  // base[pairs[k].base - 1] → base[pairs[k].base]. When EITHER endpoint
  // sits inside a dropped range, the edge crosses the cusp/thin region
  // and gmsh cannot mesh the resulting long thin sliver — this is the
  // same `Unable to recover the edge` failure mode that motivated Stage E
  // pre-trim (plan-20260522-stage-e-pretrim.md, Phase B). Skipping when
  // the previous base index is dropped removes the one extra area
  // immediately after the dropped run on each side.
  auto areaAtPairUsesDroppedEdge = [&](int base_idx) {
    if (!skip_dropped_areas) return false;
    if (inDroppedRange(base_idx)) return true;
    if (base_idx > 0 && inDroppedRange(base_idx - 1)) return true;
    return false;
  };
  for (auto k = 1; k < _base_offset_indices_pairs.size() - 1; k++) {
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "area " + std::to_string(k);
    const int layup_side = layupSide();

    int vbi_tmp = _base_offset_indices_pairs[k].base;
    int voi_tmp = _base_offset_indices_pairs[k].offset;

    if (areaAtPairUsesDroppedEdge(vbi_tmp)) {
      if (bcfg.debug_level >= DebugLevel::join) {
        PLOG(debug) << "  skipping area at base[" << vbi_tmp
                    << "] (base edge endpoint in dropped range — "
                       "skin locally absent or boundary sliver)";
      }
      continue;
    }

    PDCELVertex *vb_tmp = _curve_base->vertices()[vbi_tmp];
    PDCELVertex *vo_tmp = _curve_offset->vertices()[voi_tmp];
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "  base vertex: " + vb_tmp->printString();
        if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << 
        "  offset vertex: " + vo_tmp->printString();

    std::list<PDCELFace *> new_faces =
        bcfg.dcel->splitFace(_face, vb_tmp, vo_tmp);

    PArea *area = new PArea(this);
    count++;
    if (layup_side > 0) {
      area->setFace(new_faces.front());
      _face = new_faces.back();
    }
    else {
      area->setFace(new_faces.back());
      _face = new_faces.front();
    }

    PGeoLineSegment *ls_base = buildAreaBaseSegmentFromPair(
        _curve_base, _curve_offset.get(), _base_offset_indices_pairs, k);

    // Kept for debug prints; PArea owns the segment via its destructor.
    area->setLineSegmentBase(ls_base);

    // e1/e2 == "baseline" are now resolved per-face in
    // PArea::applyFrameFromBaseCurve (Phase B of
    // plan-20260514-decouple-local-frame-from-map.md). Only the "layup"
    // selector still pulls the through-thickness vector here.
    if (_mat_orient_e2 == "layup") {
      PGeoLineSegment *ls_layup = new PGeoLineSegment(vb_tmp, vo_tmp);
      area->setLocaly2(ls_layup->toVector());
      delete ls_layup;
    }

    bcfg.model->setFaceName(
        area->face(), _name + "_area_" + std::to_string(count));
    area->setPrevBoundVertices(prev_bound_vertices);

    for (auto v : splitBoundByLayup(vb_tmp, vo_tmp, bcfg)) {
      area->addNextBoundVertex(v);
    }

    _areas.emplace_back(area);
    prev_bound_vertices = area->nextBoundVertices();
  }

}




// Section 3: create and append the last (or only) area.
void Segment::buildLastArea(
    const std::vector<PDCELVertex *> &prev_bound_vertices,
    const std::vector<PDCELVertex *> &first_bound_vertices,
    int count, const BuilderConfig &bcfg) {

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "3. creating the last area";

  PArea *area = new PArea(this);
  count++;
  area->setFace(_face);

  const std::size_t last_pair_index = _base_offset_indices_pairs.size() - 1;
  PGeoLineSegment *ls_base = buildAreaBaseSegmentFromPair(
      _curve_base, _curve_offset.get(), _base_offset_indices_pairs, last_pair_index);
  // Kept for debug prints; PArea owns the segment via its destructor.
  area->setLineSegmentBase(ls_base);

  // e1/e2 == "baseline" are now resolved per-face in
  // PArea::applyFrameFromBaseCurve (Phase B of
  // plan-20260514-decouple-local-frame-from-map.md). Only the "layup"
  // selector still pulls the through-thickness vector here.
  if (_mat_orient_e2 == "layup") {
    PGeoLineSegment *ls_layup = new PGeoLineSegment(
        _curve_base->vertices().back(),
        _curve_offset->vertices().back());
    area->setLocaly2(ls_layup->toVector());
    delete ls_layup;
  }

  bcfg.model->setFaceName(
      area->face(), _name + "_area_" + std::to_string(count));
  area->setPrevBoundVertices(prev_bound_vertices);

  if (closed()) {
    area->setNextBoundVertices(first_bound_vertices);
  }
  else {
    // Tail area's base edge runs base[N-2] -> base[N-1].
    const auto &base_v_tail = _curve_base->vertices();
    const bool use_offset_as_base = usesOffsetAsBaseAt(
        base_v_tail[base_v_tail.size() - 2],
        base_v_tail[base_v_tail.size() - 1]);
    const int layup_side = layupSide();

    PGeoLineSegment *ls_base_end;
    bool owns_ls_base_end_vertices = false;
    if (!use_offset_as_base) {
      ls_base_end = new PGeoLineSegment(
          _curve_base->vertices()[_curve_base->vertices().size() - 2],
          _curve_base->vertices()[_curve_base->vertices().size() - 1]);
    }
    else {
      // Degenerate tail case: the last staircase step stays on the previous
      // base vertex, so the closing bound is anchored on the offset edge and
      // shifted back to recover the effective base-side construction segment.
      // Index the offset vector by its own size — under the legacy open-path
      // resample step M == N held implicitly, but the Clipper2 backend's
      // nearest-pairing variant (issue-20260520) skips that resample and
      // allows M < N.
      const auto& off_v_tail = _curve_offset->vertices();
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          off_v_tail[off_v_tail.size() - 2],
          off_v_tail[off_v_tail.size() - 1]);
      const int dir = layup_side;
      ls_base_end = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_end_vertices = true;
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "degenerated case";
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << " ls_base: " + ls_base_end->printString();
      delete ls_tmp;
    }

    const bool go_prev = traversesPrevOnTailBound(layup_side);
    for (auto v : buildOpenBoundLayerVertices(
             _curve_base->vertices().back(), ls_base_end, go_prev,
             _curve_offset->vertices().back(), bcfg)) {
      area->addNextBoundVertex(v);
    }
    if (owns_ls_base_end_vertices) {
      deleteDetachedLineSegment(ls_base_end);
    }
    else {
      delete ls_base_end;
    }
  }

  _areas.emplace_back(area);
}


// Phase-2a: route-i per-layer offset validation (no DCEL effect). See header.
void Segment::validatePerLayerOffsets(const BuilderConfig &bcfg) {
  if (_layup == nullptr || _curve_base == nullptr) return;
  // Non-const copy: Layer/Lamina accessors (getLamina/getStack/getThickness)
  // are non-const member functions.
  std::vector<Layer> layers = _layup->getLayers();
  const int n = static_cast<int>(layers.size());
  if (n == 0) return;

  // Base in SPoint2 (drop trailing duplicate on a closed Baseline).
  std::vector<SPoint2> base;
  base.reserve(_curve_base->vertices().size());
  for (auto* v : _curve_base->vertices()) {
    base.emplace_back(v->point2()[0], v->point2()[1]);
  }
  const bool is_closed = closed();
  if (is_closed && base.size() >= 2
      && _curve_base->vertices().front() == _curve_base->vertices().back()) {
    base.pop_back();
  }
  if (base.size() < 2) return;

  const int side = layupSide();
  const double total = _layup->getTotalThickness();
  if (side == 0 || !(total > 0.0)) return;
  // Match offset()'s side convention: closed flips, open passes through.
  const int clipper_side = is_closed ? -side : side;

  const std::vector<SPoint2> shell =
      offsetCurveSeg(base, is_closed, clipper_side, total);
  if (shell.size() < 2) {
    PLOG(warning) << "per-layer[" << _name << "]: shell offset empty; skip";
    return;
  }

  std::vector<SPoint2> prev = base;
  std::vector<SPoint2> raw_last;
  double cum = 0.0, prev_mean = -1.0;
  bool nesting_ok = true, maps_ok = true;
  for (int k = 0; k < n; ++k) {
    const double tk = (layers[k].getLamina() != nullptr)
                          ? layers[k].getLamina()->getThickness()
                                * layers[k].getStack()
                          : 0.0;
    cum += tk;
    const std::vector<SPoint2> curve =
        offsetCurveSeg(base, is_closed, clipper_side, cum);
    if (curve.size() < 2) { maps_ok = false; break; }
    raw_last = curve;

    const auto plan = prevabs::geo::rebuildBaseOffsetMapFromGeometry(
        prev, curve, is_closed, side, std::max(tk, 1e-12),
        prevabs::geo::readPairingAlgoEnv());
    if (!plan.ok) maps_ok = false;

    double dmax = 0.0, dsum = 0.0;
    for (const auto& p : curve) {
      const double d = footDistToPolylineSeg(p, base, is_closed);
      dmax = std::max(dmax, d);
      dsum += d;
    }
    const double dmean = dsum / static_cast<double>(curve.size());
    const bool within = (k == n - 1) || (dmax < total + 1e-6);
    if (!within || dmean < prev_mean - 1e-6) nesting_ok = false;
    prev_mean = dmean;
    prev = plan.ok ? plan.offset_points : curve;
  }

  // zero-gap: route-i cum_n curve must equal the shell (same offset call).
  double gap = std::numeric_limits<double>::infinity();
  if (raw_last.size() == shell.size()) {
    gap = 0.0;
    for (std::size_t i = 0; i < shell.size(); ++i) {
      gap = std::max(gap, std::hypot(raw_last[i].x() - shell[i].x(),
                                     raw_last[i].y() - shell[i].y()));
    }
  }
  const bool zero_gap = gap < 1e-6;
  const bool pass = nesting_ok && zero_gap && maps_ok;
  PLOG(info) << "per-layer[" << _name << "]: "
             << (is_closed ? "closed" : "open") << " N=" << base.size()
             << " layers=" << n << " nesting=" << (nesting_ok ? "Y" : "N")
             << " zero_gap=" << (zero_gap ? "Y" : "N") << "(gap=" << gap << ")"
             << " maps=" << (maps_ok ? "Y" : "N") << " -> "
             << (pass ? "PASS" : "FAIL");
}


void Segment::buildAreas(const BuilderConfig &bcfg) {
  PLogContext segment_areas_context("segment areas: " + _name);
  PLogSection segment_areas_section(
      DebugLevel::join, "segment areas", _name);
  if (!requireExactState(LifecycleState::ShellBuilt, "buildAreas")) {
    logSkippingSegmentAreasAction(
        "buildAreas", _name, "segment is not in ShellBuilt state");
    return;
  }
  if (!validateStateInvariants("buildAreas")) {
    logSkippingSegmentAreasAction(
        "buildAreas", _name, "lifecycle invariants validation failed");
    return;
  }

    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "building areas of segment: " + _name;

  // plan-20260522: re-derive the staircase from the current base/offset
  // geometry. The persisted `_base_offset_indices_pairs` is treated as a
  // geometric view, not as independent state — any prior join-time
  // mutation of the curves is reflected by recomputing here from the
  // present polylines, so no editor-side discrete splice is needed.
  // See rebuildBaseOffsetMapFromGeometry in include/offset_clipper2.hpp.
  if (_adaptive_variable_offset) {
    if (bcfg.debug_level >= DebugLevel::join) {
      PLOG(debug) << "buildAreas: keeping adaptive variable-offset "
                  << "BaseOffsetMap for segment '" << _name << "'";
    }
  } else if (_curve_base != nullptr && _curve_offset != nullptr) {
    std::vector<SPoint2> base_pts;
    base_pts.reserve(_curve_base->vertices().size());
    for (auto *v : _curve_base->vertices()) {
      base_pts.emplace_back(v->point2()[0], v->point2()[1]);
    }
    // Closed Baseline convention has front == back as the same pointer;
    // drop the trailing duplicate so the SPoint2 helper sees N distinct
    // vertices, matching the contract of planReverseMatchByNearest.
    if (closed() && base_pts.size() >= 2
        && _curve_base->vertices().front()
             == _curve_base->vertices().back()) {
      base_pts.pop_back();
    }
    std::vector<SPoint2> off_pts;
    off_pts.reserve(_curve_offset->vertices().size());
    for (auto *v : _curve_offset->vertices()) {
      off_pts.emplace_back(v->point2()[0], v->point2()[1]);
    }
    const int side = requireValidLayupSide("buildAreas/derive");
    const double dist =
        (_layup != nullptr) ? _layup->getTotalThickness() : 0.0;
    if (side != 0 && dist > 0.0 && base_pts.size() >= 2
        && off_pts.size() >= 2) {
      const auto plan = prevabs::geo::rebuildBaseOffsetMapFromGeometry(
          base_pts, off_pts, closed(), side, dist,
          prevabs::geo::readPairingAlgoEnv());
      if (plan.ok) {
        if (bcfg.debug_level >= DebugLevel::join) {
          PLOG(debug) << "buildAreas: derived staircase: "
                      << _base_offset_indices_pairs.size() << " pairs (was) -> "
                      << plan.id_pairs.size() << " pairs (derived); "
                      << "dropped ranges (was)="
                      << _dropped_base_ranges_lo.size()
                      << ", (derived)=" << plan.dropped_base_ranges_lo.size();
        }
        _base_offset_indices_pairs = plan.id_pairs;
        _dropped_base_ranges_lo    = plan.dropped_base_ranges_lo;
        _dropped_base_ranges_hi    = plan.dropped_base_ranges_hi;
      } else {
        PLOG(warning) << "buildAreas: rebuildBaseOffsetMapFromGeometry "
                         "failed for segment '" << _name
                      << "' — keeping persisted staircase";
      }
    }
  }

  // Phase-1 (plan-20260618-per-layer-offset-within-shell.md): emit the
  // base-offset-map debug dump here (moved from `offsetCurveBase`), now that
  // `_base_offset_indices_pairs` is the authoritative geometry-derived
  // staircase actually used for area construction. `--debug geo` only.
  if (bcfg.debug_level >= DebugLevel::geo
      && _curve_base != nullptr && _curve_offset != nullptr) {
    const std::string svg_path = config.file_directory + config.file_base_name
                               + "." + _name + ".base_offset_map.svg";
    dumpBaseOffsetMapSvg(svg_path, _name,
                         _curve_base->vertices(),
                         _curve_offset->vertices(),
                         _base_offset_indices_pairs,
                         &_offset_vertex_resampled,
                         &_offset_pre_resample_raw_points);
    const std::string json_path = config.file_directory + config.file_base_name
                                + "." + _name + ".base_offset_map.json";
    const double dist =
        (_layup != nullptr) ? _layup->getTotalThickness() : 0.0;
    dumpBaseOffsetMapJson(json_path, _name,
                          _curve_base->vertices(),
                          _curve_offset->vertices(),
                          _base_offset_indices_pairs,
                          closed(),
                          requireValidLayupSide("buildAreas/json"),
                          dist,
                          &_dropped_base_ranges_lo,
                          &_dropped_base_ranges_hi,
                          _used_adaptive_thickness ? &_adaptive_plan : nullptr);
  }

  // Phase-2a: validate route-i per-layer offsets in the production context
  // (flag-gated, no DCEL effect — legacy path below still builds the mesh).
  if (useLayeredOffsetEnv()) {
    validatePerLayerOffsets(bcfg);
  }

  std::vector<PDCELVertex *> first_bound_vertices;
  std::vector<PDCELVertex *> prev_bound_vertices =
      buildBeginningBound(first_bound_vertices, bcfg);

  int count = 0;
  createIntermediateAreas(prev_bound_vertices, count, bcfg);

  buildLastArea(prev_bound_vertices, first_bound_vertices, count, bcfg);

  for (auto &each_area : _areas) {
    each_area->buildLayers(bcfg);
  }
  _state = LifecycleState::AreasBuilt;
  validateStateInvariants("buildAreas");
  segment_areas_section.setEndDetails(
      "areas=" + std::to_string(_areas.size())
      + ", layers=" + std::to_string(layerCount()));
}
