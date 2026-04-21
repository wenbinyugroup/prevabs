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
#include <sstream>
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

// Search face boundary edges from v_prev for the intersection with ls_offset.
// go_prev controls traversal direction (true = prev(), false = next()).
// May split an edge. Returns the intersection vertex, or nullptr if not found.
PDCELVertex *Segment::findLayerIntersectionOnFace(
    PDCELVertex *v_prev, PDCELFace *face,
    PGeoLineSegment *ls_offset, bool go_prev,
    const BuilderConfig &bcfg) {

  PDCELVertex *v_layer = nullptr;
  double u1_tmp, u2_tmp;

  PDCELHalfEdge *he_tmp = bcfg.dcel->findHalfEdgeInFace(v_prev, face);
  PDCELHalfEdge *he_base = he_tmp;
  if (go_prev) {
    he_tmp = he_tmp->prev();
  }

  PDCELVertex *stop_vertex = _curve_offset->vertices().front();

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

      std::stringstream ss_u1_tmp;
      ss_u1_tmp << u1_tmp;
            PLOG(debug) << "u1_tmp = " + ss_u1_tmp.str();

      if (not_parallel) {
        if (fabs(u1_tmp) < bcfg.tol) {
                    PLOG(debug) << "  case 1: intersect at source";
          v_layer = he_tmp->source();
          break;
        }
        else if (fabs(1 - u1_tmp) < bcfg.tol) {
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
    bool go_prev, const BuilderConfig &bcfg) {

  std::vector<PDCELVertex *> layer_vertices;
  PDCELVertex *v_layer_prev = v_start;
  double cumu_thk = 0;
  int offset_dir = (slayupside == "left") ? 1 : -1;

  g_msg->increaseIndent();
  for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
        PLOG(debug) << "layer " + std::to_string(i + 1);

    cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                _layup->getLayers()[i].getStack();

    std::stringstream ss;
    ss << cumu_thk;
        PLOG(debug) << "cumu_thk = " + ss.str();

    PGeoLineSegment *ls_offset = offsetLineSegment(ls_base, offset_dir, cumu_thk);
        PLOG(debug) << "ls_offset: " + ls_offset->printString();

    PDCELVertex *v_layer = findLayerIntersectionOnFace(
        v_layer_prev, _face, ls_offset, go_prev, bcfg);
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

  if (_closed) {
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

    bool use_offset_as_base =
        (_base_offset_indices_pairs[0][0] == _base_offset_indices_pairs[1][0]);

    PGeoLineSegment *ls_base;
    bool owns_ls_base_vertices = false;
    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
          _curve_base->vertices()[0], _curve_base->vertices()[1]);
    }
    else {
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          _curve_offset->vertices()[0], _curve_offset->vertices()[1]);
      int dir = (slayupside == "left") ? -1 : 1;
      ls_base = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_vertices = true;
            PLOG(debug) << "degenerated case";
            PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            PLOG(debug) << " ls_base: " + ls_base->printString();
      delete ls_tmp;
    }

    bool go_prev = (slayupside == "left");
    prev_bound_vertices = buildOpenBoundLayerVertices(
        _curve_base->vertices()[0], ls_base, go_prev, bcfg);
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

    int vbi_tmp = _base_offset_indices_pairs[k][0];
    int voi_tmp = _base_offset_indices_pairs[k][1];
    bool use_offset_as_base =
        (vbi_tmp == _base_offset_indices_pairs[k - 1][0]);

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
    if (slayupside == "left") {
      area->setFace(new_faces.front());
      _face = new_faces.back();
    }
    else if (slayupside == "right") {
      area->setFace(new_faces.back());
      _face = new_faces.front();
    }

    PGeoLineSegment *ls_base;
    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
          _curve_base->vertices()[vbi_tmp - 1],
          _curve_base->vertices()[vbi_tmp]);
    }
    else {
      ls_base = new PGeoLineSegment(
          _curve_offset->vertices()[voi_tmp - 1],
          _curve_offset->vertices()[voi_tmp]);
    }

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

    _areas.push_back(area);
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

  PGeoLineSegment *ls_base = new PGeoLineSegment(
      _curve_base->vertices()[count - 1],
      _curve_base->vertices()[count]);
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

  if (_closed) {
    area->setNextBoundVertices(first_bound_vertices);
  }
  else {
    bool use_offset_as_base = (
        _base_offset_indices_pairs[_base_offset_indices_pairs.size() - 1][0]
        == _base_offset_indices_pairs[_base_offset_indices_pairs.size() - 2][0]);

    PGeoLineSegment *ls_base_end;
    bool owns_ls_base_end_vertices = false;
    if (!use_offset_as_base) {
      ls_base_end = new PGeoLineSegment(
          _curve_base->vertices()[_curve_base->vertices().size() - 2],
          _curve_base->vertices()[_curve_base->vertices().size() - 1]);
    }
    else {
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
          _curve_offset->vertices()[_curve_base->vertices().size() - 2],
          _curve_offset->vertices()[_curve_base->vertices().size() - 1]);
      int dir = (slayupside == "left") ? 1 : -1;
      ls_base_end = offsetLineSegment(ls_tmp, dir, _layup->getTotalThickness());
      owns_ls_base_end_vertices = true;
            PLOG(debug) << "degenerated case";
            PLOG(debug) << " ls_tmp: " + ls_tmp->printString();
            PLOG(debug) << " ls_base: " + ls_base_end->printString();
      delete ls_tmp;
    }

    bool go_prev = (slayupside == "right");
    for (auto v : buildOpenBoundLayerVertices(
             _curve_base->vertices().back(), ls_base_end, go_prev, bcfg)) {
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
  _areas.push_back(area);
}

void Segment::buildAreas(const BuilderConfig &bcfg) {
  MESSAGE_SCOPE(g_msg);
    PLOG(debug) << "building areas of segment: " + _name;

  std::vector<PDCELVertex *> first_bound_vertices;
  std::vector<PDCELVertex *> prev_bound_vertices =
      buildBeginningBound(first_bound_vertices, bcfg);

  int count = 0;
  createIntermediateAreas(prev_bound_vertices, count, bcfg);

  buildLastArea(prev_bound_vertices, first_bound_vertices, count, bcfg);

  for (auto each_area : _areas) {
    each_area->buildLayers(bcfg);
  }
}
