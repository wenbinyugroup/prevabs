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
    double tol, PDCEL *dcel)
{
  double u1 = (e == 0) ? -INF : INF;
  int ls_i_prev = (e == 0) ? -1 : static_cast<int>(vertices.size());
  double u1_tmp, u2_tmp; int ls_i_tmp;
  PDCELHalfEdge *he_tool = nullptr;

  for (auto hel : hels) {
    if (!dcel->isLoopKept(hel)) {
      PDCELHalfEdge *he = findCurveLoopIntersection(
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

// Returns the vertex at parametric position u2 on he's line segment.
// Splits the DCEL edge at that position if u2 is not at an endpoint.
static PDCELVertex *getOrSplitVertex(
    PDCELHalfEdge *he, double u2, double tol, PDCEL *dcel)
{
  if (fabs(u2) <= tol) return he->source();
  if (fabs(1.0 - u2) <= tol) return he->target();
  PDCELVertex *v = he->toLineSegment()->getParametricVertex(u2);
  return dcel->splitEdge(he, v);
}

// Finds the half-edge loops from already-built components that could
// intersect a segment whose interior reference point is ref_vertex.
// Returns: the enclosing outer loop (if not infinite) plus all inner
// loops found inside it.
static std::vector<PDCELHalfEdgeLoop *> collectCandidateLoops(
    PDCELVertex *ref_vertex, PDCEL *dcel)
{
  std::vector<PDCELHalfEdgeLoop *> hels;

  // Temporarily register ref_vertex if not already in the DCEL.
  bool to_be_removed = false;
  if (!ref_vertex->isRegistered()) {
    dcel->addVertex(ref_vertex);
    to_be_removed = true;
  }

  // Step 1: find the loop enclosing ref_vertex.
  PDCELHalfEdgeLoop *outer = dcel->findEnclosingLoop(ref_vertex);
  if (outer != dcel->halfedgeloops().front()) {
    hels.push_back(outer);
  }

  // Step 2: collect inner loops inside the enclosing face.
  PDCELFace *face = outer->face();
  if (face != nullptr) {
    if (face->inners().empty()) {
      // Fallback: link loops and assign faces manually.
      dcel->linkHalfEdgeLoops();
      for (auto heli : dcel->halfedgeloops()) {
        if (!dcel->isLoopKept(heli)) {
          PDCELHalfEdgeLoop *helj = heli;
          while (dcel->adjacentLoop(helj) != nullptr) {
            helj = dcel->adjacentLoop(helj);
          }
          if (helj == outer) {
            heli->setFace(face);
            face->addInnerComponent(heli->incidentEdge());
          }
        }
      }
    }

    for (auto he : face->inners()) {
      hels.push_back(he->loop());
    }
  }

  if (to_be_removed) {
    dcel->removeVertex(ref_vertex);
  }

  return hels;
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

// Adjusts base-offset index pairs for the 2-segment style-1 join after
// intersecting and trimming the offset curve against the shared bound.
static void adjustLinkingIndicesStyle1(
    Segment *seg, int end, int ls_i, int is_new)
{
  std::vector<std::vector<int>> &pairs = seg->baseOffsetIndicesPairs();
  const bool extending =
      seg->curveOffset()->vertices().size() > pairs.back()[1] + 1;

  if (is_new) {
    if (end == 0) {
      if (extending) {
        for (auto &pair : pairs) {
          pair[1]++;
        }
        pairs.insert(pairs.begin(), {0, 0});
      } else {
        while (true) {
          const int id_offset_tmp = pairs.front()[1];
          if (id_offset_tmp > (ls_i - 1)) {
            break;
          }
          pairs.erase(pairs.begin());
        }

        for (auto &pair : pairs) {
          pair[1] -= (ls_i - 1);
        }

        std::vector<std::vector<int>>::iterator it_tmp = pairs.begin();
        const int id_base_tmp = pairs.front()[0];
        for (int k = 0; k < id_base_tmp; k++) {
          it_tmp = pairs.insert(it_tmp, {id_base_tmp - 1 - k, 0});
        }
      }
    } else if (end == 1) {
      if (extending) {
        pairs.push_back({pairs.back()[0], pairs.back()[1] + 1});
      } else {
        while (true) {
          const int id_offset_tmp = pairs.back()[1];
          if (id_offset_tmp < ls_i) {
            break;
          }
          pairs.pop_back();
        }

        for (int k = pairs.back()[0] + 1;
             k < seg->curveBase()->vertices().size(); k++) {
          pairs.push_back({k, ls_i});
        }
      }
    }
  } else {
    if (end == 0) {
      for (auto &pair : pairs) {
        if (pair[1] <= ls_i) {
          pair[1] = 0;
        } else {
          pair[1] -= ls_i;
        }
      }
    } else if (end == 1) {
      for (auto &pair : pairs) {
        if (pair[1] > ls_i) {
          pair[1] = ls_i;
        }
      }
    }
  }
}

// Intersects a segment's offset curve with the shared style-1 bound,
// resolves the chosen intersection vertex, and trims the offset curve.
static PDCELVertex *intersectAndTrimOffsetWithBound(
    Segment *seg, int end,
    std::vector<PDCELVertex *> &bound,
    int &ls_i, double &ls_bu, int &is_new)
{
  std::vector<int> i1s, ibs;
  std::vector<double> u1s, ubs;
  int j1 = 0;
  int ls_bi = 0;
  double ls_u = 0.0;
  int is_new_b = 0;

  findAllIntersections(
      seg->curveOffset()->vertices(), bound, i1s, ibs, u1s, ubs);
  ls_u = getIntersectionLocation(
      seg->curveOffset()->vertices(), i1s, u1s, end, 0, ls_i, j1);
  ls_bu = ubs[j1];
  ls_bi = ibs[j1];

  PDCELVertex *v_new = getIntersectionVertex(
      seg->curveOffset()->vertices(), bound,
      ls_i, ls_bi, ls_u, ls_bu, is_new, is_new_b, TOLERANCE);
  trimCurveAtVertex(seg->curveOffset()->vertices(), v_new,
                    end == 0 ? CurveEnd::Begin : CurveEnd::End);

  PLOG(debug) << "  ls_i = " + std::to_string(ls_i);
  PLOG(debug) << "  intersection vertex for " + seg->getName()
              + ": " + v_new->printString();
  return v_new;
}

// Handles the step-like 2-segment joint by intersecting each segment's
// offset curve with the shared angle-bisector bound and ordering the
// resulting bound vertices.
static void joinStyle1(
    Segment *s1, Segment *s2, int e1, int e2, PDCELVertex *v,
    PDCELVertex *&v1_new, PDCELVertex *&v2_new,
    std::vector<PDCELVertex *> &bound_vertices)
{
  SVector3 t1 = e1 == 0 ? s1->getBeginTangent() : s1->getEndTangent();
  SVector3 t2 = e2 == 0 ? s2->getBeginTangent() : s2->getEndTangent();
  SVector3 b = calcAngleBisectVector(
      t1, t2, s1->getLayupside(), s2->getLayupside());

  PDCELVertex *tmp_v = new PDCELVertex((v->point() + b).point());
  // getIntersectionVertex mutates both input curves by inserting/replacing
  // the resolved intersection vertex. Keep one bound copy per segment so the
  // second intersection sees the same 2-vertex bisector as the original code.
  std::vector<PDCELVertex *> tmp_bound_1;
  std::vector<PDCELVertex *> tmp_bound_2;
  tmp_bound_1.push_back(v);
  tmp_bound_1.push_back(tmp_v);
  tmp_bound_2.push_back(v);
  tmp_bound_2.push_back(tmp_v);

  int ls_i1 = 0, ls_i2 = 0;
  double ls_bu1 = 0.0, ls_bu2 = 0.0;
  int is_new_1 = 0, is_new_2 = 0;

  v1_new = intersectAndTrimOffsetWithBound(
      s1, e1, tmp_bound_1, ls_i1, ls_bu1, is_new_1);
  adjustLinkingIndicesStyle1(s1, e1, ls_i1, is_new_1);

  v2_new = intersectAndTrimOffsetWithBound(
      s2, e2, tmp_bound_2, ls_i2, ls_bu2, is_new_2);
  adjustLinkingIndicesStyle1(s2, e2, ls_i2, is_new_2);

  if (fabs(ls_bu1 - ls_bu2) < TOLERANCE) {
    v2_new = v1_new;
    bound_vertices.push_back(v1_new);

    if (e2 == 0) {
      s2->curveOffset()->vertices()[0] = v1_new;
    } else if (e2 == 1) {
      s2->curveOffset()->vertices()[
          s2->curveOffset()->vertices().size() - 1] = v1_new;
    }
  } else {
    if (ls_bu1 < ls_bu2) {
      bound_vertices.push_back(v1_new);
      bound_vertices.push_back(v2_new);
    } else {
      bound_vertices.push_back(v2_new);
      bound_vertices.push_back(v1_new);
    }
  }
}

// Sweeps outward from the requested ends of two offset curves and finds the
// first acceptable intersection between their line segments.
static bool findOffsetCurvesIntersection(
    Segment *s1, Segment *s2, int e1, int e2,
    std::vector<PGeoLineSegment *> &lss1_out,
    std::vector<PGeoLineSegment *> &lss2_out,
    PGeoLineSegment *&ls1_out, PGeoLineSegment *&ls2_out,
    double &u1_out, double &u2_out,
    int &i1_out, int &i2_out)
{
  PDCELVertex *v11 = nullptr;
  PDCELVertex *v12 = nullptr;
  PDCELVertex *v21 = nullptr;
  PDCELVertex *v22 = nullptr;
  const int n1 = static_cast<int>(s1->curveOffset()->vertices().size());
  const int n2 = static_cast<int>(s2->curveOffset()->vertices().size());
  bool found = false;

  i1_out = 0;
  i2_out = 0;
  ls1_out = nullptr;
  ls2_out = nullptr;

  while (true) {
    ls1_out = nullptr;
    ls2_out = nullptr;

    if (lss1_out.size() < n1 - 1) {
      if (e1 == 0) {
        v11 = s1->curveOffset()->vertices()[i1_out];
        v12 = s1->curveOffset()->vertices()[i1_out + 1];
      } else {
        v11 = s1->curveOffset()->vertices()[n1 - 1 - i1_out];
        v12 = s1->curveOffset()->vertices()[n1 - 2 - i1_out];
      }
      ls1_out = new PGeoLineSegment(v11, v12);
      lss1_out.push_back(ls1_out);
    }

    if (lss2_out.size() < n2 - 1) {
      if (e2 == 0) {
        v21 = s2->curveOffset()->vertices()[i2_out];
        v22 = s2->curveOffset()->vertices()[i2_out + 1];
      } else {
        v21 = s2->curveOffset()->vertices()[n2 - 1 - i2_out];
        v22 = s2->curveOffset()->vertices()[n2 - 2 - i2_out];
      }
      ls2_out = new PGeoLineSegment(v21, v22);
      lss2_out.push_back(ls2_out);
    }

    ls1_out = lss1_out.back();
    for (int i = 0; i < lss2_out.size(); ++i) {
      ls2_out = lss2_out[i];
      calcLineIntersection2D(ls1_out, ls2_out, u1_out, u2_out, TOLERANCE);
      if (u1_out >= 0 && u1_out <= 1 && u2_out >= 0 && u2_out <= 1) {
        found = true;
        i2_out = i;
        break;
      }
      if (ls1_out == lss1_out.front() || ls2_out == lss2_out.front()) {
        if (ls1_out == lss1_out.front() && u1_out < 0 &&
            ls2_out == lss2_out.front() && u2_out < 0) {
          found = true;
          i2_out = i;
          break;
        }
        if (ls1_out == lss1_out.front() && u1_out < 0 &&
            u2_out >= 0 && u2_out <= 1) {
          found = true;
          i2_out = i;
          break;
        }
        if (ls2_out == lss2_out.front() && u2_out < 0 &&
            u1_out >= 0 && u1_out <= 1) {
          found = true;
          i2_out = i;
          break;
        }
      }
    }

    if (!found) {
      ls2_out = lss2_out.back();
      for (int i = 0; i < lss1_out.size(); ++i) {
        ls1_out = lss1_out[i];
        calcLineIntersection2D(ls1_out, ls2_out, u1_out, u2_out, TOLERANCE);
        if (u1_out >= 0 && u1_out <= 1 && u2_out >= 0 && u2_out <= 1) {
          found = true;
          i1_out = i;
          break;
        }
        if (ls1_out == lss1_out.front() || ls2_out == lss2_out.front()) {
          if (ls1_out == lss1_out.front() && u1_out < 0 &&
              ls2_out == lss2_out.front() && u2_out < 0) {
            found = true;
            i1_out = i;
            break;
          }
          if (ls1_out == lss1_out.front() && u1_out < 0 &&
              u2_out >= 0 && u2_out <= 1) {
            found = true;
            i1_out = i;
            break;
          }
          if (ls2_out == lss2_out.front() && u2_out < 0 &&
              u1_out >= 0 && u1_out <= 1) {
            found = true;
            i1_out = i;
            break;
          }
        }
      }
    }

    if (found || (i1_out == n1 - 2 && i2_out == n2 - 2)) {
      break;
    }

    if (i1_out < n1 - 2) {
      i1_out++;
    }
    if (i2_out < n2 - 2) {
      i2_out++;
    }
  }

  return found;
}

// Converts the winning style-2 segment parameters into the concrete
// intersection vertex and updates the segment counters to vertex indices.
static PDCELVertex *resolveIntersectionParams(
    PGeoLineSegment *ls1, double u1_tmp, double u2_tmp,
    int &i1, int &i2)
{
  PDCELVertex *v_intersect = nullptr;

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

  return v_intersect;
}

// Rebuilds both offset baselines after the style-2 intersection vertex
// and vertex indices have been resolved.
static void buildTrimmedOffsetBaselines(
    Segment *s1, Segment *s2, int e1, int e2,
    PDCELVertex *v_intersect, int i1, int i2)
{
  const int n1 = static_cast<int>(s1->curveOffset()->vertices().size());
  const int n2 = static_cast<int>(s2->curveOffset()->vertices().size());

  Baseline *bl1_off_new = new Baseline();
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
  s1->setCurveOffset(bl1_off_new);

  Baseline *bl2_off_new = new Baseline();
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
  s2->setCurveOffset(bl2_off_new);
}

// Handles the non-step 2-segment joint by finding the offset-curve
// intersection, resolving the vertex, and rebuilding both offset baselines.
static bool joinStyle2(
    Segment *s1, Segment *s2, int e1, int e2,
    PDCELVertex *&v1_new, PDCELVertex *&v2_new,
    std::vector<PDCELVertex *> &bound_vertices)
{
  double u1_tmp = 0.0;
  double u2_tmp = 0.0;
  int i1 = 0;
  int i2 = 0;
  PGeoLineSegment *ls1 = nullptr;
  PGeoLineSegment *ls2 = nullptr;
  std::vector<PGeoLineSegment *> lss1;
  std::vector<PGeoLineSegment *> lss2;

  bool found = findOffsetCurvesIntersection(
      s1, s2, e1, e2, lss1, lss2, ls1, ls2, u1_tmp, u2_tmp, i1, i2);

  if (!found) {
    for (auto p : lss1) delete p;
    for (auto p : lss2) delete p;
    PLOG(debug) << "style 2: no intersection found between offset curves, skipping join.";
    return false;
  }

  PDCELVertex *v_intersect =
      resolveIntersectionParams(ls1, u1_tmp, u2_tmp, i1, i2);

  for (auto p : lss1) delete p;
  for (auto p : lss2) delete p;

  buildTrimmedOffsetBaselines(s1, s2, e1, e2, v_intersect, i1, i2);

  v1_new = v_intersect;
  v2_new = v_intersect;
  bound_vertices.push_back(v_intersect);
  return true;
}
 
void PComponent::joinSegments(Segment *s, int e, PDCELVertex * /*v*/, const BuilderConfig &bcfg) {
  MESSAGE_SCOPE(g_msg);

  PLOG(debug) << "making segment end: " + s->getName() + " "
              + std::to_string(e);
  PLOG(debug) << "number of dependencies: "
              + std::to_string(_dependencies.size());

  // Phase 0: shortcuts.
  PLOG(debug) << "the end to be done (e): " + std::to_string(e);
  PLOG(debug) << "free end of the segment (s->free()): "
              + std::to_string(s->free());
  if (_dependencies.empty() || e == s->free()) {
    if (_dependencies.empty()) {
      PLOG(debug) << "making segment end because of no dependency.";
    } else {
      PLOG(debug) << "making segment end because of free end set although "
                     "with dependency.";
    }
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  // Phase 1: initialize reference vertex.
  if (_ref_vertex == nullptr) {
    _ref_vertex = _segments[0]->curveBase()->refVertex();
    if (_ref_vertex == nullptr) {
      int i = static_cast<int>(_segments[0]->curveBase()->vertices().size() / 2);
      _ref_vertex = _segments[0]->curveBase()->vertices()[i];
    }
  }
  PLOG(debug) << "ref vertex: " + _ref_vertex->printString();

  // Phase 2: collect candidate DCEL loops from dependency components.
  PLOG(debug) << "step 1: find the outer half edge loop";
  PLOG(debug) << "step 2: find all inner half edge loops";
  std::vector<PDCELHalfEdgeLoop *> hels =
      collectCandidateLoops(_ref_vertex, bcfg.dcel);

  if (bcfg.debug) {
    std::cout << "\nhels:\n";
    PLOG(debug) << "found half edge loops";
    for (auto hel : hels) {
      hel->log();
    }
  }

  // Phase 3: intersect base curve.
  PLOG(debug) << "step 3: calculate intersections";
  PLOG(debug) << "step 3.1: for the base curve";

  double u1 = 0.0, u2 = 0.0;
  int ls_i_prev = 0;
  PDCELHalfEdge *he_tool = findBestIntersection(
      s->curveBase()->vertices(), hels, e,
      static_cast<int>(s->curveBase()->vertices().size()) - 2,
      u1, u2, ls_i_prev, TOLERANCE, bcfg.dcel);

  PLOG(debug) << "  u1 = " + std::to_string(u1);
  PLOG(debug) << "  u2 = " + std::to_string(u2);

  if (fabs(u1) == INF) {
    PLOG(debug) << "making segment end because of not touching any other "
                   "components although with dependency.";
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  PDCELVertex *v_base = getOrSplitVertex(he_tool, u2, TOLERANCE, bcfg.dcel);
  PLOG(debug) << "  v_new = " << v_base->printString();

  // Convert segment index -> vertex index: +1 at the tail end or when
  // u1 lands on the far endpoint of the intersected segment.
  int ls_i_base = ls_i_prev + ((fabs(1.0 - u1) <= TOLERANCE || e == 1) ? 1 : 0);
  PLOG(debug) << "  ls_i = " + std::to_string(ls_i_base);

  if (e == 0) {
    for (int k = 0; k < ls_i_base; ++k) {
      s->curveBase()->vertices().erase(s->curveBase()->vertices().begin());
    }
    s->curveBase()->vertices()[0] = v_base;
  } else {
    int n = static_cast<int>(s->curveBase()->vertices().size());
    for (int k = ls_i_base; k < n - 1; ++k) {
      s->curveBase()->vertices().pop_back();
    }
    s->curveBase()->vertices().back() = v_base;
  }

  // Phase 4: intersect offset curve.
  PLOG(debug) << "step 3.2: for the offset curve";

  he_tool = findBestIntersection(
      s->curveOffset()->vertices(), hels, e,
      static_cast<int>(s->curveOffset()->vertices().size()) - 2,
      u1, u2, ls_i_prev, TOLERANCE, bcfg.dcel);

  PLOG(debug) << "  u1 = " + std::to_string(u1);
  PLOG(debug) << "  u2 = " + std::to_string(u2);

  if (fabs(u1) == INF) {
    PLOG(debug) << "offset curve: no intersection found, using free end.";
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  PDCELVertex *v_offset = getOrSplitVertex(he_tool, u2, TOLERANCE, bcfg.dcel);
  PLOG(debug) << "  v_new = " << v_offset->printString();

  // Convert segment index -> vertex index: +1 at the tail end or when
  // u1 lands on the far endpoint of the intersected segment.
  int ls_i_offset = ls_i_prev + ((fabs(1.0 - u1) <= TOLERANCE || e == 1) ? 1 : 0);
  PLOG(debug) << "  ls_i = " + std::to_string(ls_i_offset);

  if (e == 0) {
    for (int k = 0; k < ls_i_offset; ++k) {
      s->curveOffset()->vertices().erase(s->curveOffset()->vertices().begin());
    }
    s->curveOffset()->vertices()[0] = v_offset;
    s->setHeadVertexOffset(v_offset);
  } else {
    int n = static_cast<int>(s->curveOffset()->vertices().size());
    for (int k = ls_i_offset; k < n - 1; ++k) {
      s->curveOffset()->vertices().pop_back();
    }
    s->curveOffset()->vertices().back() = v_offset;
    s->setTailVertexOffset(v_offset);
  }

  // Phase 5: adjust base-offset index pairs.
  PLOG(debug) << "adjust base-offset linking indices";
  int nv_base = static_cast<int>(s->curveBase()->vertices().size());
  int nv_offset = static_cast<int>(s->curveOffset()->vertices().size());
  PLOG(debug) << "  nv_base = " + std::to_string(nv_base);
  PLOG(debug) << "  nv_offset = " + std::to_string(nv_offset);

  if (e == 0) {
    adjustPairsAfterTrimHead(
        s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset, true);
  } else {
    adjustPairsAfterTrimTail(
        s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset, true, -1, -1);
  }

  s->printBaseOffsetPairs();
}

void PComponent::joinSegments(
  Segment *s1, Segment *s2, int e1, int e2,
  PDCELVertex *v, int style, const BuilderConfig &bcfg
  ) {
  MESSAGE_SCOPE(g_msg);

  PLOG(debug) <<
    "joining segments ends: "
    + s1->getName() + " " + std::to_string(e1) + ", "
    + s2->getName() + " " + std::to_string(e2)
  ;

  PDCELVertex *v1_new = nullptr, *v2_new = nullptr;
  std::vector<PDCELVertex *> bound_vertices;
  bound_vertices.push_back(v);

  if (style == 1) {
    joinStyle1(s1, s2, e1, e2, v, v1_new, v2_new, bound_vertices);
  }
  else if (style == 2) {
    if (!joinStyle2(s1, s2, e1, e2, v1_new, v2_new, bound_vertices)) {
      return;
    }
  }

  if (e1 == 0) s1->setHeadVertexOffset(v1_new);
  else if (e1 == 1) s1->setTailVertexOffset(v1_new);

  if (e2 == 0) s2->setHeadVertexOffset(v2_new);
  else if (e2 == 1) s2->setTailVertexOffset(v2_new);

  for (int i = 0; i < static_cast<int>(bound_vertices.size()) - 1; ++i) {
    bcfg.dcel->addEdge(bound_vertices[i], bound_vertices[i + 1]);
  }

  PLOG(debug) << "  base -- offset index pairs of segment: " + s1->getName();
  for (auto i = 0; i < s1->baseOffsetIndicesPairs().size(); i++) {
    PLOG(debug) <<
      "  " + std::to_string(s1->baseOffsetIndicesPairs()[i][0]) + " : "
      + s1->curveBase()->vertices()[s1->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s1->baseOffsetIndicesPairs()[i][1]) + " : "
      + s1->curveOffset()->vertices()[s1->baseOffsetIndicesPairs()[i][1]]->printString()
    ;
  }

  PLOG(debug) << "  base -- offset index pairs of segment: " + s2->getName();
  for (auto i = 0; i < s2->baseOffsetIndicesPairs().size(); i++) {
    PLOG(debug) <<
      "  " + std::to_string(s2->baseOffsetIndicesPairs()[i][0]) + " : "
      + s2->curveBase()->vertices()[s2->baseOffsetIndicesPairs()[i][0]]->printString()
      + " -- " + std::to_string(s2->baseOffsetIndicesPairs()[i][1]) + " : "
      + s2->curveOffset()->vertices()[s2->baseOffsetIndicesPairs()[i][1]]->printString()
    ;
  }

}

void PComponent::createSegmentFreeEnd(Segment *s, int e, const BuilderConfig &bcfg) {
    PLOG(debug) << "createSegmentFreeEnd: " + s->getName() + " " + std::to_string(e);

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
    ip = getIntersectionVertex(
      s->curveOffset()->vertices(), b, ls_i1, ls_i2, u1, u2,
      is_new_1, is_new_2, TOLERANCE
    );
        PLOG(debug) << "ip = " + ip->printString();

    trimCurveAtVertex(s->curveOffset()->vertices(), ip, CurveEnd::Begin);

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
    ip = getIntersectionVertex(
      s->curveOffset()->vertices(), b, ls_i1, ls_i2, u1, u2,
      is_new_1, is_new_2, TOLERANCE
    );
        PLOG(debug) << "ip = " + ip->printString();

    trimCurveAtVertex(s->curveOffset()->vertices(), ip, CurveEnd::End);

        PLOG(debug) << "curve base:";
    s->getBaseline()->print();
        PLOG(debug) << "curve offset:";
    s->curveOffset()->print();

    // Adjust linking indices
    int ls_i_base = static_cast<int>(s->getBaseline()->vertices().size());
    int ls_i_offset = ls_i1;
    int _tmp_nv_offset = static_cast<int>(s->curveOffset()->vertices().size());

    s->printBaseOffsetPairs();
    adjustPairsAfterTrimTail(
      s->baseOffsetIndicesPairs(), ls_i_base, ls_i_offset,
      false, ls_i_base, _tmp_nv_offset);
    s->printBaseOffsetPairs();

  }

    PLOG(debug) << "curve base:";
  s->getBaseline()->print();
    PLOG(debug) << "curve offset:";
  s->curveOffset()->print();
  s->printBaseOffsetPairs();

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
