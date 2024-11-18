#include "PComponent.hpp"

#include "Material.hpp"
#include "PDCEL.hpp"
#include "PGeoClasses.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>


void PComponent::buildFilling(Message *pmessage) {

  if (!_fill_baseline_groups.empty()) {

    std::list<Baseline *> bl_closed;
    std::list<Baseline *> bl_open;

    // Join baselines
    Baseline *bl;
    for (auto blg : _fill_baseline_groups) {
      bl = joinCurves(blg);

      if (_fill_ref_baseline == blg.front()) {
        _fill_ref_baseline = bl;
      }

      if (bl->vertices().front() == bl->vertices().back()) {
        bl_closed.push_back(bl);
      } else {
        bl_open.push_back(bl);
      }
    }

    // std::cout << "\ncurve bl\n";
    // for (auto k = 0; k < bl->vertices().size(); k++) {
    //   std::cout << k << ": " << bl->vertices()[k] << std::endl;
    // }

    // Trim/Extend ends for each open baseline
    for (auto bl : bl_open) {
      double u1_head, u2_head, u1_tail, u2_tail, u1_tmp, u2_tmp;
      int ls_i_head = -1, ls_i_tmp;
      std::size_t ls_i_tail = bl->vertices().size();
      u1_head = -INF;
      u1_tail = INF;

      std::vector<PDCELVertex *> tmp_vertices;
      PDCELHalfEdge *he_tool_head, *he_tool_tail, *he;
      PDCELHalfEdgeLoop *hel_tool_head, *hel_tool_tail;

      for (auto hel : _pmodel->dcel()->halfedgeloops()) {
        if (!hel->keep()) {
          // he = findCurvesIntersection(bl, hel, 0, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE);
          tmp_vertices = {bl->vertices()[0], bl->vertices()[1]};
          he = findCurvesIntersection(tmp_vertices, hel, 0, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE, pmessage);
          if (he != nullptr) {
            if (
              (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1_head)  // before the first vertex
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp > ls_i_head)  // inner line segment
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp == ls_i_head && u1_tmp > u1_head) // same line segment but inner u
            ) {
              u1_head = u1_tmp;
              u2_head = u2_tmp;
              he_tool_head = he;
              hel_tool_head = hel;
            }
          }

          // he = findCurvesIntersection(bl, hel, 1, ls_i, u1_tmp, u2_tmp, TOLERANCE);
          tmp_vertices.clear();
          tmp_vertices = {bl->vertices()[bl->vertices().size() - 2], bl->vertices()[bl->vertices().size() - 1]};
          he = findCurvesIntersection(tmp_vertices, hel, 1, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE, pmessage);
          if (he != nullptr) {
            if (
              ((ls_i_tmp == tmp_vertices.size() - 1) && u1_tmp > 1 && u1_tmp < u1_tail)  // after the first vertex
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp < ls_i_tail)  // inner line segment
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp == ls_i_tail && u1_tmp < u1_tail) // same line segment but inner u
              ) {
              u1_tail = u1_tmp;
              u2_tail = u2_tmp;
              he_tool_tail = he;
              hel_tool_tail = hel;
            }
          }
        }
      }

      // std::cout << "        u1_head = " << u1_head << std::endl;
      // std::cout << "        he_tool_head = " << he_tool_head << std::endl;
      // std::cout << "        u1_tail = " << u1_tail << std::endl;
      // std::cout << "        he_tool_tail = " << he_tool_tail << std::endl;

      PDCELVertex *vnew;
      PGeoLineSegment *ls;

      if (u2_head == 0) {
        vnew = he_tool_head->source();
      } else if (u2_head == 1) {
        vnew = he_tool_head->target();
      } else {
        ls = new PGeoLineSegment(he_tool_head->source(),
                                  he_tool_head->target());
        vnew = ls->getParametricVertex(u2_head);
        _pmodel->dcel()->splitEdge(he_tool_head, vnew);
      }
      bl->vertices()[0] = vnew;

      if (u2_tail == 0) {
        vnew = he_tool_tail->source();
      } else if (u2_tail == 1) {
        vnew = he_tool_tail->target();
      } else {
        ls = new PGeoLineSegment(he_tool_tail->source(),
                                  he_tool_tail->target());
        vnew = ls->getParametricVertex(u2_tail);
        _pmodel->dcel()->splitEdge(he_tool_tail, vnew);
      }
      bl->vertices()[bl->vertices().size() - 1] = vnew;

      // std::cout << "\ncurve bl\n";
      // for (auto k = 0; k < bl->vertices().size(); k++) {
      //   std::cout << k << ": " << bl->vertices()[k] << std::endl;
      // }

      // std::cout << "        curve _fill_ref_baseline:" << std::endl;
      // for (auto v : _fill_ref_baseline->vertices()) {
      //   // std::cout << "        " << v << std::endl;
      //   v->printWithAddress();
      // }

      _pmodel->dcel()->addEdgesFromCurve(bl);
    }

    for (auto bl : bl_closed) {
      _pmodel->dcel()->addEdgesFromCurve(bl);
    }

    _pmodel->dcel()->removeTempLoops();
    _pmodel->dcel()->createTempLoops();
    _pmodel->dcel()->linkHalfEdgeLoops();

    // _pmodel->dcel()->print_dcel();
  }




  PDCELHalfEdgeLoop *hel_out;
  if (_fill_location != nullptr) {
    // The filling area is defined by a point The half edge loop
    // has been already created

    // Find the half edge loop enclosing the point
    _pmodel->dcel()->addVertex(_fill_location);
    hel_out = _pmodel->dcel()->findEnclosingLoop(_fill_location);
    // std::cout << "[debug] half edge loop hel_out:" << std::endl;
    // hel_out->print();
    _pmodel->dcel()->removeVertex(_fill_location);
  }

  else {
    // The filling area is defined by the side of some baseline
    PDCELHalfEdge *he;
    he = _pmodel->dcel()->findHalfEdge(_fill_ref_baseline->vertices()[0],
                                        _fill_ref_baseline->vertices()[1]);

    // std::cout << "        half edge he:" << he << std::endl;

    if (_fill_side == -1) {
      he = he->twin();
    }

    // Find the outer boundary
    hel_out = he->loop();
    while (hel_out->adjacentLoop() != nullptr) {
      hel_out = hel_out->adjacentLoop();
    }
  }




  // Keep the loop and create a new face
  if (hel_out == _pmodel->dcel()->halfedgeloops().front()) {
    // The location is outside the shape
    // Raise the exception
  }
  else {
    _fill_face = _pmodel->dcel()->addFace(hel_out);
    _fill_face->setName(_name + "_fill_face");
    _fill_face->setMaterial(_fill_material);
    hel_out->setKeep(true);
    hel_out->setFace(_fill_face);

    LayerType *lt = _pmodel->getLayerTypeByMaterialAngle(_fill_material, _fill_theta3);
    _fill_face->setLayerType(lt);
    _fill_face->setTheta1(_fill_theta1);

    // Update all corresponding inner boundaries, if there are any
    _pmodel->dcel()->linkHalfEdgeLoops();
    for (auto heli : _pmodel->dcel()->halfedgeloops()) {
      if (!heli->keep()) {
        // heli->print();
        PDCELHalfEdgeLoop *helj = heli;
        while (helj->adjacentLoop() != nullptr) {
          // helj->print();
          helj = helj->adjacentLoop();
        }
        if (helj == hel_out) {
          heli->setKeep(true);
          heli->setFace(_fill_face);
          _fill_face->addInnerComponent(heli->incidentEdge());
        }
      }
    }
    // _fill_face->print();
  }




  // Set local mesh size
  if (_mesh_size != -1) {
    // std::cout << _mesh_size << std::endl;
    _fill_face->setMeshSize(_mesh_size);
    for (auto v : _embedded_vertices) {
      _fill_face->addEmbeddedVertex(v);
    }
  }


}
