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


void Segment::buildAreas(Message *pmessage) {
  pmessage->increaseIndent();
  // std::cout << std::endl;
  // if (config.debug) {
  //   // std::cout << "[debug] building segment areas: " << _name << std::endl;
  //   // fprintf(config.fdeb, "- building segment areas: %s\n", _name.c_str());
  //   pmessage->print(1, "building segment areas: " + _name);
  // }
  PLOG(debug) << pmessage->message("building areas of segment: " + _name);
  // pmessage->print(9, "building segment areas: " + _name);

  // std::cout << "        segment original face:" << std::endl;
  // _face->print();
  // if (config.debug) {
  //   // fprintf(config.fdeb, "        segment original face:\n");
  //   // writeFace(config.fdeb, _face);
  // }





  PArea *area, *area_prev = nullptr;
  PGeoLineSegment *ls, *ls_base, *ls_tt, *ls_offset, *ls_layup;
  PDCELHalfEdge *he, *he_tmp, *he_tmp_prev, *he_tmp_next;
  PDCELVertex *v_layer, *v_layer_prev, *vb_tmp, *vo_tmp, *v1_tmp, *v2_tmp;
  std::list<PDCELFace *> new_faces;
  std::vector<PDCELVertex *> prev_bound_vertices_tmp, first_bound_vertices;
  std::string name;
  double cumu_thk = 0, norm_thk, u_tmp, u1_tmp, u2_tmp;

  // _curve_base->print(pmessage, 9);





  // 1. Build the beginning bound of the first area
  // The goal is to calculate dividing points on the line that create layers

  PLOG(debug) << pmessage->message("1. creating the beginning bound of the first area");

  pmessage->increaseIndent();
  if (_closed) {
    PLOG(debug) << pmessage->message("closed segment");

    vb_tmp = _curve_base->vertices()[0];
    vo_tmp = _curve_offset->vertices()[0];
    cumu_thk = 0;
    // ls_tt = new PGeoLineSegment(_curve_base->vertices()[0],
    //                             _curve_offset->vertices()[0]);
    for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
      // std::cout << "        layer: " << i << std::endl;
      cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                  _layup->getLayers()[i].getStack();
      norm_thk = cumu_thk / _layup->getTotalThickness();
      // v_layer = ls_tt->getParametricVertex(norm_thk);
      v_layer = new PDCELVertex(
          getParametricPoint(vb_tmp->point(), vo_tmp->point(), norm_thk));

      // Split the edge
      if (i == 0) {
        v1_tmp = vb_tmp;
      } else {
        v1_tmp = v_layer_prev;
      }
      v2_tmp = vo_tmp;

      he_tmp = _pmodel->dcel()->findHalfEdge(v1_tmp, v2_tmp);
      // std::cout << "        half edge he_tmp: " << he_tmp << std::endl;
      _pmodel->dcel()->splitEdge(he_tmp, v_layer);

      v_layer_prev = v_layer;

      prev_bound_vertices_tmp.push_back(v_layer);
    }
    first_bound_vertices = prev_bound_vertices_tmp;
  }



  else {
    PLOG(debug) << pmessage->message("open segment");

    vb_tmp = _curve_base->vertices()[0];
    vo_tmp = _curve_offset->vertices()[0];
    // std::cout << "vb_tmp = " << vb_tmp << ", vo_tmp = " << vo_tmp << std::endl;

    PLOG(debug) << pmessage->message("first vertex of the base: " + vb_tmp->printString());
    PLOG(debug) << pmessage->message("first vertex of the offset: " + vo_tmp->printString());


    bool use_offset_as_base = false;
    if (_base_offset_indices_pairs[0][0] == _base_offset_indices_pairs[1][0]) {
      use_offset_as_base = true;
    }

    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(_curve_base->vertices()[0],
                                    _curve_base->vertices()[1]);
    }
    else {
      PLOG(debug) << pmessage->message("degenerated case");
      PGeoLineSegment *ls_tmp = new PGeoLineSegment(
        _curve_offset->vertices()[0], _curve_offset->vertices()[1]);
      if (slayupside == "left") {
        ls_base = offsetLineSegment(ls_tmp, -1, _layup->getTotalThickness());
      } else {
        ls_base = offsetLineSegment(ls_tmp, 1, _layup->getTotalThickness());
      }
      PLOG(debug) << pmessage->message(" ls_tmp: " + ls_tmp->printString());
      PLOG(debug) << pmessage->message(" ls_base: " + ls_base->printString());
    }

    v_layer_prev = _curve_base->vertices()[0];




    pmessage->increaseIndent();
    cumu_thk = 0;
    for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {

      PLOG(debug) << pmessage->message("layer " + std::to_string(i+1));

      cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                  _layup->getLayers()[i].getStack();

      std::stringstream ss_cumu_thk_tmp;
      ss_cumu_thk_tmp << cumu_thk;
      PLOG(debug) << pmessage->message("cumu_thk = " + ss_cumu_thk_tmp.str());

      if (slayupside == "left") {
        ls_offset = offsetLineSegment(ls_base, 1, cumu_thk);
      } else {
        ls_offset = offsetLineSegment(ls_base, -1, cumu_thk);
      }

      PLOG(debug) << pmessage->message("ls_offset: " + ls_offset->printString());

      // Find the intersecting line segment
      // v_layer = nullptr;
      // Each time search from the base vertex
      // std::cout << "        v_layer_prev = " << v_layer_prev << std::endl;
      he_tmp = _pmodel->dcel()->findHalfEdge(v_layer_prev, _face);
      PDCELHalfEdge *he_base = he_tmp;
      if (slayupside == "left") {
        he_tmp = he_tmp->prev();
      }


      pmessage->increaseIndent();
      do {

        // double u_tmp;
        // u_tmp = he_tmp->toLineSegment()->getParametricLocation(v_layer);
        PLOG(debug) << pmessage->message("");
        PLOG(debug) << pmessage->message("he_tmp: " + he_tmp->printString());

        bool not_parallel;
        not_parallel = calcLineIntersection2D(he_tmp->toLineSegment(),
                                              ls_offset, u1_tmp, u2_tmp);
        // std::cout << "        not_parallel = " << not_parallel << std::endl;
        // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
        std::stringstream ss_u1_tmp;
        ss_u1_tmp << u1_tmp;
        PLOG(debug) << pmessage->message("u1_tmp = " + ss_u1_tmp.str());


        if (not_parallel) {
          if (u1_tmp >= 0 && u1_tmp <= 1) {
            if (fabs(u1_tmp) < config.tol) {
              PLOG(debug) << pmessage->message("  case 1: intersect at source");
              v_layer = he_tmp->source();
            }
            else if (fabs(1 - u1_tmp) < config.tol) {
              PLOG(debug) << pmessage->message("  case 2: intersect at target");
              v_layer = he_tmp->target();
            }
            else {
              PLOG(debug) << pmessage->message("  case 3: intersect current edge");
              v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
              PLOG(debug) << pmessage->message("  v_layer: " + v_layer->printString());
              _pmodel->dcel()->splitEdge(he_tmp, v_layer);
            }
            break;
          }
          else if (u1_tmp < 0 && slayupside == "left") {
            PLOG(debug) << pmessage->message("  case 4: intersect previous edge");
            he_tmp_next = he_tmp->prev();
          }
          else if (u1_tmp > 1 && slayupside == "right") {
            PLOG(debug) << pmessage->message("  case 5: intersect next edge");
            he_tmp_next = he_tmp->next();
          }
        }

        else {
          // TODO: parallel case
          if (slayupside == "left") he_tmp_next = he_tmp->prev();
          else if (slayupside == "right") he_tmp_next = he_tmp->next();
        }

        if (
          (slayupside == "left" && he_tmp->source() == _curve_offset->vertices().front()) ||
          (slayupside == "right" && he_tmp->target() == _curve_offset->vertices().front())
        ) {
          PLOG(debug) << pmessage->message("reach the last edge");
          break;
        }

        he_tmp = he_tmp_next;

        // if (slayupside == "left") {
        //   if (not_parallel) {
        //     // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
        //     if (fabs(u1_tmp) < TOLERANCE) {
        //       // std::cout << "        case 1" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 1");
        //       v_layer = he_tmp->source();
        //       break;
        //     }
        //     if (u1_tmp < 0 || u1_tmp > 1) {
        //       // std::cout << "        case 2" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 2");
        //       he_tmp = he_tmp->prev();
        //       continue;
        //     }
        //     if (u1_tmp > 0 && u1_tmp < 1) {
        //       // The new vertex is in the current half edge
        //       // if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       //   v_layer = he_tmp->target();
        //       // } else {
        //       // std::cout << "        case 3" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 3");
        //       v_layer = he_tmp->toLineSegment()->getParametricVertex(u1_tmp);
        //       _pmodel->dcel()->splitEdge(he_tmp, v_layer);
        //       // }
        //       break;
        //     }
        //   } else {
        //     // TODO: parallel case
        //     he_tmp = he_tmp->prev();
        //     continue;
        //   }
        //   if (he_tmp->source() == _curve_offset->vertices().front()) {
        //     break;
        //   }
        // }
        // else {
        //   if (not_parallel) {
        //     // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
        //     if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       // std::cout << "        case 1" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 1");
        //       v_layer = he_tmp->target();
        //       break;
        //     }
        //     if (u1_tmp < 0 || u1_tmp > 1) {
        //       // std::cout << "        case 2" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 2");
        //       he_tmp = he_tmp->next();
        //       continue;
        //     }
        //     if (u1_tmp > 0 && u1_tmp < 1) {
        //       // The new vertex is in the current half edge
        //       // if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       //   v_layer = he_tmp->target();
        //       // } else {
        //       // std::cout << "        case 3" << std::endl;
        //       PLOG(debug) << pmessage->message("  case 3");
        //       v_layer = he_tmp->toLineSegment()->getParametricVertex(u1_tmp);
        //       _pmodel->dcel()->splitEdge(he_tmp, v_layer);
        //       // }
        //       break;
        //     }
        //   } else {
        //     // TODO: parallel case
        //     he_tmp = he_tmp->next();
        //     continue;
        //   }
        //   if (he_tmp->target() == _curve_offset->vertices().front()) {
        //     break;
        //   }
        // }

      } while (he_tmp != he_base);
      pmessage->decreaseIndent();



      if (v_layer == nullptr) {
        PLOG(error) << pmessage->message("cannot find intersection");
        break;
      }
      else {
        PLOG(debug) << pmessage->message("v_layer: " + v_layer->printString());
        prev_bound_vertices_tmp.push_back(v_layer);
        v_layer_prev = v_layer;
      }

    }
    pmessage->decreaseIndent();
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

  pmessage->decreaseIndent();





  // 2. Build the ending bound of all areas and create areas except the last one
  // if there are more than one area

  // std::cout << "\nfind layer vertices on inner bounds\n";
  PLOG(debug) << pmessage->message("2. creating areas");

  pmessage->increaseIndent();
  // 1. Find linking base-offset vertices pairs
  //    From the second vertex to the second to the last vertex of the
  //    offset curve
  // 2. Split background face
  // 3. Create new area
  // 4. Split bound according to the layup

  int offset_v_index = 0;
  int offset_v_linkto, offset_v_linkto_next;
  int ii;
  int count = 0;
  // while (!_inner_bounds.empty()) {
  // while (offset_v_index < _curve_offset->vertices().size()) {
  // for (auto vi = 1; vi < _curve_base->vertices().size() - 1; vi++) {
  for (auto k = 1; k < _base_offset_indices_pairs.size() - 1; k++) {
    PLOG(debug) << pmessage->message("area " + std::to_string(k));

    int vbi_tmp = _base_offset_indices_pairs[k][0];
    int voi_tmp = _base_offset_indices_pairs[k][1];

    bool use_offset_as_base = false;
    if (vbi_tmp == _base_offset_indices_pairs[k-1][0]) {
      use_offset_as_base = true;
    }

    // 1.
    // PLOG(debug) << pmessage->message("  2.1");
    // vb_tmp = _curve_base->vertices()[vi];
    // vo_tmp = _curve_offset->vertices()[_offset_indices_base_link_to[vi]];
    vb_tmp = _curve_base->vertices()[vbi_tmp];
    vo_tmp = _curve_offset->vertices()[voi_tmp];
    PLOG(debug) << pmessage->message("  base vertex: " + vb_tmp->printString());
    PLOG(debug) << pmessage->message("  offset vertex: " + vo_tmp->printString());

    ls_layup = new PGeoLineSegment(vb_tmp, vo_tmp);

    // 2.
    // PLOG(debug) << pmessage->message("  2.2");
    new_faces = _pmodel->dcel()->splitFace(_face, vb_tmp, vo_tmp);

    // 3.
    // PLOG(debug) << pmessage->message("  2.3");
    area = new PArea(_pmodel, this);
    count++;
    if (slayupside == "left") {
      area->setFace(new_faces.front());
      _face = new_faces.back();
    } else if (slayupside == "right") {
      area->setFace(new_faces.back());
      _face = new_faces.front();
    }


    if (!use_offset_as_base) {
      ls_base = new PGeoLineSegment(
        // _curve_base->vertices()[vi - 1], _curve_base->vertices()[vi]
        _curve_base->vertices()[vbi_tmp - 1], _curve_base->vertices()[vbi_tmp]
      );
    }
    else {
      ls_base = new PGeoLineSegment(
        _curve_offset->vertices()[voi_tmp - 1], _curve_offset->vertices()[voi_tmp]
      );
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

    area->face()->setName(_name + "_area_" + std::to_string(count));
    area->setPrevBoundVertices(prev_bound_vertices_tmp);

    // 4.
    // PLOG(debug) << pmessage->message("  2.4");
    cumu_thk = 0;
    for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {
      cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                  _layup->getLayers()[i].getStack();
      norm_thk = cumu_thk / _layup->getTotalThickness();
      // v_layer = ls_tt->getParametricVertex(norm_thk);
      v_layer = new PDCELVertex(
          getParametricPoint(vb_tmp->point(), vo_tmp->point(), norm_thk));

      // Split the edge
      if (i == 0) {
        v1_tmp = vb_tmp;
      } else {
        v1_tmp = v_layer_prev;
      }
      v2_tmp = vo_tmp;

      he_tmp = _pmodel->dcel()->findHalfEdge(v1_tmp, v2_tmp);
      // std::cout << "        half edge he_tmp: " << he_tmp << std::endl;
      _pmodel->dcel()->splitEdge(he_tmp, v_layer);

      area->addNextBoundVertex(v_layer);

      v_layer_prev = v_layer;
    }

    _areas.push_back(area);
    area_prev = area;
    prev_bound_vertices_tmp = area->nextBoundVertices();

  }

  pmessage->decreaseIndent();





  // 3. Build the last or the only one area

  PLOG(debug) << pmessage->message("3. creating the last area");

  pmessage->increaseIndent();

  area = new PArea(_pmodel, this);
  count++;
  // std::cout << "        count = " << count << std::endl;
  area->setFace(_face);
  ls_base = new PGeoLineSegment(_curve_base->vertices()[count - 1],
                                _curve_base->vertices()[count]);
  // std::stringstream ss;
  // ss << "ls_base: " << ls_base;
  // pmessage->print(9, ss.str());
  area->setLineSegmentBase(ls_base);

  ls_layup = new PGeoLineSegment(_curve_base->vertices().back(),
                                _curve_offset->vertices().back());

  // Set y2 of the local layer orientation
  // as the direction of the current line segment of baseline
  if (_mat_orient_e1 == "baseline") {
    area->setLocaly1(ls_base->toVector());
  }

  if (_mat_orient_e2 == "baseline") {
    area->setLocaly2(ls_base->toVector());
  }
  else if (_mat_orient_e2 == "layup") {
    area->setLocaly2(ls_layup->toVector());
  }
  // std::cout << "[debug] vector localy2: " << ls_base->toVector() <<
  // std::endl;

  area->face()->setName(_name + "_area_" + std::to_string(count));
  // if (_name == "sgm_18") {
  //   std::cout << "        new area face: " << area->face()->name() <<
  //   std::endl; area->face()->print();
  // }
  // std::cout << "        creating the last area face: " <<
  // area->face()->name()
  //           << std::endl;
  // std::cout << "        new area face: " << area->face()->name() <<
  // std::endl; area->face()->print();
  if (config.debug) {
    // fprintf(config.fdeb, "        new area face:\n");
    // writeFace(config.fdeb, area->face());
  }

  area->setPrevBoundVertices(prev_bound_vertices_tmp);

  // Calculate the next_bound_vertices
  // std::cout << "[debug] calculating vertices on the next bound" << std::endl;
  if (_closed) {
    area->setNextBoundVertices(first_bound_vertices);
  }

  else {

    // vb_tmp = _curve_base->vertices().back();
    // vo_tmp = _curve_offset->vertices().back();

    bool use_offset_as_base = false;
    if (
      _base_offset_indices_pairs[_base_offset_indices_pairs.size()-1][0]
      == _base_offset_indices_pairs[_base_offset_indices_pairs.size()-2][0]) {
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
      PLOG(debug) << pmessage->message(" ls_tmp: " + ls_tmp->printString());
      PLOG(debug) << pmessage->message(" ls_base: " + ls_base->printString());
    }

    v_layer_prev = _curve_base->vertices().back();


    cumu_thk = 0;
    pmessage->increaseIndent();
    for (int i = 0; i < _layup->getLayers().size() - 1; ++i) {

      PLOG(debug) << pmessage->message("layer " + std::to_string(i+1));

      // Create offset of the base curve
      cumu_thk += _layup->getLayers()[i].getLamina()->getThickness() *
                  _layup->getLayers()[i].getStack();

      if (slayupside == "left") {
        ls_offset = offsetLineSegment(ls_base, 1, cumu_thk);
      } else {
        ls_offset = offsetLineSegment(ls_base, -1, cumu_thk);
      }

      PLOG(debug) << pmessage->message("ls_offset: " + ls_offset->printString());

      // Find the intersecting line segment
      // v_layer = nullptr;
      // Each time search from the base vertex
      he_tmp = _pmodel->dcel()->findHalfEdge(v_layer_prev, _face);
      PDCELHalfEdge *he_base = he_tmp;
      if (slayupside == "right") {
        he_tmp = he_tmp->prev();
      }


      pmessage->increaseIndent();
      do {

        // double u_tmp;
        // u_tmp = he_tmp->toLineSegment()->getParametricLocation(v_layer);
        PLOG(debug) << pmessage->message("");
        PLOG(debug) << pmessage->message("he_tmp: " + he_tmp->printString());

        bool not_parallel;

        not_parallel = calcLineIntersection2D(he_tmp->toLineSegment(),
                                              ls_offset, u1_tmp, u2_tmp);

        std::stringstream ss_u1_tmp;
        ss_u1_tmp << u1_tmp;
        PLOG(debug) << pmessage->message("u1_tmp = " + ss_u1_tmp.str());


        if (not_parallel) {
          if (fabs(u1_tmp) < config.tol) {
            PLOG(debug) << pmessage->message("  case 1: intersect at source");
            v_layer = he_tmp->source();
            break;
          }
          else if (fabs(1 - u1_tmp) < config.tol) {
            PLOG(debug) << pmessage->message("  case 2: intersect at target");
            v_layer = he_tmp->target();
            break;
          }
          else if (u1_tmp < 0 && slayupside == "right") {
            PLOG(debug) << pmessage->message("  case 3: intersect previous edge");
            he_tmp_next = he_tmp->prev();
          }
          else if (u1_tmp > 1 && slayupside == "left") {
            PLOG(debug) << pmessage->message("  case 4: intersect next edge");
            he_tmp_next = he_tmp->next();
          }
          else if (u1_tmp > 0 && u1_tmp < 1) {
            PLOG(debug) << pmessage->message("  case 5: intersect current edge");
            v_layer = he_tmp->toLineSegment()->getParametricVertex1(u1_tmp);
            _pmodel->dcel()->splitEdge(he_tmp, v_layer);
            break;
          }
        }

        else {
          // TODO: parallel case
          if (slayupside == "right") he_tmp_next = he_tmp->prev();
          else if (slayupside == "left") he_tmp_next = he_tmp->next();
        }

        if (
          (slayupside == "right" && he_tmp->source() == _curve_offset->vertices().front()) ||
          (slayupside == "left" && he_tmp->target() == _curve_offset->vertices().front())
        ) {
          PLOG(debug) << pmessage->message("reach the last edge");
          break;
        }

        he_tmp = he_tmp_next;


        // if (slayupside == "right") {
        //   if (not_parallel) {
        //     // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
        //     if (fabs(u1_tmp) < TOLERANCE) {
        //       // std::cout << "        case 1" << std::endl;
        //       v_layer = he_tmp->source();
        //       break;
        //     }
        //     if (u1_tmp < 0 || u1_tmp > 1) {
        //       // std::cout << "        case 2" << std::endl;
        //       he_tmp = he_tmp->prev();
        //       continue;
        //     }
        //     if (u1_tmp > 0 && u1_tmp < 1) {
        //       // The new vertex is in the current half edge
        //       // if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       //   v_layer = he_tmp->target();
        //       // } else {
        //       // std::cout << "        case 3" << std::endl;
        //       v_layer = he_tmp->toLineSegment()->getParametricVertex(u1_tmp);
        //       _pmodel->dcel()->splitEdge(he_tmp, v_layer);
        //       // }
        //       break;
        //     }
        //   } else {
        //     // TODO: parallel case
        //     he_tmp = he_tmp->prev();
        //     continue;
        //   }
        //   if (he_tmp->source() == _tail_vertex_offset) {
        //     break;
        //   }
        // }
        // else {
        //   if (not_parallel) {
        //     // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
        //     if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       // std::cout << "        case 1" << std::endl;
        //       v_layer = he_tmp->target();
        //       break;
        //     }
        //     if (u1_tmp < 0 || u1_tmp > 1) {
        //       // std::cout << "        case 2" << std::endl;
        //       he_tmp = he_tmp->next();
        //       continue;
        //     }
        //     if (u1_tmp > 0 && u1_tmp < 1) {
        //       // The new vertex is in the current half edge
        //       // if (fabs(1 - u1_tmp) < TOLERANCE) {
        //       //   v_layer = he_tmp->target();
        //       // } else {
        //       // std::cout << "        case 3" << std::endl;
        //       v_layer = he_tmp->toLineSegment()->getParametricVertex(u1_tmp);
        //       _pmodel->dcel()->splitEdge(he_tmp, v_layer);
        //       // }
        //       break;
        //     }
        //   } else {
        //     // TODO: parallel case
        //     he_tmp = he_tmp->next();
        //     continue;
        //   }
        //   if (he_tmp->target() == _tail_vertex_offset) {
        //     break;
        //   }
        // }
      } while (he_tmp != he_base);
      pmessage->decreaseIndent();


      if (v_layer == nullptr) {
        PLOG(error) << pmessage->message("cannot find intersection");
        break;
      }
      else {
        PLOG(debug) << pmessage->message("v_layer: " + v_layer->printString());
        area->addNextBoundVertex(v_layer);
        v_layer_prev = v_layer;
      }

    }
    pmessage->decreaseIndent();

  }

  // std::cout << "        next bound vertices:" << std::endl;
  // for (auto v : area->nextBoundVertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  _areas.push_back(area);

  pmessage->decreaseIndent();

  // Slice layers
  for (auto area : _areas) {
    area->buildLayers(pmessage);
  }

  pmessage->decreaseIndent();
}

