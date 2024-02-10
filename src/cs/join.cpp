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


void PComponent::joinSegments(Segment *s, int e, PDCELVertex *v, Message *pmessage) {
  pmessage->increaseIndent();
  // if (config.debug) {
  //   std::cout << "[debug] making segment end: " << s->getName() << " " << e
  //             << std::endl;
  //   std::stringstream ss;
  //   ss << "making segment end: " << s->getName() << " " << e;
  //   pmessage->print(9, ss.str());
  // }
  PLOG(debug) << pmessage->message("making segment end: " + s->getName() + " " + std::to_string(e));
  // std::cout << std::endl;
  // std::cout << "\n[debug] function: joinSegments " << s->getName() << " " << e
  //           << std::endl;

  if (_dependencies.empty()) {
    createSegmentFreeEnd(s, e);
    return;
  }
  else {

    if (e == s->free()) {
      createSegmentFreeEnd(s, e);
      return;
    }
    else {

      PDCELHalfEdge *he_tool, *he;
      int ls_i, ls_i_prev, ls_i_tmp;
      double u1, u2, u1_tmp, u2_tmp;
      PDCELVertex *v_new;

      if (e == 0) {
        u1 = -INF;
        ls_i_prev = -1;
      }
      else if (e == 1) {
        u1 = INF;
        ls_i_prev = s->curveBase()->vertices().size();
      }



      // 1. Use the reference point of the component to find the outer
      // half edge loop that enclosing this point
      // 2. Find all inner half edge loops inside the outer half edge loop
      // 3. For each found loop, calculate intersections and
      //    keep the intersection(s) closest to the reference point

      if (_ref_vertex == nullptr) {
        _ref_vertex = _segments[0]->curveBase()->refVertex();
        if (_ref_vertex == nullptr) {
          int i = std::floor(_segments[0]->curveBase()->vertices().size() / 2);
          _ref_vertex = _segments[0]->curveBase()->vertices()[i];
        }
      }
      // std::cout << "\n_ref_vertex = " << _ref_vertex << std::endl;
      PLOG(debug) << pmessage->message("ref vertex: " + _ref_vertex->printString());

      bool to_be_removed;
      if (_ref_vertex->dcel() != nullptr) {
        to_be_removed = false;
      }
      else {
        _pmodel->dcel()->addVertex(_ref_vertex);
        to_be_removed = true;
      }




      // 1.
      // std::cout << "\n[debug] step 1\n";
      PLOG(debug) << pmessage->message("step 1: find the outer half edge loop");

      std::vector<PDCELHalfEdgeLoop *> hels;
      PDCELHalfEdgeLoop *tmp_hel_out;
      tmp_hel_out = _pmodel->dcel()->findEnclosingLoop(_ref_vertex);
      // std::cout << "\ntmp_hel_out\n";
      // tmp_hel_out->print();
      // std::cout << "\ntmp_hel_out->incidentEdge(): " << tmp_hel_out->incidentEdge() << std::endl;
      // std::cout << "\ndcel->halfedges.front(): " << _pmodel->dcel()->halfedges().front() << std::endl;
      if (tmp_hel_out != _pmodel->dcel()->halfedgeloops().front()) {
        // Not the first loop with INF size
        hels.push_back(tmp_hel_out);
      }

      // _pmodel->dcel()->print_dcel();




      // 2.
      // std::cout << "\n[debug] step 2\n";
      PLOG(debug) << pmessage->message("step 2: find all inner half edge loops");

      PDCELFace *tmp_face = tmp_hel_out->face();
      // std::cout << "\ntmp_face:\n";
      if (tmp_face != nullptr) {
        // tmp_face->print();
        // Try to update inner loops first
        if (tmp_face->inners().size() == 0) {
          _pmodel->dcel()->linkHalfEdgeLoops();
          for (auto heli : _pmodel->dcel()->halfedgeloops()) {
            if (!heli->keep()) {
              PDCELHalfEdgeLoop *helj = heli;
              while (helj->adjacentLoop() != nullptr) {
                helj = helj->adjacentLoop();
              }
              if (helj == tmp_hel_out) {
                // hels.push_back(helj);
                heli->setFace(tmp_face);
                tmp_face->addInnerComponent(heli->incidentEdge());
              }
            }
          }
        }

        for (auto tmp_he : tmp_face->inners()) {
          hels.push_back(tmp_he->loop());
        }
      }
      else {
        // std::cout << "nullptr\n";
      }

      if (to_be_removed) {
        _pmodel->dcel()->removeVertex(_ref_vertex);
      }

      // delete tmp_face;
      // delete tmp_hel_out;

      if (config.debug) {
        std::cout << "\nhels:\n";
        PLOG(debug) << pmessage->message("found half edge loops");
        for (auto hel : hels) {
          tmp_hel_out->log();
        }
        // std::cout << "\n";
        // hel->print();
      }




      // 3.
      PLOG(debug) << pmessage->message("step 3: calculate intersections");

      // 3.1. For the base curve
      PLOG(debug) << pmessage->message("step 3.1: for the base curve");

      // for (auto hel : _pmodel->dcel()->halfedgeloops()) {
      for (auto hel : hels) {
        if (!hel->keep()) {
          // std::cout << "        half edge loop hel:" << std::endl;
          // hel->print();

          he = findCurvesIntersection(s->curveBase()->vertices(), hel, e, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE, pmessage);
          // he = findCurvesIntersection(tmp_vertices, hel, e, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE);

          // std::cout << "        u1_tmp = " << u1_tmp << std::endl;
          // std::cout << "ls_i_tmp = " << ls_i_tmp << std::endl;
          // pmessage->print(9, "u1_tmp = " + std::to_string(u1_tmp));
          // pmessage->print(9, "u2_tmp = " + std::to_string(u2_tmp));
          // if (he != nullptr && u1_tmp < u1) {
          if (he != nullptr) {
            if (e == 0) {
              if (
                (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1)  // before the first vertex
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i > ls_i_prev)  // inner line segment
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i == ls_i_prev && u1_tmp > u1) // same line segment but inner u
                ) {
                u1 = u1_tmp;
                u2 = u2_tmp;
                he_tool = he;
                ls_i_prev = ls_i_tmp;
                // hel_tool = hel;
              }
            }
            else if (e == 1) {
              if (
                ((ls_i_tmp == s->curveBase()->vertices().size() - 1) && u1_tmp > 1 && u1_tmp < u1)  // after the first vertex
                // ((ls_i_tmp == tmp_vertices.size() - 1) && u1_tmp > 1 && u1_tmp < u1)  // after the first vertex
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i < ls_i_prev)  // inner line segment
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i == ls_i_prev && u1_tmp < u1) // same line segment but inner u
                ) {
                u1 = u1_tmp;
                u2 = u2_tmp;
                he_tool = he;
                ls_i_prev = ls_i_tmp;
                // hel_tool = hel;
              }
            }
          }
        }
      }
      ls_i = ls_i_prev;
      if ((fabs(1 - u1_tmp) <= TOLERANCE) || e == 1) ls_i += 1;

      // std::cout << "final intersection" << std::endl;
      // pmessage->print(9, "u1 = " + std::to_string(u1));
      // pmessage->print(9, "u2 = " + std::to_string(u2));
      // std::cout << "he_tool: " << he_tool << std::endl;
      PLOG(debug) << pmessage->message("  u1 = " + std::to_string(u1));
      PLOG(debug) << pmessage->message("  u2 = " + std::to_string(u2));

      if (fabs(u1) == INF) {
        createSegmentFreeEnd(s, e);
        return;
      }
      else {
        // This end is trimmed by some other components
        if (fabs(u2) <= TOLERANCE) {
          v_new = he_tool->source();
        }
        else if (fabs(1 - u2) <= TOLERANCE) {
          v_new = he_tool->target();
        }
        else {
          v_new = he_tool->toLineSegment()->getParametricVertex(u2);
          _pmodel->dcel()->splitEdge(he_tool, v_new);
          // std::cout << "        half edges of vertex v_new: " << v_new <<
          // std::endl; v_new->printAllLeavingHalfEdges();
        }
      }
      PLOG(debug) << pmessage->message("  v_new = ") << v_new->printString();

      int ls_i_base = ls_i;
      // std::cout << "ls_i = " << ls_i << std::endl;
      // std::cout << "u1 = " << u1 << std::endl;
      // std::cout << "v_new = " << v_new << std::endl;
      PLOG(debug) << pmessage->message("  ls_i = " + std::to_string(ls_i));

      // std::stringstream ss;
      // ss << "v_new: " << v_new;
      // pmessage->print(9, ss.str());

      if (e == 0) {
        for (auto k = 0; k < ls_i; k++) {
          s->curveBase()->vertices().erase(s->curveBase()->vertices().begin());
          s->baseOffsetIndicesLink().erase(s->baseOffsetIndicesLink().begin());
        }

        s->curveBase()->vertices()[0] = v_new;
        // std::cout << "        half edges of vertex
        // s->curveBase()->vertices().front(): " <<
        // s->curveBase()->vertices().front() << std::endl;
        // s->curveBase()->vertices().front()->printAllLeavingHalfEdges();
      }
      else if (e == 1) {
        // ls_i = ls_i + _index_off;
        int n = s->curveBase()->vertices().size();
        for (auto k = ls_i; k < n - 1; k++) {
          s->curveBase()->vertices().pop_back();
          s->baseOffsetIndicesLink().pop_back();
        }
        s->curveBase()->vertices().back() = v_new;
        // std::cout << "        half edges of vertex
        // s->curveBase()->vertices().back(): " <<
        // s->curveBase()->vertices().back() << std::endl;
        // s->curveBase()->vertices().back()->printAllLeavingHalfEdges();
      }





      // 3.2. For the offset curve
      PLOG(debug) << pmessage->message("step 3.2: for the offset curve");

      if (e == 0) {
        u1 = -INF;
        ls_i_prev = -1;
      }
      else if (e == 1) {
        u1 = INF;
        ls_i_prev = s->curveOffset()->vertices().size();
      }


      // for (auto hel : _pmodel->dcel()->halfedgeloops()) {
      for (auto hel : hels) {
        if (!hel->keep()) {

          he = findCurvesIntersection(s->curveOffset()->vertices(), hel, e, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE, pmessage);
          // he = findCurvesIntersection(tmp_vertices, hel, e, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE);

          // std::cout << "ls_i_tmp = " << ls_i_tmp << std::endl;
          // pmessage->print(9, "u1_tmp = " + std::to_string(u1_tmp));
          // pmessage->print(9, "u2_tmp = " + std::to_string(u2_tmp));
          // if (he != nullptr && u1_tmp < u1) {
          if (he != nullptr) {
            PLOG(debug) << pmessage->message("he: ") << he->printString();
            if (e == 0) {
              if (
                (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1)  // before the first vertex
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i > ls_i_prev)  // inner line segment
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i == ls_i_prev && u1_tmp > u1) // same line segment but inner u
                ) {
                u1 = u1_tmp;
                u2 = u2_tmp;
                he_tool = he;
                ls_i_prev = ls_i_tmp;
                // hel_tool = hel;
              }
            }
            else if (e == 1) {
              if (
                ((ls_i_tmp == s->curveOffset()->vertices().size() - 2) && u1_tmp > 1 && u1_tmp < u1)  // after the first vertex
                // ((ls_i_tmp == tmp_vertices.size() - 1) && u1_tmp > 1 && u1_tmp < u1)  // after the first vertex
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i < ls_i_prev)  // inner line segment
                || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i == ls_i_prev && u1_tmp < u1) // same line segment but inner u
                ) {
                u1 = u1_tmp;
                u2 = u2_tmp;
                he_tool = he;
                ls_i_prev = ls_i_tmp;
                // hel_tool = hel;
              }
            }
          }
        }
      }
      ls_i = ls_i_prev;
      if ((fabs(1 - u1_tmp) <= TOLERANCE) || e == 1) ls_i += 1;

      // std::cout << "final intersection" << std::endl;
      // pmessage->print(9, "u1 = " + std::to_string(u1));
      // pmessage->print(9, "u2 = " + std::to_string(u2));
      // std::cout << "he_tool: " << he_tool << std::endl;
      PLOG(debug) << pmessage->message("  u1 = " + std::to_string(u1));
      PLOG(debug) << pmessage->message("  u2 = " + std::to_string(u2));

      // This end is trimmed by some other components
      if (fabs(u2) <= TOLERANCE) {
        v_new = he_tool->source();
      }
      else if (fabs(1 - u2) <= TOLERANCE) {
        v_new = he_tool->target();
      }
      else {
        v_new = he_tool->toLineSegment()->getParametricVertex(u2);
        _pmodel->dcel()->splitEdge(he_tool, v_new);
        // std::cout << "        half edges of vertex v_new: " << v_new <<
        // std::endl; v_new->printAllLeavingHalfEdges();
      }
      PLOG(debug) << pmessage->message("  v_new = ") << v_new->printString();

      int ls_i_offset = ls_i;
      // std::cout << "ls_i = " << ls_i << std::endl;
      // std::cout << "u1 = " << u1 << std::endl;
      // std::cout << "v_new = " << v_new << std::endl;
      PLOG(debug) << pmessage->message("  ls_i = " + std::to_string(ls_i));

      if (e == 0) {
        for (auto k = 0; k < ls_i; k++) {
          s->curveOffset()->vertices().erase(s->curveOffset()->vertices().begin());
        }

        s->curveOffset()->vertices()[0] = v_new;
        s->setHeadVertexOffset(v_new);
      }
      else if (e == 1) {
        // ls_i = ls_i + _index_off;
        int n = s->curveOffset()->vertices().size();
        for (auto k = ls_i; k < n - 1; k++) {
          s->curveOffset()->vertices().pop_back();
        }
        s->curveOffset()->vertices().back() = v_new;
        s->setTailVertexOffset(v_new);
      }



      // Adjust base-offset linking indicies
      // 1. Remove the index pairs is they are in the trim region
      // 2. Renumber the indices (if trim start end)
      // 3. Add the pairs for the trimmed end
      PLOG(debug) << pmessage->message("adjust base-offset linking indices");

      int nv_base = s->curveBase()->vertices().size();
      int nv_offset = s->curveOffset()->vertices().size();
      PLOG(debug) << pmessage->message("  nv_base = " + std::to_string(nv_base));
      PLOG(debug) << pmessage->message("  nv_offset = " + std::to_string(nv_offset));

      if (e == 0) {

        // 1.1: Remove the index pairs if the base vertex index is in the trim region
        while (1) {
          int id_base_tmp = s->baseOffsetIndicesPairs().front()[0];
          if (id_base_tmp > ls_i_base) break;
          s->baseOffsetIndicesPairs().erase(s->baseOffsetIndicesPairs().begin());
        }

        // 1.2: Remove the index pairs if the offset vertex index is in the trim region
        while (1) {
          int id_offset_tmp = s->baseOffsetIndicesPairs().front()[1];
          if (id_offset_tmp > ls_i_offset) break;
          s->baseOffsetIndicesPairs().erase(s->baseOffsetIndicesPairs().begin());
        }

        // 2: Renumber indices
        for (int k = 0; k < s->baseOffsetIndicesPairs().size(); k++) {
          s->baseOffsetIndicesPairs()[k][0] -= ls_i_base;
          s->baseOffsetIndicesPairs()[k][1] -= ls_i_offset;
        }

        // 3: Add the index pairs for the trimmed end
        int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
        PLOG(debug) << pmessage->message("  nv_diff = " + std::to_string(nv_diff));

        std::vector<std::vector<int>>::iterator it_tmp = s->baseOffsetIndicesPairs().begin();
        for (int k = 0; k < nv_diff; k++) {
          if (ls_i_base > ls_i_offset) {
            std::vector<int> id_pair_tmp{0, nv_diff - k};
            it_tmp = s->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
          }
          else if (ls_i_base < ls_i_offset) {
            std::vector<int> id_pair_tmp{nv_diff - k, 0};
            it_tmp = s->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
          }
        }
        std::vector<int> id_pair_tmp{0, 0};
        it_tmp = s->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);



        // Old method
        for (auto k = 0; k < s->curveBase()->vertices().size(); k++) {
          if (k <= ls_i) {
            s->baseOffsetIndicesLink()[k] = 0;
          }
          else {
            s->baseOffsetIndicesLink()[k] -= ls_i;
          }
        }

      }
      else if (e == 1) {

        // 1.1: Remove the index pairs if the base vertex index is in the trim region
        while (1) {
          int id_base_tmp = s->baseOffsetIndicesPairs().back()[0];
          if (id_base_tmp < ls_i_base) break;
          s->baseOffsetIndicesPairs().pop_back();
        }

        // 1.2: Remove the index pairs if the offset vertex index is in the trim region
        while (1) {
          int id_offset_tmp = s->baseOffsetIndicesPairs().back()[1];
          if (id_offset_tmp < ls_i_offset) break;
          s->baseOffsetIndicesPairs().pop_back();
        }

        // 2: Renumber indices
        // for (int k = 0; k < s->baseOffsetIndicesPairs().size(); k++) {
        //   s->baseOffsetIndicesPairs()[k][0] -= ls_i_base;
        //   s->baseOffsetIndicesPairs()[k][1] -= ls_i_offset;
        // }

        // 3: Add the index pairs for the trimmed end
        int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
        PLOG(debug) << pmessage->message("  nv_diff = " + std::to_string(nv_diff));

        // std::vector<std::vector<int>>::iterator it_tmp = s->baseOffsetIndicesPairs().begin();
        int id_base_last_tmp = s->baseOffsetIndicesPairs().back()[0];
        int id_offset_last_tmp = s->baseOffsetIndicesPairs().back()[1];
        for (int k = 0; k < nv_diff; k++) {
          std::vector<int> id_pair_tmp{id_base_last_tmp+1, id_offset_last_tmp+1};
          s->baseOffsetIndicesPairs().push_back(id_pair_tmp);
          if (ls_i_base > ls_i_offset) {
            id_base_last_tmp++;
          }
          else if (ls_i_base < ls_i_offset) {
            id_offset_last_tmp++;
            // std::vector<int> id_pair_tmp{nv_diff - k, 0};
            // it_tmp = s->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
          }
        }
        std::vector<int> id_pair_tmp{id_base_last_tmp+1, id_offset_last_tmp+1};
        s->baseOffsetIndicesPairs().push_back(id_pair_tmp);


        // Old method
        for (auto k = 0; k < s->curveBase()->vertices().size(); k++) {
          if (k >= ls_i) {
            s->baseOffsetIndicesLink()[k] = s->curveOffset()->vertices().size() - 1;
          }
        }

      }

    }
  }

  // std::cout << "\n        baseline s->curveBase -- link to" << std::endl;
  // for (auto k = 0; k < s->curveBase()->vertices().size(); k++) {
  //   std::cout << "        " << s->curveBase()->vertices()[k]
  //   << " -- " << s->baseOffsetIndicesLink()[k] << std::endl;
  // }

  // PLOG(debug) << pmessage->message("base vertex -- offset vertex index");
  // for (auto k = 0; k < s->curveBase()->vertices().size(); k++) {
  //   PLOG(debug) << pmessage->message(
  //     "  " + s->curveBase()->vertices()[k]->printString() + " -- "
  //     + std::to_string(s->baseOffsetIndicesLink()[k])
  //   );
  // }

  PLOG(debug) << pmessage->message("base -- offset index pairs");
  for (auto i = 0; i < s->baseOffsetIndicesPairs().size(); i++) {
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(s->baseOffsetIndicesPairs()[i][0]) + " : "
      + s->curveBase()->vertices()[s->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s->baseOffsetIndicesPairs()[i][1]) + " : "
      + s->curveOffset()->vertices()[s->baseOffsetIndicesPairs()[i][1]]->printString()
    );
  }

  // std::cout << "\n        baseline s->curveOffset" << std::endl;
  // for (auto v : s->curveOffset()->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }


  pmessage->decreaseIndent();

  return;
}



















void PComponent::joinSegments(Segment *s1, Segment *s2, int e1, int e2,
                              PDCELVertex *v, int style, Message *pmessage) {
  pmessage->increaseIndent();

  // if (config.debug) {
  //   // std::cout << "[debug] joining segments ends: " << s1->getName() << " " << e1
  //   //           << ", " << s2->getName() << " " << e2 << std::endl;
  //   std::stringstream ss;
  //   ss << "joining segments ends: " << s1->getName() << " " << e1
  //      << ", " << s2->getName() << " " << e2;
  //   pmessage->print(9, ss.str());
  // }
  PLOG(debug) << pmessage->message(
    "joining segments ends: "
    + s1->getName() + " " + std::to_string(e1) + ", "
    + s2->getName() + " " + std::to_string(e2)
  );

  // std::cout << std::endl;
  // std::cout << "\n[debug] joining segments ends: " << s1->getName() << " " << e1
  //           << ", " << s2->getName() << " " << e2 << std::endl;

  PDCELVertex *v1_new, *v2_new, *v_intersect;
  Baseline *bl1_off_new, *bl2_off_new;
  std::vector<PDCELVertex *> bound_vertices;
  bound_vertices.push_back(v);









  // Step-like joint
  // This style uses the angle bisector line as the boundary
  // between the two segments
  // If the layup thicknesses are not the same
  // or base curves are not symmetric about the bisector,
  // then there will be a step change at the joint
  if (style == 1) {


    // Calculate the bounding vector and line segment
    SVector3 b, t1, t2;
    t1 = e1 == 0 ? s1->getBeginTangent() : s1->getEndTangent();
    t2 = e2 == 0 ? s2->getBeginTangent() : s2->getEndTangent();
    // std::cout << "        vector t1: " << t1 << std::endl;
    // std::cout << "        vector t2: " << t2 << std::endl;
    b = calcAngleBisectVector(t1, t2, s1->getLayupside(), s2->getLayupside());
    // std::cout << "        vector b: " << b << std::endl;

    PGeoLineSegment *ls_b = new PGeoLineSegment(v, b);
    PDCELVertex *tmp_v = new PDCELVertex((v->point()+b).point());
    std::vector<PDCELVertex *> tmp_bound_1, tmp_bound_2;
    tmp_bound_1.push_back(v);
    tmp_bound_1.push_back(tmp_v);
    tmp_bound_2.push_back(v);
    tmp_bound_2.push_back(tmp_v);




    // Intersect offset curve with the bound for each segment
    int ib_tmp, i_tmp;
    double u1, u2, ub1, ub2;
    std::vector<int> i1s, i2s, ibs1, ibs2;
    std::vector<double> u1s, u2s, ubs1, ubs2;
    int ls_i1, ls_i2, ls_bi1, ls_bi2;
    double ls_u1, ls_u2, ls_bu1, ls_bu2;
    int is_new_1, is_new_2, is_new_b1, is_new_b2;
    int j1;




    // Intersect segment 1 offset curve and the bound

    findAllIntersections(
      s1->curveOffset()->vertices(), tmp_bound_1, i1s, ibs1, u1s, ubs1
    );
    ls_u1 = getIntersectionLocation(
      s1->curveOffset()->vertices(), i1s, u1s, e1, 0, ls_i1, j1, pmessage
    );
    // ls_bu1 = getIntersectionLocation(
    //   tmp_bound, ibs1, ubs1, 0, 0, ls_bi1
    // );
    ls_bi1 = ibs1[j1];
    ls_bu1 = ubs1[j1];
    // std::cout << "\nls_bi1 = " << ls_bi1 << ", ls_bu1 = " << ls_bu1 << std::endl;
    v1_new = getIntersectionVertex(
      s1->curveOffset()->vertices(), tmp_bound_1,
      ls_i1, ls_bi1, ls_u1, ls_bu1, e1, 0, 0, 0, is_new_1, is_new_b1, TOLERANCE
    );
    trim(s1->curveOffset()->vertices(), v1_new, e1);

    PLOG(debug) << pmessage->message("  ls_i1 = " + std::to_string(ls_i1));

    PLOG(debug) << pmessage->message(
      "  intersection vertex for " + s1->getName()
      + ": " + v1_new->printString()
    );




    // Adjust linking indices
  
    if (is_new_1) {
      // If the intersection vertex is a new one

      if (e1 == 0) {
        // At the starting end

        if (s1->curveOffset()->vertices().size() > s1->baseOffsetIndicesPairs().back()[1] + 1) {
          // Extending case
          // Add 1 to every offset index
          for (auto k = 0; k < s1->baseOffsetIndicesPairs().size(); k++) {
            (s1->baseOffsetIndicesPairs()[k][1])++;
          }
          // Insert a new pair of (0, 0)
          std::vector<int> id_pair_tmp{0, 0};
          s1->baseOffsetIndicesPairs().insert(
            s1->baseOffsetIndicesPairs().begin(), id_pair_tmp
          );
        }
        else {
          // Trimming case
          // 1: Remove the index pairs if the offset vertex index is in the trim region
          while (1) {
            int id_offset_tmp = s1->baseOffsetIndicesPairs().front()[1];
            if (id_offset_tmp > (ls_i1-1)) break;
            s1->baseOffsetIndicesPairs().erase(s1->baseOffsetIndicesPairs().begin());
          }

          // 2: Renumber indices
          for (int k = 0; k < s1->baseOffsetIndicesPairs().size(); k++) {
            s1->baseOffsetIndicesPairs()[k][1] -= (ls_i1-1);
          }

          // 3: Add the index pairs for the trimmed end
          std::vector<std::vector<int>>::iterator it_tmp = s1->baseOffsetIndicesPairs().begin();
          int id_base_tmp = s1->baseOffsetIndicesPairs().front()[0];
          for (int k = 0; k < id_base_tmp; k++) {
            std::vector<int> id_pair_tmp{id_base_tmp - 1 - k, 0};
            it_tmp = s1->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
          }
          // std::vector<int> id_pair_tmp{0, 0};
          // it_tmp = s1->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
        }


        // Old method
        // if (s1->curveOffset()->vertices().size() > s1->baseOffsetIndicesLink().back() + 1) {
        //   for (auto k = 1; k < s1->baseOffsetIndicesLink().size(); k++) {
        //     s1->baseOffsetIndicesLink()[k] += 1;
        //   }
        // }
        // else {
        //   for (auto k = 0; k < s1->baseOffsetIndicesLink().size(); k++) {
        //     if (s1->baseOffsetIndicesLink()[k] < ls_i1) {
        //       s1->baseOffsetIndicesLink()[k] = 0;
        //     }
        //     else {
        //       s1->baseOffsetIndicesLink()[k] -= ls_i1 - 1;
        //     }
        //   }
        // }


      }
      else if (e1 == 1) {
        // At the stopping end

        if (s1->curveOffset()->vertices().size() > s1->baseOffsetIndicesPairs().back()[1] + 1) {
          // Extending case
          std::vector<int> id_pair_tmp{
            s1->baseOffsetIndicesPairs().back()[0],
            s1->baseOffsetIndicesPairs().back()[1] + 1
          };
          s1->baseOffsetIndicesPairs().push_back(id_pair_tmp);
        }
        else {
          // Trimming case
          // 1.2: Remove the index pairs if the offset vertex index is in the trim region
          while (1) {
            int id_offset_tmp = s1->baseOffsetIndicesPairs().back()[1];
            if (id_offset_tmp < ls_i1) break;
            s1->baseOffsetIndicesPairs().pop_back();
          }

          // 3: Add the index pairs for the trimmed end
          for (
            auto k = s1->baseOffsetIndicesPairs().back()[0]+1;
            k < s1->curveBase()->vertices().size(); k++
          ) {
            std::vector<int> id_pair_tmp{k, ls_i1};
            s1->baseOffsetIndicesPairs().push_back(id_pair_tmp);
          }
        }


        // Old method
        // if (s1->curveOffset()->vertices().size() > s1->baseOffsetIndicesLink().back() + 1) {
        //   s1->baseOffsetIndicesLink().back() += 1;
        // }
        // else {
        //   for (auto k = 0; k < s1->baseOffsetIndicesLink().size(); k++) {
        //     if (s1->baseOffsetIndicesLink()[k] > ls_i1) {
        //       s1->baseOffsetIndicesLink()[k] = ls_i1;
        //     }
        //   }
        // }
      }

    }
    else {

      // If the intersection vertex is an existing one
      if (e1 == 0) {
        for (auto k = 0; k < s1->baseOffsetIndicesPairs().size(); k++) {
          if (s1->baseOffsetIndicesPairs()[k][1] <= ls_i1) {
            s1->baseOffsetIndicesPairs()[k][1] = 0;
          }
          else {
            s1->baseOffsetIndicesPairs()[k][1] -= ls_i1;
          }
        }


        // Old method
        // for (auto k = 0; k < s1->baseOffsetIndicesLink().size(); k++) {
        //   if (s1->baseOffsetIndicesLink()[k] <= ls_i1) {
        //     s1->baseOffsetIndicesLink()[k] = 0;
        //   }
        //   else {
        //     s1->baseOffsetIndicesLink()[k] -= ls_i1;
        //   }
        // }

      }
      else if (e1 == 1) {
        for (auto k = 0; k < s1->baseOffsetIndicesPairs().size(); k++) {
          if (s1->baseOffsetIndicesPairs()[k][1] > ls_i1) {
            s1->baseOffsetIndicesPairs()[k][1] = ls_i1;
          }
        }


        // Old method
        // for (auto k = 0; k < s1->baseOffsetIndicesLink().size(); k++) {
        //   if (s1->baseOffsetIndicesLink()[k] > ls_i1) {
        //     s1->baseOffsetIndicesLink()[k] = ls_i1;
        //   }
        // }

      }

    }

    // std::cout << "\n[debug] new base-offset link:";
    // for (auto k : s1->baseOffsetIndicesLink()) {
    //   std::cout << " " << k;
    // }
    // std::cout << std::endl;




    // Intersect segment 2 offset curve and the bound

    findAllIntersections(
      s2->curveOffset()->vertices(), tmp_bound_2, i2s, ibs2, u2s, ubs2
    );
    ls_u2 = getIntersectionLocation(
      s2->curveOffset()->vertices(), i2s, u2s, e2, 0, ls_i2, j1, pmessage
    );
    // ls_bu2 = getIntersectionLocation(
    //   tmp_bound, ibs2, ubs2, 0, 0, ls_bi2
    // );
    ls_bi2 = ibs2[j1];
    ls_bu2 = ubs2[j1];
    // std::cout << "\nls_bi2 = " << ls_bi2 << ", ls_bu2 = " << ls_bu2 << std::endl;
    v2_new = getIntersectionVertex(
      s2->curveOffset()->vertices(), tmp_bound_2,
      ls_i2, ls_bi2, ls_u2, ls_bu2, e2, 0, 0, 0, is_new_2, is_new_b2, TOLERANCE
    );
    trim(s2->curveOffset()->vertices(), v2_new, e2);

    PLOG(debug) << pmessage->message("  ls_i2 = " + std::to_string(ls_i2));

    PLOG(debug) << pmessage->message(
      "  intersection vertex for " + s2->getName()
      + ": " + v2_new->printString()
    );




    // Adjust linking indices

    if (is_new_2) {
      // If the intersection vertex is a new one

      if (e2 == 0) {
        // At the starting end

        if (s2->curveOffset()->vertices().size() > s2->baseOffsetIndicesPairs().back()[1] + 1) {
          // Extending case
          // Add 1 to every offset index
          for (auto k = 0; k < s2->baseOffsetIndicesPairs().size(); k++) {
            (s2->baseOffsetIndicesPairs()[k][1])++;
          }
          // Insert a new pair of (0, 0)
          std::vector<int> id_pair_tmp{0, 0};
          s2->baseOffsetIndicesPairs().insert(
            s2->baseOffsetIndicesPairs().begin(), id_pair_tmp
          );
        }
        else {
          // Trimming case
          // 1: Remove the index pairs if the offset vertex index is in the trim region
          while (1) {
            int id_offset_tmp = s2->baseOffsetIndicesPairs().front()[1];
            if (id_offset_tmp > (ls_i2-1)) break;
            s2->baseOffsetIndicesPairs().erase(s2->baseOffsetIndicesPairs().begin());
          }

          // 2: Renumber indices
          for (int k = 0; k < s2->baseOffsetIndicesPairs().size(); k++) {
            s2->baseOffsetIndicesPairs()[k][1] -= (ls_i2-1);
          }

          // 3: Add the index pairs for the trimmed end
          std::vector<std::vector<int>>::iterator it_tmp = s2->baseOffsetIndicesPairs().begin();
          int id_base_tmp = s2->baseOffsetIndicesPairs().front()[0];
          for (int k = 0; k < id_base_tmp; k++) {
            std::vector<int> id_pair_tmp{id_base_tmp - 1 - k, 0};
            it_tmp = s2->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
          }
          // std::vector<int> id_pair_tmp{0, 0};
          // it_tmp = s1->baseOffsetIndicesPairs().insert(it_tmp, id_pair_tmp);
        }


        // Old method
        // if (s2->curveOffset()->vertices().size() > s2->baseOffsetIndicesLink().back() + 1) {
        //   for (auto k = 1; k < s2->baseOffsetIndicesLink().size(); k++) {
        //     s2->baseOffsetIndicesLink()[k] += 1;
        //   }
        // }
        // else {
        //   for (auto k = 0; k < s2->baseOffsetIndicesLink().size(); k++) {
        //     if (s2->baseOffsetIndicesLink()[k] < ls_i2) {
        //       s2->baseOffsetIndicesLink()[k] = 0;
        //     }
        //     else {
        //       s2->baseOffsetIndicesLink()[k] -= ls_i2 - 1;
        //     }
        //   }
        // }


      }
      else if (e2 == 1) {
        // At the stopping end

        if (s2->curveOffset()->vertices().size() > s2->baseOffsetIndicesPairs().back()[1] + 1) {
          // Extending case
          std::vector<int> id_pair_tmp{
            s2->baseOffsetIndicesPairs().back()[0],
            s2->baseOffsetIndicesPairs().back()[1] + 1
          };
          s2->baseOffsetIndicesPairs().push_back(id_pair_tmp);
        }
        else {
          // Trimming case
          // 1.2: Remove the index pairs if the offset vertex index is in the trim region
          while (1) {
            int id_offset_tmp = s2->baseOffsetIndicesPairs().back()[1];
            if (id_offset_tmp < ls_i2) break;
            s2->baseOffsetIndicesPairs().pop_back();
          }

          // 3: Add the index pairs for the trimmed end
          for (
            auto k = s2->baseOffsetIndicesPairs().back()[0]+1;
            k < s2->curveBase()->vertices().size(); k++
          ) {
            std::vector<int> id_pair_tmp{k, ls_i2};
            s2->baseOffsetIndicesPairs().push_back(id_pair_tmp);
          }
        }


        // Old method
        // if (s2->curveOffset()->vertices().size() > s2->baseOffsetIndicesLink().back() + 1) {
        //   s2->baseOffsetIndicesLink().back() += 1;
        // }
        // else {
        //   for (auto k = 0; k < s2->baseOffsetIndicesLink().size(); k++) {
        //     if (s2->baseOffsetIndicesLink()[k] > ls_i2) {
        //       s2->baseOffsetIndicesLink()[k] = ls_i2;
        //     }
        //   }
        // }

      }


    }
    else {
      // If the intersection vertex is an existing one

      if (e2 == 0) {
        for (auto k = 0; k < s2->baseOffsetIndicesPairs().size(); k++) {
          if (s2->baseOffsetIndicesPairs()[k][1] <= ls_i2) {
            s2->baseOffsetIndicesPairs()[k][1] = 0;
          }
          else {
            s2->baseOffsetIndicesPairs()[k][1] -= ls_i2;
          }
        }

        // Old method
        // for (auto k = 0; k < s2->baseOffsetIndicesLink().size(); k++) {
        //   if (s2->baseOffsetIndicesLink()[k] <= ls_i2) {
        //     s2->baseOffsetIndicesLink()[k] = 0;
        //   }
        //   else {
        //     s2->baseOffsetIndicesLink()[k] -= ls_i2;
        //   }
        // }
      }
      else if (e2 == 1) {

        for (auto k = 0; k < s2->baseOffsetIndicesPairs().size(); k++) {
          if (s2->baseOffsetIndicesPairs()[k][1] > ls_i2) {
            s2->baseOffsetIndicesPairs()[k][1] = ls_i2;
          }
        }

        // Old method
        // for (auto k = 0; k < s2->baseOffsetIndicesLink().size(); k++) {
        //   if (s2->baseOffsetIndicesLink()[k] > ls_i2) {
        //     s2->baseOffsetIndicesLink()[k] = ls_i2;
        //   }
        // }
      }
    }

    // std::cout << "\n[debug] new base-offset link:";
    // for (auto k : s2->baseOffsetIndicesLink()) {
    //   std::cout << " " << k;
    // }
    // std::cout << std::endl;




    // std::cout << "|ub1 - ub2| = " << fabs(ub1 - ub2) << std::endl;
    if (fabs(ls_bu1 - ls_bu2) < TOLERANCE) {
      // Two new points are close, leave one
      v2_new = v1_new;
      bound_vertices.push_back(v1_new);

      if (e2 == 0) {
        // bl2_off_new->vertices()[0] = v1_new;
        s2->curveOffset()->vertices()[0] = v1_new;
      }
      else if (e2 == 1) {
        // bl2_off_new->vertices()[bl2_off_new->vertices().size() - 1] = v1_new;
        s2->curveOffset()->vertices()[s2->curveOffset()->vertices().size() - 1] = v1_new;
      }
    }
    else {
      if (ls_bu1 < ls_bu2) {
        bound_vertices.push_back(v1_new);
        bound_vertices.push_back(v2_new);
      }
      else {
        bound_vertices.push_back(v2_new);
        bound_vertices.push_back(v1_new);
      }
    }
  }
  
  







  // Non-step-like joint
  else if (style == 2) {

    // Intersect the two offset curves
    double u1_tmp, u2_tmp;
    PDCELVertex *v11, *v12, *v21, *v22;
    int i1 = 0, i2 = 0, n1, n2;
    n1 = s1->curveOffset()->vertices().size();
    n2 = s2->curveOffset()->vertices().size();
    PGeoLineSegment *ls1, *ls2;
    std::vector<PGeoLineSegment *> lss1, lss2;
    bool found = false;
    while (1) {
      ls1 = nullptr;
      ls2 = nullptr;

      if (lss1.size() < n1 - 1) {
        if (e1 == 0) {
          v11 = s1->curveOffset()->vertices()[i1];
          v12 = s1->curveOffset()->vertices()[i1 + 1];
        }
        else {
          v11 = s1->curveOffset()->vertices()[n1 - 1 - i1];
          v12 = s1->curveOffset()->vertices()[n1 - 2 - i1];
        }
        ls1 = new PGeoLineSegment(v11, v12);
        lss1.push_back(ls1);
      }

      if (lss2.size() < n2 - 1) {
        if (e2 == 0) {
          v21 = s2->curveOffset()->vertices()[i2];
          v22 = s2->curveOffset()->vertices()[i2 + 1];
        }
        else {
          v21 = s2->curveOffset()->vertices()[n2 - 1 - i2];
          v22 = s2->curveOffset()->vertices()[n2 - 2 - i2];
        }
        ls2 = new PGeoLineSegment(v21, v22);
        lss2.push_back(ls2);
      }

      ls1 = lss1.back();
      for (int i = 0; i < lss2.size(); ++i) {
        ls2 = lss2[i];
        calcLineIntersection2D(ls1, ls2, u1_tmp, u2_tmp);
        if (u1_tmp >= 0 && u1_tmp <= 1 && u2_tmp >= 0 && u2_tmp <= 1) {
          found = true;
          i2 = i;
          break;
        }
        if (ls1 == lss1.front() || ls2 == lss2.front()) {
          if (ls1 == lss1.front() && u1_tmp < 0 && ls2 == lss2.front() &&
              u2_tmp < 0) {
            found = true;
            i2 = i;
            break;
          }
          if (ls1 == lss1.front() && u1_tmp < 0 && u2_tmp >= 0 && u2_tmp <= 1) {
            found = true;
            i2 = i;
            break;
          }
          if (ls2 == lss2.front() && u2_tmp < 0 && u1_tmp >= 0 && u1_tmp <= 1) {
            found = true;
            i2 = i;
            break;
          }
        }
      }

      if (!found) {
        ls2 = lss2.back();
        for (int i = 0; i < lss1.size(); ++i) {
          ls1 = lss1[i];
          calcLineIntersection2D(ls1, ls2, u1_tmp, u2_tmp);
          if (u1_tmp >= 0 && u1_tmp <= 1 && u2_tmp >= 0 && u2_tmp <= 1) {
            found = true;
            i1 = i;
            break;
          }
          if (ls1 == lss1.front() || ls2 == lss2.front()) {
            if (ls1 == lss1.front() && u1_tmp < 0 && ls2 == lss2.front() &&
                u2_tmp < 0) {
              found = true;
              i1 = i;
              break;
            }
            if (ls1 == lss1.front() && u1_tmp < 0 && u2_tmp >= 0 &&
                u2_tmp <= 1) {
              found = true;
              i1 = i;
              break;
            }
            if (ls2 == lss2.front() && u2_tmp < 0 && u1_tmp >= 0 &&
                u1_tmp <= 1) {
              found = true;
              i1 = i;
              break;
            }
          }
        }
      }

      if (found || (i1 == n1 - 2 && i2 == n2 - 2)) {
        break;
      } else {
        if (i1 < n1 - 2) {
          i1++;
        }
        if (i2 < n2 - 2) {
          i2++;
        }
      }
    }

    if (found) {
      if (fabs(u1_tmp) < TOLERANCE) {
        i1 += 1;
        v_intersect = ls1->v1();
      }
      else if (fabs(1 - u1_tmp) < TOLERANCE) {
        i1 += 2;
        v_intersect = ls1->v2();
      }
      else {
        i1 += 1;
        v_intersect = ls1->getParametricVertex(u1_tmp);
      }

      if (fabs(u2_tmp) < TOLERANCE) {
        i2 += 1;
      }
      else if (fabs(1 - u2_tmp) < TOLERANCE) {
        i2 += 2;
      }
      else {
        i2 += 1;
      }

      bl1_off_new = new Baseline();
      if (e1 == 0) {
        bl1_off_new->addPVertex(v_intersect);
        for (int i = i1; i < n1; ++i) {
          bl1_off_new->addPVertex(s1->curveOffset()->vertices()[i]);
        }
      }
      else if (e1 == 1) {
        for (int i = 0; i < n1 - i1; ++i) {
          bl1_off_new->addPVertex(s1->curveOffset()->vertices()[i]);
        }
        bl1_off_new->addPVertex(v_intersect);
      }

      bl2_off_new = new Baseline();
      if (e2 == 0) {
        bl2_off_new->addPVertex(v_intersect);
        for (int i = i2; i < n2; ++i) {
          bl2_off_new->addPVertex(s2->curveOffset()->vertices()[i]);
        }
      }
      else if (e2 == 1) {
        for (int i = 0; i < n2 - i2; ++i) {
          bl2_off_new->addPVertex(s2->curveOffset()->vertices()[i]);
        }
        bl2_off_new->addPVertex(v_intersect);
      }

      v1_new = v_intersect;
      v2_new = v_intersect;

      bound_vertices.push_back(v_intersect);
    }

    s1->setCurveOffset(bl1_off_new);
    s2->setCurveOffset(bl2_off_new);
  }

  // std::cout << "        bound_vertices:" << std::endl;
  // for (auto v : bound_vertices) {
  //   v->printWithAddress();
  // }

  if (e1 == 0) {
    s1->setHeadVertexOffset(v1_new);
  }
  else if (e1 == 1) {
    s1->setTailVertexOffset(v1_new);
  }

  if (e2 == 0) {
    s2->setHeadVertexOffset(v2_new);
  }
  else if (e2 == 1) {
    s2->setTailVertexOffset(v2_new);
  }

  // std::cout << "\n[debug] new offset vertices of segment " << s1->getName() << std::endl;
  // for (auto v : s1->curveOffset()->vertices()) {
  //   std::cout << v << std::endl;
  // }

  // std::cout << "\n[debug] new offset vertices of segment " << s2->getName() << std::endl;
  // for (auto v : s2->curveOffset()->vertices()) {
  //   std::cout << v << std::endl;
  // }

  // Create half edges
  PDCELHalfEdge *he_new;
  for (int i = 0; i < bound_vertices.size() - 1; ++i) {
    he_new = _pmodel->dcel()->addEdge(bound_vertices[i], bound_vertices[i + 1]);
    // std::cout << "        new half edge: " << he_new << std::endl;
  }

  PLOG(debug) << pmessage->message("  base -- offset index pairs of segment: " + s1->getName());
  for (auto i = 0; i < s1->baseOffsetIndicesPairs().size(); i++) {
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(s1->baseOffsetIndicesPairs()[i][0]) + " : "
      + s1->curveBase()->vertices()[s1->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s1->baseOffsetIndicesPairs()[i][1]) + " : "
      + s1->curveOffset()->vertices()[s1->baseOffsetIndicesPairs()[i][1]]->printString()
    );
  }

  PLOG(debug) << pmessage->message("  base -- offset index pairs of segment: " + s2->getName());
  for (auto i = 0; i < s2->baseOffsetIndicesPairs().size(); i++) {
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    PLOG(debug) << pmessage->message(
      "  " + std::to_string(s2->baseOffsetIndicesPairs()[i][0]) + " : "
      + s2->curveBase()->vertices()[s2->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s2->baseOffsetIndicesPairs()[i][1]) + " : "
      + s2->curveOffset()->vertices()[s2->baseOffsetIndicesPairs()[i][1]]->printString()
    );
  }

  pmessage->decreaseIndent();
}



















void PComponent::createSegmentFreeEnd(Segment *s, int e) {
  // std::cout << "[debug] createSegmentFreeEnd: " << s->getName() << " " << e
  // << std::endl;
  if (e == 0 && s->prevBound().normSq() != 0) {
    // Use pre-defined bound direction
  } else if (e == 1 && s->nextBound().normSq() != 0) {
    // Use pre-defined bound direction
  }


  if (e == 0) {
    // s->getBaseline()->vertices().front()->printWithAddress();

    _pmodel->dcel()->addEdge(s->getBaseline()->vertices().front(),
                             s->curveOffset()->vertices().front());
    s->setHeadVertexOffset(s->curveOffset()->vertices().front());

    // s->getBaseline()->vertices().front()->printAllLeavingHalfEdges(1);
  } else if (e == 1) {
    // s->getBaseline()->vertices().front()->printWithAddress();

    _pmodel->dcel()->addEdge(s->getBaseline()->vertices().back(),
                             s->curveOffset()->vertices().back());
    s->setTailVertexOffset(s->curveOffset()->vertices().back());

    // s->getBaseline()->vertices().back()->printAllLeavingHalfEdges(1);
  }
}
