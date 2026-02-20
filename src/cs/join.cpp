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

#include "geo_types.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>

// Returns true if (ls_i_tmp, u1_tmp) is a better intersection than the
// current best (ls_i_prev, u1) when searching from end `e` of a polyline.
// last_seg_idx: (vertices.size()-1) for base curve, (vertices.size()-2) for offset.
static bool isBetterIntersection(
    int e, int ls_i_tmp, double u1_tmp,
    int ls_i_prev, double u1,
    int last_seg_idx, double tol)
{
  const bool u_on_seg = (fabs(u1_tmp) <= tol)
                     || (u1_tmp > 0 && u1_tmp < 1)
                     || (fabs(1 - u1_tmp) <= tol);
  if (e == 0) {
    return (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1)
        || (u_on_seg && ls_i_tmp > ls_i_prev)
        || (u_on_seg && ls_i_tmp == ls_i_prev && u1_tmp > u1);
  } else {
    return (ls_i_tmp == last_seg_idx && u1_tmp > 1 && u1_tmp < u1)
        || (u_on_seg && ls_i_tmp < ls_i_prev)
        || (u_on_seg && ls_i_tmp == ls_i_prev && u1_tmp < u1);
  }
}

// Finds the best-intersecting half-edge from `hels` against `vertices`,
// working from end `e`. Returns the winning half-edge (nullptr if none found).
// last_seg_idx: (vertices.size()-1) for base, (vertices.size()-2) for offset.
static PDCELHalfEdge *findBestIntersection(
    const std::vector<PDCELVertex *> &vertices,
    const std::vector<PDCELHalfEdgeLoop *> &hels,
    int e, int last_seg_idx,
    double &u1_out, double &u2_out, int &ls_i_out,
    double tol)
{
  double u1 = (e == 0) ? -INF : INF;
  int ls_i_prev = (e == 0) ? -1 : static_cast<int>(vertices.size());
  double u1_tmp, u2_tmp; int ls_i_tmp;
  PDCELHalfEdge *he_tool = nullptr;

  for (auto hel : hels) {
    if (!hel->keep()) {
      PDCELHalfEdge *he = findCurvesIntersection(
          vertices, hel, e, ls_i_tmp, u1_tmp, u2_tmp, tol);
      if (he != nullptr &&
          isBetterIntersection(e, ls_i_tmp, u1_tmp,
                               ls_i_prev, u1, last_seg_idx, tol)) {
        u1 = u1_tmp; u2_out = u2_tmp; he_tool = he; ls_i_prev = ls_i_tmp;
      }
    }
  }

  ls_i_out = ls_i_prev;
  u1_out = u1;
  return he_tool;
}

// Adjusts base-offset index pairs after trimming the head (e==0) of a segment.
// Steps: (1) remove pairs in trim region, (2) renumber, (3) insert staircase.
// If remove_base is false, only offset pairs are removed/renumbered in steps 1-2.
static void adjustPairsAfterTrimHead(
    std::vector<std::vector<int>> &pairs,
    int ls_i_base, int ls_i_offset,
    bool remove_base)
{
  if (remove_base) {
    while (pairs.front()[0] <= ls_i_base)
      pairs.erase(pairs.begin());
  }
  while (pairs.front()[1] <= ls_i_offset)
    pairs.erase(pairs.begin());

  for (auto &p : pairs) {
    p[0] -= ls_i_base;
    p[1] -= ls_i_offset;
  }

  int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
  auto it = pairs.begin();
  for (int k = 0; k < nv_diff; k++) {
    if (ls_i_base > ls_i_offset)
      it = pairs.insert(it, {0, nv_diff - k});
    else if (ls_i_base < ls_i_offset)
      it = pairs.insert(it, {nv_diff - k, 0});
  }
  pairs.insert(it, {0, 0});
}

// Adjusts base-offset index pairs after trimming the tail (e==1) of a segment.
// Steps: (1) remove pairs in trim region, (3) append staircase (no renumber at tail).
// If remove_base is false, only offset pairs are removed in step 1.
// nv_base_cap / nv_offset_cap: bounds to prevent over-extension (-1 = no cap).
static void adjustPairsAfterTrimTail(
    std::vector<std::vector<int>> &pairs,
    int ls_i_base, int ls_i_offset,
    bool remove_base,
    int nv_base_cap, int nv_offset_cap)
{
  if (remove_base) {
    while (pairs.back()[0] >= ls_i_base)
      pairs.pop_back();
  }
  while (pairs.back()[1] >= ls_i_offset)
    pairs.pop_back();

  int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
  int id_base = pairs.back()[0];
  int id_offset = pairs.back()[1];
  for (int k = 0; k < nv_diff; k++) {
    if (nv_base_cap >= 0 && id_base == nv_base_cap - 2 &&
        nv_offset_cap >= 0 && id_offset == nv_offset_cap - 2)
      break;
    pairs.push_back({id_base + 1, id_offset + 1});
    if (ls_i_base > ls_i_offset)      id_base++;
    else if (ls_i_base < ls_i_offset) id_offset++;
  }
  pairs.push_back({id_base + 1, id_offset + 1});
}

void PComponent::joinSegments(Segment *s, int e, PDCELVertex * /*v*/, const BuilderConfig &bcfg) {

  MESSAGE_SCOPE(g_msg);

  PLOG(debug) << g_msg->message("making segment end: " + s->getName() + " " + std::to_string(e));

  PLOG(debug) << g_msg->message("number of dependencies: " + std::to_string(_dependencies.size()));

  if (_dependencies.empty()) {
    PLOG(debug) << g_msg->message("making segment end because of no dependency.");
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  else {

    PLOG(debug) << g_msg->message("the end to be done (e): " + std::to_string(e));
    PLOG(debug) << g_msg->message("free end of the segment (s->free()): " + std::to_string(s->free()));

    if (e == s->free()) {
      PLOG(debug) << g_msg->message("making segment end because of free end set although with dependency.");
      createSegmentFreeEnd(s, e, bcfg);
      return;
    }

    else {

      PDCELHalfEdge *he_tool;
      int ls_i, ls_i_prev;
      double u1, u2;
      PDCELVertex *v_new;

      // 1. Use the reference point of the component to find the outer
      // half edge loop that enclosing this point
      // 2. Find all inner half edge loops inside the outer half edge loop
      // 3. For each found loop, calculate intersections and
      //    keep the intersection(s) closest to the reference point

      if (_ref_vertex == nullptr) {
        _ref_vertex = _segments[0]->curveBase()->refVertex();
        if (_ref_vertex == nullptr) {
          int i = static_cast<int>(_segments[0]->curveBase()->vertices().size() / 2);
          _ref_vertex = _segments[0]->curveBase()->vertices()[i];
        }
      }
      PLOG(debug) << g_msg->message("ref vertex: " + _ref_vertex->printString());

      bool to_be_removed;
      if (_ref_vertex->dcel() != nullptr) {
        to_be_removed = false;
      }
      else {
        bcfg.dcel->addVertex(_ref_vertex);
        to_be_removed = true;
      }

      // 1.
      PLOG(debug) << g_msg->message("step 1: find the outer half edge loop");

      std::vector<PDCELHalfEdgeLoop *> hels;
      PDCELHalfEdgeLoop *tmp_hel_out;
      tmp_hel_out = bcfg.dcel->findEnclosingLoop(_ref_vertex);
      if (tmp_hel_out != bcfg.dcel->halfedgeloops().front()) {
        // Not the first loop with INF size
        hels.push_back(tmp_hel_out);
      }

      // 2.
      PLOG(debug) << g_msg->message("step 2: find all inner half edge loops");

      PDCELFace *tmp_face = tmp_hel_out->face();
      if (tmp_face != nullptr) {
        // Try to update inner loops first
        if (tmp_face->inners().size() == 0) {
          bcfg.dcel->linkHalfEdgeLoops();
          for (auto heli : bcfg.dcel->halfedgeloops()) {
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

      if (to_be_removed) {
        bcfg.dcel->removeVertex(_ref_vertex);
      }

      if (bcfg.debug) {
        std::cout << "\nhels:\n";
        PLOG(debug) << g_msg->message("found half edge loops");
        for (auto hel : hels) {
          hel->log();
        }
      }

      // 3.
      PLOG(debug) << g_msg->message("step 3: calculate intersections");

      // 3.1. For the base curve
      PLOG(debug) << g_msg->message("step 3.1: for the base curve");

      he_tool = findBestIntersection(
        s->curveBase()->vertices(), hels, e,
        (int)s->curveBase()->vertices().size() - 1,
        u1, u2, ls_i_prev, TOLERANCE);
      ls_i = ls_i_prev;
      if ((fabs(1 - u1) <= TOLERANCE) || e == 1) ls_i += 1;

      PLOG(debug) << g_msg->message("  u1 = " + std::to_string(u1));
      PLOG(debug) << g_msg->message("  u2 = " + std::to_string(u2));

      if (fabs(u1) == INF) {
        PLOG(debug) << g_msg->message("making segment end because of not touching any other components although with dependency.");
        createSegmentFreeEnd(s, e, bcfg);
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
          bcfg.dcel->splitEdge(he_tool, v_new);
        }
      }
      PLOG(debug) << g_msg->message("  v_new = ") << v_new->printString();

      int ls_i_base = ls_i;
      PLOG(debug) << g_msg->message("  ls_i = " + std::to_string(ls_i));

      if (e == 0) {
        for (auto k = 0; k < ls_i; k++) {
          s->curveBase()->vertices().erase(s->curveBase()->vertices().begin());
          s->baseOffsetIndicesLink().erase(s->baseOffsetIndicesLink().begin());
        }

        s->curveBase()->vertices()[0] = v_new;
      }
      else if (e == 1) {
        int n = static_cast<int>(s->curveBase()->vertices().size());
        for (auto k = ls_i; k < n - 1; k++) {
          s->curveBase()->vertices().pop_back();
          s->baseOffsetIndicesLink().pop_back();
        }
        s->curveBase()->vertices().back() = v_new;
      }

      // 3.2. For the offset curve
      PLOG(debug) << g_msg->message("step 3.2: for the offset curve");

      he_tool = findBestIntersection(
        s->curveOffset()->vertices(), hels, e,
        (int)s->curveOffset()->vertices().size() - 2,
        u1, u2, ls_i_prev, TOLERANCE);
      ls_i = ls_i_prev;
      if ((fabs(1 - u1) <= TOLERANCE) || e == 1) ls_i += 1;

      PLOG(debug) << g_msg->message("  u1 = " + std::to_string(u1));
      PLOG(debug) << g_msg->message("  u2 = " + std::to_string(u2));

      if (fabs(u1) == INF) {
        PLOG(debug) << g_msg->message("offset curve: no intersection found, using free end.");
        createSegmentFreeEnd(s, e, bcfg);
        return;
      }

      // This end is trimmed by some other components
      if (fabs(u2) <= TOLERANCE) {
        v_new = he_tool->source();
      }
      else if (fabs(1 - u2) <= TOLERANCE) {
        v_new = he_tool->target();
      }
      else {
        v_new = he_tool->toLineSegment()->getParametricVertex(u2);
        bcfg.dcel->splitEdge(he_tool, v_new);
      }
      PLOG(debug) << g_msg->message("  v_new = ") << v_new->printString();

      int ls_i_offset = ls_i;
      PLOG(debug) << g_msg->message("  ls_i = " + std::to_string(ls_i));

      if (e == 0) {
        for (auto k = 0; k < ls_i; k++) {
          s->curveOffset()->vertices().erase(s->curveOffset()->vertices().begin());
        }

        s->curveOffset()->vertices()[0] = v_new;
        s->setHeadVertexOffset(v_new);
      }
      else if (e == 1) {
        int n = static_cast<int>(s->curveOffset()->vertices().size());
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
      PLOG(debug) << g_msg->message("adjust base-offset linking indices");

      int nv_base = static_cast<int>(s->curveBase()->vertices().size());
      int nv_offset = static_cast<int>(s->curveOffset()->vertices().size());
      PLOG(debug) << g_msg->message("  nv_base = " + std::to_string(nv_base));
      PLOG(debug) << g_msg->message("  nv_offset = " + std::to_string(nv_offset));

      if (e == 0) {

        adjustPairsAfterTrimHead(s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset, true);

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

        adjustPairsAfterTrimTail(s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset, true, -1, -1);

        // Old method
        for (auto k = 0; k < s->curveBase()->vertices().size(); k++) {
          if (k >= ls_i) {
            s->baseOffsetIndicesLink()[k] = static_cast<int>(s->curveOffset()->vertices().size()) - 1;
          }
        }

      }

    }
  }

  s->printBaseOffsetPairs(g_msg);

  return;
}

void PComponent::joinSegments(
  Segment *s1, Segment *s2, int e1, int e2,
  PDCELVertex *v, int style, const BuilderConfig &bcfg
  ) {
  MESSAGE_SCOPE(g_msg);

  PLOG(debug) << g_msg->message(
    "joining segments ends: "
    + s1->getName() + " " + std::to_string(e1) + ", "
    + s2->getName() + " " + std::to_string(e2)
  );

  PDCELVertex *v1_new = nullptr, *v2_new = nullptr, *v_intersect = nullptr;
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
    b = calcAngleBisectVector(t1, t2, s1->getLayupside(), s2->getLayupside());

    PDCELVertex *tmp_v = new PDCELVertex((v->point()+b).point());
    std::vector<PDCELVertex *> tmp_bound_1, tmp_bound_2;
    tmp_bound_1.push_back(v);
    tmp_bound_1.push_back(tmp_v);
    tmp_bound_2.push_back(v);
    tmp_bound_2.push_back(tmp_v);

    // Intersect offset curve with the bound for each segment
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
      s1->curveOffset()->vertices(), i1s, u1s, e1, 0, ls_i1, j1
    );
    ls_bi1 = ibs1[j1];
    ls_bu1 = ubs1[j1];
    v1_new = getIntersectionVertex(
      s1->curveOffset()->vertices(), tmp_bound_1,
      ls_i1, ls_bi1, ls_u1, ls_bu1, e1, 0, 0, 0, is_new_1, is_new_b1, TOLERANCE
    );
    trim(s1->curveOffset()->vertices(), v1_new, e1);

    PLOG(debug) << g_msg->message("  ls_i1 = " + std::to_string(ls_i1));

    PLOG(debug) << g_msg->message(
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
        }

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

      }
      else if (e1 == 1) {
        for (auto k = 0; k < s1->baseOffsetIndicesPairs().size(); k++) {
          if (s1->baseOffsetIndicesPairs()[k][1] > ls_i1) {
            s1->baseOffsetIndicesPairs()[k][1] = ls_i1;
          }
        }

      }

    }

    // Intersect segment 2 offset curve and the bound

    findAllIntersections(
      s2->curveOffset()->vertices(), tmp_bound_2, i2s, ibs2, u2s, ubs2
    );
    ls_u2 = getIntersectionLocation(
      s2->curveOffset()->vertices(), i2s, u2s, e2, 0, ls_i2, j1
    );
    ls_bi2 = ibs2[j1];
    ls_bu2 = ubs2[j1];
    v2_new = getIntersectionVertex(
      s2->curveOffset()->vertices(), tmp_bound_2,
      ls_i2, ls_bi2, ls_u2, ls_bu2, e2, 0, 0, 0, is_new_2, is_new_b2, TOLERANCE
    );
    trim(s2->curveOffset()->vertices(), v2_new, e2);

    PLOG(debug) << g_msg->message("  ls_i2 = " + std::to_string(ls_i2));

    PLOG(debug) << g_msg->message(
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
        }

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

      }
      else if (e2 == 1) {

        for (auto k = 0; k < s2->baseOffsetIndicesPairs().size(); k++) {
          if (s2->baseOffsetIndicesPairs()[k][1] > ls_i2) {
            s2->baseOffsetIndicesPairs()[k][1] = ls_i2;
          }
        }

      }
    }

    if (fabs(ls_bu1 - ls_bu2) < TOLERANCE) {
      // Two new points are close, leave one
      v2_new = v1_new;
      bound_vertices.push_back(v1_new);

      if (e2 == 0) {
        s2->curveOffset()->vertices()[0] = v1_new;
      }
      else if (e2 == 1) {
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
    double u1_tmp = 0.0, u2_tmp = 0.0;
    PDCELVertex *v11, *v12, *v21, *v22;
    int i1 = 0, i2 = 0, n1, n2;
    n1 = static_cast<int>(s1->curveOffset()->vertices().size());
    n2 = static_cast<int>(s2->curveOffset()->vertices().size());
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
        calcLineIntersection2D(ls1, ls2, u1_tmp, u2_tmp, TOLERANCE);
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
          calcLineIntersection2D(ls1, ls2, u1_tmp, u2_tmp, TOLERANCE);
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

      for (auto p : lss1) delete p;
      for (auto p : lss2) delete p;

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
    else {
      for (auto p : lss1) delete p;
      for (auto p : lss2) delete p;
      PLOG(debug) << g_msg->message("style 2: no intersection found between offset curves, skipping join.");
      return;
    }

    s1->setCurveOffset(bl1_off_new);
    s2->setCurveOffset(bl2_off_new);
  }

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

  // Create half edges
  PDCELHalfEdge *he_new;
  for (int i = 0; i < bound_vertices.size() - 1; ++i) {
    he_new = bcfg.dcel->addEdge(bound_vertices[i], bound_vertices[i + 1]);
  }

  PLOG(debug) << g_msg->message("  base -- offset index pairs of segment: " + s1->getName());
  for (auto i = 0; i < s1->baseOffsetIndicesPairs().size(); i++) {
    PLOG(debug) << g_msg->message(
      "  " + std::to_string(s1->baseOffsetIndicesPairs()[i][0]) + " : "
      + s1->curveBase()->vertices()[s1->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s1->baseOffsetIndicesPairs()[i][1]) + " : "
      + s1->curveOffset()->vertices()[s1->baseOffsetIndicesPairs()[i][1]]->printString()
    );
  }

  PLOG(debug) << g_msg->message("  base -- offset index pairs of segment: " + s2->getName());
  for (auto i = 0; i < s2->baseOffsetIndicesPairs().size(); i++) {
    PLOG(debug) << g_msg->message(
      "  " + std::to_string(s2->baseOffsetIndicesPairs()[i][0]) + " : "
      + s2->curveBase()->vertices()[s2->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s2->baseOffsetIndicesPairs()[i][1]) + " : "
      + s2->curveOffset()->vertices()[s2->baseOffsetIndicesPairs()[i][1]]->printString()
    );
  }

}

void PComponent::createSegmentFreeEnd(Segment *s, int e, const BuilderConfig &bcfg) {
  PLOG(debug) << g_msg->message("createSegmentFreeEnd: " + s->getName() + " " + std::to_string(e));

  // Trim head
  if (e == 0 && s->prevBound().normSq() != 0) {

    SPoint3 sp0{s->getBaseline()->vertices().front()->point()};
    SVector3 sv1{s->prevBound()};
    SPoint3 sp1{sp0 + sv1.point()};
    PDCELVertex *p = new PDCELVertex(sp1);

    std::vector<PDCELVertex *> b;
    b.assign({s->getBaseline()->vertices().front(), p});

    std::vector<int> i1s, i2s;
    std::vector<double> u1s, u2s;

    findAllIntersections(s->curveOffset()->vertices(), b, i1s, i2s, u1s, u2s);

    double u1, u2;
    int ls_i1, ls_i2, j1, j2;
    u1 = getIntersectionLocation(
      s->curveOffset()->vertices(), i1s, u1s, 0, false, ls_i1, j1
    );
    u2 = getIntersectionLocation(
      b, i2s, u2s, 0, false, ls_i2, j2
    );

    PDCELVertex *ip;
    int is_new_1, is_new_2;
    double tol{1.0e-9};
    ip = getIntersectionVertex(
      s->curveOffset()->vertices(), b, ls_i1, ls_i2, u1, u2, 0, 0,
      false, false, is_new_1, is_new_2, tol
    );
    PLOG(debug) << g_msg->message("ip = " + ip->printString());

    trim(s->curveOffset()->vertices(), ip, 0);

    // Adjust linking indices
    int ls_i_offset = ls_i1 - 1;
    adjustPairsAfterTrimHead(s->baseOffsetIndicesPairs(), 0, ls_i_offset, false);

  }

  // Trim tail
  else if (e == 1 && s->nextBound().normSq() != 0) {


    SPoint3 sp0{s->getBaseline()->vertices().back()->point()};
    SVector3 sv1{s->nextBound()};
    SPoint3 sp1{sp0 + sv1.point()};
    PDCELVertex *p = new PDCELVertex(sp1);

    std::vector<PDCELVertex *> b;
    b.assign({s->getBaseline()->vertices().back(), p});

    std::vector<int> i1s, i2s;
    std::vector<double> u1s, u2s;

    findAllIntersections(s->curveOffset()->vertices(), b, i1s, i2s, u1s, u2s);

    double u1, u2;
    int ls_i1, ls_i2, j1, j2;
    u1 = getIntersectionLocation(
      s->curveOffset()->vertices(), i1s, u1s, 1, false, ls_i1, j1
    );
    u2 = getIntersectionLocation(
      b, i2s, u2s, 1, false, ls_i2, j2
    );

    PDCELVertex *ip;
    int is_new_1, is_new_2;
    double tol{1.0e-9};
    ip = getIntersectionVertex(
      s->curveOffset()->vertices(), b, ls_i1, ls_i2, u1, u2, 1, 1,
      false, false, is_new_1, is_new_2, tol
    );
    PLOG(debug) << g_msg->message("ip = " + ip->printString());

    trim(s->curveOffset()->vertices(), ip, 1);

    PLOG(debug) << g_msg->message("curve base:");
    s->getBaseline()->print();
    PLOG(debug) << g_msg->message("curve offset:");
    s->curveOffset()->print();

    // Adjust linking indices
    int ls_i_base = static_cast<int>(s->getBaseline()->vertices().size());
    int ls_i_offset = ls_i1;
    int _tmp_nv_offset = static_cast<int>(s->curveOffset()->vertices().size());

    s->printBaseOffsetPairs(g_msg);
    adjustPairsAfterTrimTail(
      s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset,
      false, ls_i_base, _tmp_nv_offset);
    s->printBaseOffsetPairs(g_msg);

  }

  PLOG(debug) << g_msg->message("curve base:");
  s->getBaseline()->print();
  PLOG(debug) << g_msg->message("curve offset:");
  s->curveOffset()->print();
  s->printBaseOffsetPairs(g_msg);

  if (e == 0) {
    bcfg.dcel->addEdge(s->getBaseline()->vertices().front(),
                             s->curveOffset()->vertices().front());
    s->setHeadVertexOffset(s->curveOffset()->vertices().front());
  }
  else if (e == 1) {
    bcfg.dcel->addEdge(s->getBaseline()->vertices().back(),
                             s->curveOffset()->vertices().back());
    s->setTailVertexOffset(s->curveOffset()->vertices().back());
  }
}
