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

#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>


void set_local_orientation(
  PArea *area,
  const int &base_vector_index, const int &offset_vector_index,
  const bool &use_offset_as_base
  ) {
  Segment *segment = area->segment();
  PGeoLineSegment *ls_base;
  if (!use_offset_as_base) {
    ls_base = new PGeoLineSegment(
      segment->curveBase()->vertices()[base_vector_index - 1], segment->curveBase()->vertices()[base_vector_index]
    );
  } else {
    ls_base = new PGeoLineSegment(
      segment->curveOffset()->vertices()[offset_vector_index - 1], segment->curveOffset()->vertices()[offset_vector_index]
    );
  }

  area->setLineSegmentBase(ls_base);

  if (segment->getMatOrient1() == "baseline") {
    area->setLocaly1(ls_base->toVector());
  }

  if (segment->getMatOrient2() == "baseline") {
    area->setLocaly2(ls_base->toVector());
  }
  else if (segment->getMatOrient2() == "layup") {
    area->setLocaly2(ls_layup->toVector());
  }
}


void divide_interior_wall_by_layup(
  Layup *layup, std::vector<PDCELVertex *> &wall_vertices, PDCEL *dcel
  ) {
  double cumu_thk = 0;  // Cumulative thickness of the layup
  PDCELVertex *v_base = wall_vertices.front();
  PDCELVertex *v_offset = wall_vertices.back();
  PDCELVertex *v_layer_prev = v_base;
  for (int i = 0; i < layup->getLayers().size() - 1; ++i) {
    cumu_thk += layup->getLayers()[i].getLamina()->getThickness() *
                layup->getLayers()[i].getStack();

    // Since the interior wall is straight, then the next dividing point
    // can be found by simply calculating the percentage of the cumulative
    // thickness of the layup
    double norm_thk = cumu_thk / layup->getTotalThickness();
    PDCELVertex *v_layer = new PDCELVertex(
        getParametricPoint(v_base->point(), v_offset->point(), norm_thk));

    PDCELHalfEdge *he_tmp = dcel->findHalfEdge(v_layer_prev, v_offset);
    dcel->splitEdge(he_tmp, v_layer);

    // Insert the new vertex into the vector before the last vertex
    wall_vertices.insert(wall_vertices.begin() + wall_vertices.size() - 1, v_layer);

    v_layer_prev = v_layer;
  }
}


/// @brief Divide the end wall by the layup
/// @param layup Layup of the segment
/// @param ls_base Base line segment of the segment
/// @param slayupside Side of the layup
/// @param end Which end of the segment (0: beginning, 1: ending)
/// @param wall_vertices Wall vertices of the segment
/// @param layer_div_vertices Layer division vertices of the wall
/// @param dcel DCEL of the segment
void divide_end_wall_by_layup(
  Layup *layup, PGeoLineSegment *ls_base, const std::string &slayupside, const int &end,
  std::vector<PDCELVertex *> &wall_vertices,
  std::vector<PDCELVertex *> &layer_div_vertices, PDCEL *dcel,
  Message *pmessage
  ) {
  // The end wall can be arbitrary shape,
  // so the method is to, for each layer,
  // 1. offset the base line segment by the cumulative thickness of the
  //    previous layers
  // 2. find the intersection of the offset line segment with the wall
  // 3. insert the intersection point to the wall_vertices vector and split
  //    the edge
  // 4. add the intersection point to the layer_div_vertices vector

  PDCELVertex *v_base = wall_vertices.front();
  PDCELVertex *v_offset = wall_vertices.back();

  layer_div_vertices.push_back(v_base);

  // PDCELVertex *v_layer_prev = v_base;
  double cumu_thk = 0;  // Cumulative thickness of the layup
  PGeoLineSegment *ls_offset;
  for (int i = 0; i < layup->getLayers().size() - 1; ++i) {
    // For all layers except the last one

    // 1. offset the base line segment by the cumulative thickness of the
    //    previous layers
    cumu_thk += layup->getLayers()[i].getLamina()->getThickness() *
                layup->getLayers()[i].getStack();

    if (slayupside == "left") {
      ls_offset = offsetLineSegment(ls_base, 1, cumu_thk);
    } else {
      ls_offset = offsetLineSegment(ls_base, -1, cumu_thk);
    }

    // 2. find the intersection of the offset line segment with the wall
    int i1, i2;
    double u1, u2;
    bool is_new_1, is_new_2;
    PDCELVertex *v_intersect;
    if (end == 0) {
      // If this is the beginning of the segment,
      // then keep the intersection point closer to the tail of the line segment
      v_intersect = get_polylines_intersection_close_to(
        wall_vertices, ls_offset->vertices(), -1, 1, 0, 0, 1, 0,
        i1, u1, i2, u2, is_new_1, is_new_2, pmessage);
    } else {
      // If this is the ending of the segment,
      // then keep the intersection point closer to the head of the line segment
      v_intersect = get_polylines_intersection_close_to(
        wall_vertices, ls_offset->vertices(), -1, 0, 0, 0, 0, 1,
        i1, u1, i2, u2, is_new_1, is_new_2, pmessage);
    }

    // 3. insert the intersection point to the wall_vertices vector and split
    //    the edge
    if (is_new_1) {
      wall_vertices.insert(wall_vertices.begin() + i1, v_intersect);

      // Split the edge
      // Find the intersecting half-edge
      PDCELHalfEdge *_he = dcel->findHalfEdge(wall_vertices[i1], wall_vertices[i1 + 1]);
      dcel->splitEdge(_he, v_intersect);

    }

    // 4. add the intersection point to the layer_div_vertices vector
    layer_div_vertices.push_back(v_intersect);

 }
}




/// @brief Build an area of the segment
/// @param[in] segment Segment of the area
/// @param[in] base_vector_index Index of the base vector of the area
/// @param[in] offset_vector_index Index of the offset vector of the area
/// @param[in] slayupside Side of the layup
/// @param[in] end Which end of the segment (0: beginning, 1: ending)
/// @param[in] face Face of the area
/// @param[in] prev_bound_vertices Previous bound vertices of the area
/// @param[in] pmessage Message object for logging
/// @return Pointer to the area
PArea *build_area(
  Segment *segment, const int &base_vector_index, const int &offset_vector_index,
  const std::string &slayupside, const int &end,
  PDCELFace *face, std::vector<PDCELVertex *> prev_bound_vertices,
  Message *pmessage
  ) {
  PDCELVertex *v_base = segment->curveBase()->vertices()[base_vector_index];
  PDCELVertex *v_offset = segment->curveOffset()->vertices()[offset_vector_index];

  // 2.1. Split background face
  PGeoLineSegment *ls_wall = new PGeoLineSegment(v_base, v_offset);
  std::list<PDCELFace *> new_faces = segment->pmodel()->dcel()->splitFace(face, v_base, v_offset);


  // 2.2. Create new area
  PArea *area = new PArea(segment->pmodel(), segment);
  if (segment->getLayupSide() == "left") {
    area->setFace(new_faces.front());
    face = new_faces.back();
  } else if (segment->getLayupSide() == "right") {
    area->setFace(new_faces.back());
    face = new_faces.front();
  }
  area->face()->setName(segment->getName() + "_area_" + std::to_string(segment->areas().size()));


  // 2.3. Calculate local orientation of the area
  set_local_orientation(area, base_vector_index, offset_vector_index, use_offset_as_base);


  // 2.4. Divide the wall according to the layup
  area->setPrevBoundVertices(prev_bound_vertices);

  std::vector<PDCELVertex *> next_wall_vertices{v_base, v_offset};

  if (end == -1) {
    divide_interior_wall_by_layup(
      segment->getLayup(), next_wall_vertices, segment->pmodel()->dcel());
  }
  else {
    // divide_end_wall_by_layup(
    //   segment->getLayup(), ls_base, slayupside, end, prev_bound_vertices,
    //   layer_div_vertices, segment->pmodel()->dcel(), pmessage);
  }

  area->setNextBoundVertices(next_wall_vertices);

  return area;
}




/// @brief Build the areas of the segment
///
/// Build the areas of the segment by splitting the face into areas
/// and then splitting the areas into layers.
/// Things to do:
/// - Build the dividing walls between areas
/// - Based on the layup, split the walls
/// Things to consider:
/// - The segment is closed or open
/// - For open segments, check if the intermediate walls intersect with the first and last walls of the segment
///
/// @param pmessage Message object for logging
void Segment::buildAreas(Message *pmessage) {
  pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("building areas of segment: " + _name);


  PArea *area, *area_prev = nullptr;
  PGeoLineSegment *ls_base, *ls_offset, *ls_layup;
  PDCELHalfEdge *he_tmp, *he_tmp_next;
  PDCELVertex *v_layer, *v_layer_prev, *vb_tmp, *vo_tmp, *v1_tmp, *v2_tmp;
  std::list<PDCELFace *> new_faces;
  std::vector<PDCELVertex *> prev_bound_vertices_tmp, first_bound_vertices, layer_div_vertices;
  std::string name;
  double cumu_thk = 0, norm_thk, u1_tmp, u2_tmp;



  // 1. Build the beginning bound of the first area
  // The goal is to calculate dividing points on the line that create layers

  PLOG(debug) << pmessage->message("1. creating the beginning bound of the first area");

  // pmessage->increaseIndent();
  if (_closed) {
    PLOG(debug) << pmessage->message("closed segment");

    vb_tmp = _curve_base->vertices()[0];
    vo_tmp = _curve_offset->vertices()[0];

    prev_bound_vertices_tmp = {vb_tmp, vo_tmp};

    divide_interior_wall_by_layup(
      _layup, prev_bound_vertices_tmp, _pmodel->dcel());


    // cumu_thk = 0;

    // for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {

    //   cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
    //               _layup->getLayers()[i].getStack();
    //   norm_thk = cumu_thk / _layup->getTotalThickness();

    //   v_layer = new PDCELVertex(
    //       getParametricPoint(vb_tmp->point(), vo_tmp->point(), norm_thk));

    //   // Split the edge
    //   if (i == 0) {
    //     v1_tmp = vb_tmp;
    //   } else {
    //     v1_tmp = v_layer_prev;
    //   }
    //   v2_tmp = vo_tmp;

    //   he_tmp = _pmodel->dcel()->findHalfEdge(v1_tmp, v2_tmp);

    //   _pmodel->dcel()->splitEdge(he_tmp, v_layer);

    //   v_layer_prev = v_layer;

    //   prev_bound_vertices_tmp.push_back(v_layer);
    // }

    first_bound_vertices = prev_bound_vertices_tmp;
  }



  else {
    PLOG(debug) << pmessage->message("open segment");

    vb_tmp = _curve_base->vertices()[0];
    vo_tmp = _curve_offset->vertices()[0];

    PLOG(debug) << pmessage->message("first vertex of the base: ") << vb_tmp;
    PLOG(debug) << pmessage->message("first vertex of the offset: ") << vo_tmp;


    // bool use_offset_as_base = false;
    if (_base_offset_indices_pairs[0][0] == _base_offset_indices_pairs[1][0]) {
      // If the first two vertices of the base curve are the same
      //   then use the offset curve as the new base curve
      PLOG(debug) << pmessage->message("  set the offset curve as the new 'base' curve");
      // use_offset_as_base = true;
      // PLOG(debug) << pmessage->message("degenerated case");
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
        _curve_offset->vertices()[0], _curve_offset->vertices()[1]);
      PLOG(debug) << pmessage->message(" ls_tmp: " + ls_tmp->printString());

      if (slayupside == "left") {
        ls_base = offsetLineSegment(ls_tmp, -1, _layup->getTotalThickness());
      } else {
        ls_base = offsetLineSegment(ls_tmp, 1, _layup->getTotalThickness());
      }
    }

    else {
      ls_base = new PGeoLineSegment(
        _curve_base->vertices()[0], _curve_base->vertices()[1]);
    }

    PLOG(debug) << pmessage->message(" ls_base: ") << ls_base;


    // Divide the wall by the layup
    divide_end_wall_by_layup(
      _layup, ls_base, slayupside, 0, prev_bound_vertices_tmp,
      layer_div_vertices, _pmodel->dcel(), pmessage);



    // v_layer_prev = _curve_base->vertices()[0];




    // // pmessage->increaseIndent();
    // cumu_thk = 0;
    // for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {

    //   PLOG(debug) << pmessage->message("layer ") << i+1;

    //   cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
    //               _layup->getLayers()[i].getStack();

    //   PLOG(debug) << pmessage->message("cumu_thk = ") << cumu_thk;

    //   if (slayupside == "left") {
    //     ls_offset = offsetLineSegment(ls_base, 1, cumu_thk);
    //   } else {
    //     ls_offset = offsetLineSegment(ls_base, -1, cumu_thk);
    //   }

    //   PLOG(debug) << pmessage->message("ls_offset: ") << ls_offset;

    //   // Find the intersecting line segment
    //   // Each time search from the base vertex
    //   he_tmp = _pmodel->dcel()->findHalfEdge(v_layer_prev, _face);
    //   PDCELHalfEdge *he_base = he_tmp;
    //   if (slayupside == "left") {
    //     he_tmp = he_tmp->prev();
    //   }


    //   // pmessage->increaseIndent();
    //   do {

    //     PLOG(debug) << pmessage->message("");
    //     PLOG(debug) << pmessage->message("he_tmp: ") << he_tmp;

    //     // bool not_parallel;
    //     // not_parallel = calcLineIntersection2D(
    //     //   he_tmp->toLineSegment(), ls_offset, u1_tmp, u2_tmp, TOLERANCE);
    //     PDCELVertex *v_intersect;
    //     bool not_parallel = calc_line_intersection_2d(
    //       he_tmp->toLineSegment()->v1(), he_tmp->toLineSegment()->v2(),
    //       ls_offset->v1(), ls_offset->v2(), v_intersect,
    //       u1_tmp, u2_tmp, 1, 1, 1, 1
    //     );

    //     // std::stringstream ss_u1_tmp;
    //     // ss_u1_tmp << u1_tmp;
    //     PLOG(debug) << pmessage->message("u1_tmp = ") << u1_tmp;


    //     if (not_parallel) {
    //       if (u1_tmp >= 0 && u1_tmp <= 1) {
    //         if (fabs(u1_tmp) < config.tol) {
    //           PLOG(debug) << pmessage->message("  case 1: intersect at source");
    //           v_layer = he_tmp->source();
    //         }
    //         else if (fabs(1 - u1_tmp) < config.tol) {
    //           PLOG(debug) << pmessage->message("  case 2: intersect at target");
    //           v_layer = he_tmp->target();
    //         }
    //         else {
    //           PLOG(debug) << pmessage->message("  case 3: intersect current edge");
    //           v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
    //           PLOG(debug) << pmessage->message("  v_layer: " + v_layer->printString());
    //           _pmodel->dcel()->splitEdge(he_tmp, v_layer);
    //         }
    //         break;
    //       }
    //       else if (u1_tmp < 0 && slayupside == "left") {
    //         PLOG(debug) << pmessage->message("  case 4: intersect previous edge");
    //         he_tmp_next = he_tmp->prev();
    //       }
    //       else if (u1_tmp > 1 && slayupside == "right") {
    //         PLOG(debug) << pmessage->message("  case 5: intersect next edge");
    //         he_tmp_next = he_tmp->next();
    //       }
    //     }

    //     else {
    //       // TODO: parallel case
    //       if (slayupside == "left") he_tmp_next = he_tmp->prev();
    //       else if (slayupside == "right") he_tmp_next = he_tmp->next();
    //     }

    //     if (
    //       (slayupside == "left" && he_tmp->source() == _curve_offset->vertices().front()) ||
    //       (slayupside == "right" && he_tmp->target() == _curve_offset->vertices().front())
    //     ) {
    //       PLOG(debug) << pmessage->message("reach the last edge");
    //       break;
    //     }

    //     he_tmp = he_tmp_next;

    //   } while (he_tmp != he_base);
    //   // pmessage->decreaseIndent();



    //   if (v_layer == nullptr) {
    //     PLOG(error) << pmessage->message("cannot find intersection");
    //     break;
    //   }
    //   else {
    //     PLOG(debug) << pmessage->message("v_layer: ") << v_layer;
    //     prev_bound_vertices_tmp.push_back(v_layer);
    //     v_layer_prev = v_layer;
    //   }

    // }
    // pmessage->decreaseIndent();
  }

  // std::cout << "        prev bound vertices:" << std::endl;
  // for (auto v : prev_bound_vertices_tmp) {
  //   std::cout << "        " << v << std::endl;
  // }

  // _offset_vertices_link_to[0] = 0;
  // _offset_vertices_link_to[_offset_vertices_link_to.size() - 1] =
  //     _curve_base->vertices().size() - 1;

  // std::cout << "        _offset_vertices_link_to:" << std::endl;
  // for (auto i : _offset_vertices_link_to) {
  //   std::cout << "        " << i << std::endl;
  // }

  // pmessage->decreaseIndent();





  // 2. Build the ending bound of all areas and create areas except the last one
  // if there are more than one area

  PLOG(debug) << pmessage->message("2. creating areas");

  // pmessage->increaseIndent();
  // 2.1. Find linking base-offset vertices pairs
  //    From the second vertex to the second to the last vertex of the
  //    offset curve
  // 2.2. Split background face
  // 2.3. Create new area
  // 2.4. Split bound according to the layup

  int offset_v_index = 0;
  int count = 0;

  for (auto k = 1; k < _base_offset_indices_pairs.size() - 1; k++) {
    count++;

    PLOG(debug) << pmessage->message("area ") << k;

    int vbi_tmp = _base_offset_indices_pairs[k][0];
    int voi_tmp = _base_offset_indices_pairs[k][1];

    bool use_offset_as_base = false;
    if (vbi_tmp == _base_offset_indices_pairs[k-1][0]) {
      use_offset_as_base = true;
    }

    area = build_area(this, vbi_tmp, voi_tmp, _face, prev_bound_vertices_tmp, pmessage);

    // vb_tmp = _curve_base->vertices()[vbi_tmp];
    // vo_tmp = _curve_offset->vertices()[voi_tmp];
    // PLOG(debug) << pmessage->message("  base vertex: ") << vb_tmp;
    // PLOG(debug) << pmessage->message("  offset vertex: ") << vo_tmp;

    // ls_layup = new PGeoLineSegment(vb_tmp, vo_tmp);

    // // 2.2.
    // // PLOG(debug) << pmessage->message("  2.2");
    // new_faces = _pmodel->dcel()->splitFace(_face, vb_tmp, vo_tmp);

    // // 2.3.
    // // PLOG(debug) << pmessage->message("  2.3");
    // area = new PArea(_pmodel, this);
    // if (slayupside == "left") {
    //   area->setFace(new_faces.front());
    //   _face = new_faces.back();
    // } else if (slayupside == "right") {
    //   area->setFace(new_faces.back());
    //   _face = new_faces.front();
    // }


    // if (!use_offset_as_base) {
    //   ls_base = new PGeoLineSegment(
    //     _curve_base->vertices()[vbi_tmp - 1], _curve_base->vertices()[vbi_tmp]
    //   );
    // }
    // else {
    //   ls_base = new PGeoLineSegment(
    //     _curve_offset->vertices()[voi_tmp - 1], _curve_offset->vertices()[voi_tmp]
    //   );
    // }

    // area->setLineSegmentBase(ls_base);

    // if (_mat_orient_e1 == "baseline") {
    //   area->setLocaly1(ls_base->toVector());
    // }

    // if (_mat_orient_e2 == "baseline") {
    //   area->setLocaly2(ls_base->toVector());
    // }
    // else if (_mat_orient_e2 == "layup") {
    //   area->setLocaly2(ls_layup->toVector());
    // }

    // area->face()->setName(_name + "_area_" + std::to_string(count));
    // area->setPrevBoundVertices(prev_bound_vertices_tmp);

    // // 2.4.
    // // PLOG(debug) << pmessage->message("  2.4");
    // cumu_thk = 0;
    // for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
    //   cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
    //               _layup->getLayers()[i].getStack();
    //   norm_thk = cumu_thk / _layup->getTotalThickness();
    //   // v_layer = ls_tt->getParametricVertex(norm_thk);
    //   v_layer = new PDCELVertex(
    //       getParametricPoint(vb_tmp->point(), vo_tmp->point(), norm_thk));

    //   // Split the edge
    //   if (i == 0) {
    //     v1_tmp = vb_tmp;
    //   } else {
    //     v1_tmp = v_layer_prev;
    //   }
    //   v2_tmp = vo_tmp;

    //   he_tmp = _pmodel->dcel()->findHalfEdge(v1_tmp, v2_tmp);
    //   // std::cout << "        half edge he_tmp: " << he_tmp << std::endl;
    //   _pmodel->dcel()->splitEdge(he_tmp, v_layer);

    //   area->addNextBoundVertex(v_layer);

    //   v_layer_prev = v_layer;
    // }

    _areas.push_back(area);
    // area_prev = area;
    prev_bound_vertices_tmp = area->nextBoundVertices();

  }

  // pmessage->decreaseIndent();





  // 3. Build the last or the only one area

  PLOG(debug) << pmessage->message("3. creating the last area");

  // pmessage->increaseIndent();

  count++;

  area = new PArea(_pmodel, this);
  area->face()->setName(_name + "_area_" + std::to_string(_areas.size()));

  // ls_base = new PGeoLineSegment(_curve_base->vertices()[count - 1],
  //                               _curve_base->vertices()[count]);
  // area->setLineSegmentBase(ls_base);

  area->setFace(_face);


  // ls_layup = new PGeoLineSegment(_curve_base->vertices().back(),
  //                               _curve_offset->vertices().back());

  // // Set y2 of the local layer orientation
  // // as the direction of the current line segment of baseline
  // if (_mat_orient_e1 == "baseline") {
  //   area->setLocaly1(ls_base->toVector());
  // }

  // if (_mat_orient_e2 == "baseline") {
  //   area->setLocaly2(ls_base->toVector());
  // }
  // else if (_mat_orient_e2 == "layup") {
  //   area->setLocaly2(ls_layup->toVector());
  // }



  area->setPrevBoundVertices(prev_bound_vertices_tmp);

  // Calculate the next_bound_vertices
  // std::cout << "[debug] calculating vertices on the next bound" << std::endl;
  if (_closed) {
    // If the segment is closed, the next bound vertices are the first bound vertices
    area->setNextBoundVertices(first_bound_vertices);
  }

  else {

    // Find the base line segment
    bool use_offset_as_base = false;
    if (
      _base_offset_indices_pairs[_base_offset_indices_pairs.size()-1][0]
      == _base_offset_indices_pairs[_base_offset_indices_pairs.size()-2][0]) {
      // Degenerated case
      // The base line segment is a point
      // Use the offset line segment as the base line segment
      use_offset_as_base = true;
    }

    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
        _curve_base->vertices()[_curve_base->vertices().size() - 2],
        _curve_base->vertices()[_curve_base->vertices().size() - 1]);
    }
    else {
      PLOG(debug) << pmessage->message("degenerated case");
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
        _curve_offset->vertices()[_curve_base->vertices().size() - 2],
        _curve_offset->vertices()[_curve_base->vertices().size() - 1]);
      if (slayupside == "left") {
        ls_base = offsetLineSegment(ls_tmp, -1, _layup->getTotalThickness());
      } else {
        ls_base = offsetLineSegment(ls_tmp, 1, _layup->getTotalThickness());
      }
      PLOG(debug) << pmessage->message(" ls_tmp: ") << ls_tmp;
      PLOG(debug) << pmessage->message(" ls_base: ") << ls_base;
    }

    // Calculate and set the local orientation
    set_local_orientation(area, count - 1, count, use_offset_as_base);


    // Divide the end wall by the layup
    divide_end_wall_by_layup(
      _layup, ls_base, slayupside, 1, _next_bound_vertices,
      layer_div_vertices, _pmodel->dcel(), pmessage);

    area->setNextBoundVertices(layer_div_vertices);

    // v_layer_prev = _curve_base->vertices().back();


    // cumu_thk = 0;
    // // pmessage->increaseIndent();
    // for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {

    //   PLOG(debug) << pmessage->message("layer ") << i+1;

    //   // Create offset of the base curve
    //   cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
    //               _layup->getLayers()[i].getStack();

    //   if (slayupside == "left") {
    //     ls_offset = offsetLineSegment(ls_base, 1, cumu_thk);
    //   } else {
    //     ls_offset = offsetLineSegment(ls_base, -1, cumu_thk);
    //   }

    //   PLOG(debug) << pmessage->message("ls_offset: ") << ls_offset;

    //   // Find the intersecting line segment
    //   // v_layer = nullptr;
    //   // Each time search from the base vertex
    //   he_tmp = _pmodel->dcel()->findHalfEdge(v_layer_prev, _face);
    //   PDCELHalfEdge *he_base = he_tmp;
    //   if (slayupside == "right") {
    //     he_tmp = he_tmp->prev();
    //   }


    //   // pmessage->increaseIndent();
    //   do {

    //     // double u_tmp;
    //     // u_tmp = he_tmp->toLineSegment()->getParametricLocation(v_layer);
    //     PLOG(debug) << pmessage->message("");
    //     PLOG(debug) << pmessage->message("he_tmp: ") << he_tmp;

    //     // bool not_parallel;

    //     // not_parallel = calcLineIntersection2D(
    //     //   he_tmp->toLineSegment(), ls_offset, u1_tmp, u2_tmp, TOLERANCE);
    //     PDCELVertex *v_intersect;
    //     bool not_parallel = calc_line_intersection_2d(
    //       he_tmp->toLineSegment()->v1(), he_tmp->toLineSegment()->v2(),
    //       ls_offset->v1(), ls_offset->v2(), v_intersect,
    //       u1_tmp, u2_tmp, 1, 1, 1, 1
    //     );

    //     std::stringstream ss_u1_tmp;
    //     ss_u1_tmp << u1_tmp;
    //     PLOG(debug) << pmessage->message("u1_tmp = ") << u1_tmp;


    //     if (not_parallel) {
    //       if (fabs(u1_tmp) < config.tol) {
    //         PLOG(debug) << pmessage->message("  case 1: intersect at source");
    //         v_layer = he_tmp->source();
    //         break;
    //       }
    //       else if (fabs(1 - u1_tmp) < config.tol) {
    //         PLOG(debug) << pmessage->message("  case 2: intersect at target");
    //         v_layer = he_tmp->target();
    //         break;
    //       }
    //       else if (u1_tmp < 0 && slayupside == "right") {
    //         PLOG(debug) << pmessage->message("  case 3: intersect previous edge");
    //         he_tmp_next = he_tmp->prev();
    //       }
    //       else if (u1_tmp > 1 && slayupside == "left") {
    //         PLOG(debug) << pmessage->message("  case 4: intersect next edge");
    //         he_tmp_next = he_tmp->next();
    //       }
    //       else if (u1_tmp > 0 && u1_tmp < 1) {
    //         PLOG(debug) << pmessage->message("  case 5: intersect current edge");
    //         v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
    //         _pmodel->dcel()->splitEdge(he_tmp, v_layer);
    //         break;
    //       }
    //     }

    //     else {
    //       // TODO: parallel case
    //       if (slayupside == "right") he_tmp_next = he_tmp->prev();
    //       else if (slayupside == "left") he_tmp_next = he_tmp->next();
    //     }

    //     if (
    //       (slayupside == "right" && he_tmp->source() == _curve_offset->vertices().front()) ||
    //       (slayupside == "left" && he_tmp->target() == _curve_offset->vertices().front())
    //     ) {
    //       PLOG(debug) << pmessage->message("reach the last edge");
    //       break;
    //     }

    //     he_tmp = he_tmp_next;

    //   } while (he_tmp != he_base);
    //   // pmessage->decreaseIndent();


    //   if (v_layer == nullptr) {
    //     PLOG(error) << pmessage->message("cannot find intersection");
    //     break;
    //   }
    //   else {
    //     PLOG(debug) << pmessage->message("v_layer: " + v_layer->printString());
    //     area->addNextBoundVertex(v_layer);
    //     v_layer_prev = v_layer;
    //   }

    // }
    // pmessage->decreaseIndent();

  }

  // std::cout << "        next bound vertices:" << std::endl;
  // for (auto v : area->nextBoundVertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  _areas.push_back(area);

  // pmessage->decreaseIndent();

  // Slice layers
  for (auto area : _areas) {
    area->buildLayers(pmessage);
  }

  pmessage->decreaseIndent();
}

