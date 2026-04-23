#include "PSegment.hpp"

#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace {

void deleteDetachedLineSegment(PGeoLineSegment *segment) {
  if (segment == nullptr) {
    return;
  }
  delete segment->v1();
  delete segment->v2();
  delete segment;
}




bool usesOffsetAsBaseAtPair(
    const BaseOffsetMap &pairs, std::size_t pair_index) {
  // Repeated base indices mean the staircase map advanced only on the offset
  // side at this step. Geometrically, the laminate consumed offset length
  // while the base curve stayed on the same vertex, so the local "base" edge
  // for area construction must come from the offset curve instead.
  return pair_index > 0
         && pairs[pair_index].base == pairs[pair_index - 1].base;
}




PGeoLineSegment *buildAreaBaseSegmentFromPair(
    Baseline *curve_base, Baseline *curve_offset,
    const BaseOffsetMap &pairs, std::size_t pair_index) {
  const int vbi = pairs[pair_index].base;
  const int voi = pairs[pair_index].offset;
  const bool use_offset_as_base =
      usesOffsetAsBaseAtPair(pairs, pair_index);

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
  PDCELHalfEdge *he_base = he_tmp;
  if (go_prev) {
    he_tmp = he_tmp->prev();
  }

  g_msg->increaseIndent();
  do {
        PLOG(debug) << "";
        PLOG(debug) << "he_tmp: " + he_tmp->printString();

    // Skip degenerate edges (source and target at the same position).
    // These can be created by splitEdge when the split vertex lands on an
    // existing endpoint, producing a zero-length half-edge.
    if (he_tmp->source()->point().distance(he_tmp->target()->point())
        < TOLERANCE) {
            g_msg->warn("degenerate edge, skipping");
    }
    else {
      bool not_parallel = calcLineIntersection2D(
          he_tmp->toLineSegment(), ls_offset, u1_tmp, u2_tmp, TOLERANCE);

            PLOG(debug) << "u1_tmp = " << u1_tmp;
            PLOG(debug) << "u2_tmp = " << u2_tmp;

      if (not_parallel) {
        if (fabs(u1_tmp) <= endpoint_tol) {
                    PLOG(debug) << "  case 1: intersect at source";
          v_layer = he_tmp->source();
          break;
        }
        else if (fabs(1 - u1_tmp) <= endpoint_tol) {
                    PLOG(debug) << "  case 2: intersect at target";
          v_layer = he_tmp->target();
          break;
        }
        else if (u1_tmp > 0 && u1_tmp < 1) {
                    PLOG(debug) << "  case 3: intersect current edge";
          v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
                    PLOG(debug) << "  v_layer: " + v_layer->printString();
          v_layer = bcfg.dcel->splitEdge(he_tmp, v_layer);
          break;
        }
        // u1 out of range on this edge: fall through to advance
      }
      // parallel: fall through to advance
    }

    // Stop condition: reached the boundary of the offset curve
    if (go_prev && he_tmp->source() == stop_vertex) {
            PLOG(debug) << "reach the last edge";
      break;
    }
    if (!go_prev && he_tmp->target() == stop_vertex) {
            PLOG(debug) << "reach the last edge";
      break;
    }

    // Advance in traversal direction — always valid, no stale pointer
    he_tmp = go_prev ? he_tmp->prev() : he_tmp->next();

  } while (he_tmp != he_base);
  g_msg->decreaseIndent();

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

  g_msg->increaseIndent();
  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
        PLOG(debug) << "layer " + std::to_string(i + 1);

    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();

        PLOG(debug) << "cumu_thk = " << cumu_thk;

    PGeoLineSegment *ls_offset = offsetLineSegment(ls_base, offset_dir, cumu_thk);
        PLOG(debug) << "ls_offset: " + ls_offset->printString();

    PDCELVertex *v_layer = findLayerIntersectionOnFace(
        v_layer_prev, _face, ls_offset, go_prev, stop_vertex, bcfg);
    deleteDetachedLineSegment(ls_offset);

    if (v_layer == nullptr) {
            g_msg->error("cannot find intersection");
      break;
    }

        PLOG(debug) << "v_layer: " + v_layer->printString();
    layer_vertices.push_back(v_layer);
    v_layer_prev = v_layer;
  }
  g_msg->decreaseIndent();

  return layer_vertices;
}




// Section 1: compute beginning-bound layer vertices.
// For closed segments, also fills first_bound_vertices.
std::vector<PDCELVertex *> Segment::buildBeginningBound(
    std::vector<PDCELVertex *> &first_bound_vertices,
    const BuilderConfig &bcfg) {

    PLOG(debug) << 
      "1. creating the beginning bound of the first area";
  g_msg->increaseIndent();

  std::vector<PDCELVertex *> prev_bound_vertices;

  if (closed()) {
        PLOG(debug) << "closed segment";
    prev_bound_vertices = splitBoundByLayup(
        _curve_base->vertices()[0], _curve_offset->vertices()[0], bcfg);
    first_bound_vertices = prev_bound_vertices;
  }
  else {
        PLOG(debug) << "open segment";

    PDCELVertex *vb = _curve_base->vertices()[0];
    PDCELVertex *vo = _curve_offset->vertices()[0];
        PLOG(debug) << 
        "first vertex of the base: " + vb->printString();
        PLOG(debug) << 
        "first vertex of the offset: " + vo->printString();

    const bool use_offset_as_base =
        usesOffsetAsBaseAtPair(_base_offset_indices_pairs, 1);
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
            PLOG(debug) << "degenerated case";
            PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            PLOG(debug) << " ls_base: " + ls_base->printString();
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

  g_msg->decreaseIndent();
  return prev_bound_vertices;
}




// Section 2: create all intermediate areas and update prev_bound_vertices
// and count.
void Segment::createIntermediateAreas(
    std::vector<PDCELVertex *> &prev_bound_vertices,
    int &count, const BuilderConfig &bcfg) {

    PLOG(debug) << "2. creating areas";
  g_msg->increaseIndent();

  for (auto k = 1; k < _base_offset_indices_pairs.size() - 1; k++) {
        PLOG(debug) << "area " + std::to_string(k);
    const int layup_side = layupSide();

    int vbi_tmp = _base_offset_indices_pairs[k].base;
    int voi_tmp = _base_offset_indices_pairs[k].offset;
    PDCELVertex *vb_tmp = _curve_base->vertices()[vbi_tmp];
    PDCELVertex *vo_tmp = _curve_offset->vertices()[voi_tmp];
        PLOG(debug) << "  base vertex: " + vb_tmp->printString();
        PLOG(debug) << 
        "  offset vertex: " + vo_tmp->printString();

    PGeoLineSegment *ls_layup = new PGeoLineSegment(vb_tmp, vo_tmp);

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

    area->setLineSegmentBase(ls_base);

    if (_mat_orient_e1 == "baseline") {
      area->setLocaly1(ls_base->toVector());
    }
    if (_mat_orient_e2 == "baseline") {
      area->setLocaly2(ls_base->toVector());
    }
    else if (_mat_orient_e2 == "layup") {
      area->setLocaly2(ls_layup->toVector());
    }
    delete ls_layup;

    bcfg.model->faceData(area->face()).name = _name + "_area_" + std::to_string(count);
    area->setPrevBoundVertices(prev_bound_vertices);

    for (auto v : splitBoundByLayup(vb_tmp, vo_tmp, bcfg)) {
      area->addNextBoundVertex(v);
    }

    _areas.emplace_back(area);
    prev_bound_vertices = area->nextBoundVertices();
  }

  g_msg->decreaseIndent();
}




// Section 3: create and append the last (or only) area.
void Segment::buildLastArea(
    const std::vector<PDCELVertex *> &prev_bound_vertices,
    const std::vector<PDCELVertex *> &first_bound_vertices,
    int count, const BuilderConfig &bcfg) {

    PLOG(debug) << "3. creating the last area";
  g_msg->increaseIndent();

  PArea *area = new PArea(this);
  count++;
  area->setFace(_face);

  const std::size_t last_pair_index = _base_offset_indices_pairs.size() - 1;
  PGeoLineSegment *ls_base = buildAreaBaseSegmentFromPair(
      _curve_base, _curve_offset.get(), _base_offset_indices_pairs, last_pair_index);
  area->setLineSegmentBase(ls_base);

  PGeoLineSegment *ls_layup = new PGeoLineSegment(
      _curve_base->vertices().back(),
      _curve_offset->vertices().back());

  if (_mat_orient_e1 == "baseline") {
    area->setLocaly1(ls_base->toVector());
  }
  if (_mat_orient_e2 == "baseline") {
    area->setLocaly2(ls_base->toVector());
  }
  else if (_mat_orient_e2 == "layup") {
    area->setLocaly2(ls_layup->toVector());
  }
  delete ls_layup;

  bcfg.model->faceData(area->face()).name = _name + "_area_" + std::to_string(count);
  area->setPrevBoundVertices(prev_bound_vertices);

  if (closed()) {
    area->setNextBoundVertices(first_bound_vertices);
  }
  else {
    const bool use_offset_as_base =
        usesOffsetAsBaseAtPair(_base_offset_indices_pairs, last_pair_index);
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
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          _curve_offset->vertices()[_curve_base->vertices().size() - 2],
          _curve_offset->vertices()[_curve_base->vertices().size() - 1]);
      const int dir = layup_side;
      ls_base_end = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_end_vertices = true;
            PLOG(debug) << "degenerated case";
            PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            PLOG(debug) << " ls_base: " + ls_base_end->printString();
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

  g_msg->decreaseIndent();
  _areas.emplace_back(area);
}




void Segment::buildAreas(const BuilderConfig &bcfg) {
  MESSAGE_SCOPE(g_msg);
  if (!requireExactState(LifecycleState::ShellBuilt, "buildAreas")) {
    return;
  }
  if (!validateStateInvariants("buildAreas")) {
    return;
  }

    PLOG(debug) << "building areas of segment: " + _name;

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
}
