#include "PComponent.hpp"

#include "Material.hpp"
#include "PDCEL.hpp"
#include "PGeoClasses.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "joinHelpers.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>

// Convention used throughout this file:
//   e == 0  →  head (beginning) end of the curve
//   e == 1  →  tail (end) end of the curve
//   u       →  parametric position on a line segment, u ∈ [0, 1]
//   ls_i    →  line-segment index (0-based) in a polyline
//   BaseOffsetMap: staircase mapping from base-curve vertex indices to
//                  offset-curve vertex indices, see geo_types.hpp.

namespace {

using LineSegmentPtr = std::unique_ptr<PGeoLineSegment>;

// Validates the staircase invariant on a BaseOffsetMap and asserts on
// violation. Used as a guard after every structural edit.
void assertValidBaseOffsetMap(
    const BaseOffsetMap &map, const std::string &caller)
{
  std::string error_message;
  const bool valid = validateBaseOffsetMap(map, &error_message);
  if (!valid) {
    PLOG(error) << caller + ": invalid BaseOffsetMap: " + error_message;
  }
  assert(valid && "BaseOffsetMap staircase invariant violated");
}




// Decides whether the parametric intersection (u1, u2) between two offset
// line segments is geometrically acceptable.
//
// Normal case: both u1 and u2 in [0, 1] → always acceptable.
//
// Edge case: when one or both segments are the first in their respective
// lists (i.e. closest to the curve endpoint being scanned), a small backward
// extrapolation (u < 0) is permitted.  This handles curves whose offset
// endpoints are slightly inside each other so the segments themselves don't
// formally overlap but their extensions do.
static bool isAcceptableOffsetIntersection(
    PGeoLineSegment *ls1, PGeoLineSegment *ls2,
    const std::vector<LineSegmentPtr> &lss1,
    const std::vector<LineSegmentPtr> &lss2,
    double u1, double u2)
{
  // Both parameters within the segments → clean intersection.
  if (u1 >= 0 && u1 <= 1 && u2 >= 0 && u2 <= 1) {
    return true;
  }

  // Allow one or both to extrapolate backward only at the very first segment.
  if (ls1 == lss1.front().get() || ls2 == lss2.front().get()) {
    if (ls1 == lss1.front().get() && u1 < 0 &&
        ls2 == lss2.front().get() && u2 < 0) {
      return true;
    }
    if (ls1 == lss1.front().get() && u1 < 0 &&
        u2 >= 0 && u2 <= 1) {
      return true;
    }
    if (ls2 == lss2.front().get() && u2 < 0 &&
        u1 >= 0 && u1 <= 1) {
      return true;
    }
  }

  return false;
}




// Scans every segment in moving_list against fixed_ls and returns the first
// acceptable intersection found.  Sets matched_index to the winning position
// in moving_list and u1_out / u2_out to the parametric parameters.
static bool scanOffsetIntersectionCandidates(
    PGeoLineSegment *&fixed_ls,
    PGeoLineSegment *&moving_ls,
    const std::vector<LineSegmentPtr> &fixed_list,
    const std::vector<LineSegmentPtr> &moving_list,
    double &u1_out, double &u2_out,
    int &matched_index)
{
  for (int i = 0; i < moving_list.size(); ++i) {
    moving_ls = moving_list[i].get();
    calcLineIntersection2D(fixed_ls, moving_ls, u1_out, u2_out, TOLERANCE);
    if (isAcceptableOffsetIntersection(
            fixed_ls, moving_ls, fixed_list, moving_list, u1_out, u2_out)) {
      matched_index = i;
      return true;
    }
  }
  return false;
}

} // namespace




// Compares two candidate intersections when searching from end `e` of a
// polyline.  Returns true if (ls_i_tmp, u1_tmp) is a better intersection than
// the current best (ls_i_prev, u1).
//
// Search direction:
//   e == 0 (head): we want the intersection that is deepest into the curve,
//                  i.e. largest segment index and largest u within a segment.
//   e == 1 (tail): we want the intersection closest to the tail,
//                  i.e. smallest segment index and smallest u.
//
// Special-case: if the intersection falls on the first (e==0) or last (e==1)
// segment outside its [0,1] range, it is handled by the staircase-entry
// promotion logic in the caller.
//
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
    // Head search: prefer higher segment index, then higher u.
    return (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1)
        || (u_on_seg && ls_i_tmp > ls_i_prev)
        || (u_on_seg && ls_i_tmp == ls_i_prev && u1_tmp > u1);
  } else {
    // Tail search: prefer lower segment index, then lower u.
    return (ls_i_tmp == last_seg_idx && u1_tmp > 1 && u1_tmp < u1)
        || (u_on_seg && ls_i_tmp < ls_i_prev)
        || (u_on_seg && ls_i_tmp == ls_i_prev && u1_tmp < u1);
  }
}




// Iterates over all non-kept half-edge loops from `hels` and finds the one
// that best intersects the polyline defined by `vertices`, scanning from
// end `e`.
//
// Returns the winning half-edge (nullptr if no intersection found).
// Outputs u1_out / u2_out: parametric positions on the curve / half-edge.
// ls_i_out: winning segment index in `vertices`.
//
// `hels` is already filtered to the dependency-component loops that may clip
// the current segment end, so both kept and non-kept loops are considered
// here. This is required for closed shell dependencies whose relevant trim
// boundary may be the kept inner loop rather than the exterior temp loop.
//
// last_seg_idx: (vertices.size()-1) for base curve, (vertices.size()-2) for offset.
static PDCELHalfEdge *findBestIntersection(
    const std::vector<PDCELVertex *> &vertices,
    const std::vector<PDCELHalfEdgeLoop *> &hels,
    int e, int last_seg_idx,
    double &u1_out, double &u2_out, int &ls_i_out,
    double tol)
{
  // Initialise to sentinel: -INF means "no candidate yet" for head search,
  // +INF means "no candidate yet" for tail search.
  double u1 = (e == 0) ? -INF : INF;
  int ls_i_prev = (e == 0) ? -1 : static_cast<int>(vertices.size());
  double u1_tmp, u2_tmp; int ls_i_tmp;
  PDCELHalfEdge *he_tool = nullptr;

  for (auto hel : hels) {
    PDCELHalfEdge *he = findCurveLoopIntersection(
        vertices, hel, e, ls_i_tmp, u1_tmp, u2_tmp, tol);
    if (he != nullptr &&
        isBetterIntersection(e, ls_i_tmp, u1_tmp,
                             ls_i_prev, u1, last_seg_idx, tol)) {
      // New best candidate found.
      u1 = u1_tmp; u2_out = u2_tmp; he_tool = he; ls_i_prev = ls_i_tmp;
    }
  }

  ls_i_out = ls_i_prev;
  u1_out = u1;
  return he_tool;
}




// Returns the DCEL vertex at parametric position u2 on he's line segment.
// If u2 is at an endpoint (within tol), returns the existing endpoint vertex.
// Otherwise, splits the DCEL edge at u2 and returns the new split vertex.
static PDCELVertex *getOrSplitVertex(
    PDCELHalfEdge *he, double u2, double tol, PDCEL *dcel)
{
  if (fabs(u2) <= tol) return he->source();
  if (fabs(1.0 - u2) <= tol) return he->target();
  PDCELVertex *v = he->toLineSegment()->getParametricVertex(u2);
  return dcel->splitEdge(he, v);
}




// Collects the half-edge loops from already-built dependency components that
// a segment could potentially intersect.
//
// We gather both the face loop itself and its twin loop. Closed shell
// dependencies can expose either side as the correct trim boundary:
// a web inside a box must clip against the shell's inner contour, while other
// configurations still need the exterior loop. The downstream intersection
// ranking picks the geometrically deepest hit from the requested end.
static std::vector<PDCELHalfEdgeLoop *> collectCandidateLoops(
    const std::list<PComponent *> &dependencies)
{
  std::vector<PDCELHalfEdgeLoop *> hels;
  auto add_loop_once = [&hels](PDCELHalfEdgeLoop *loop) {
    if (loop == nullptr) {
      return;
    }
    if (std::find(hels.begin(), hels.end(), loop) == hels.end()) {
      hels.push_back(loop);
    }
  };

  for (auto dep : dependencies) {
    for (auto seg : dep->segments()) {
      PDCELFace *face = seg->face();
      if (face == nullptr || face->outer() == nullptr) continue;
      add_loop_once(face->outer()->loop());

      PDCELHalfEdge *he_twin = face->outer()->twin();
      if (he_twin == nullptr) continue;
      add_loop_once(he_twin->loop());
    }
  }

  return hels;
}




// Stores the cap (termination) vertex indices for a tail-trim operation.
// When both caps are present they prevent the staircase filler loop from
// overshooting the already-computed endpoint.
struct TailTrimCaps {
  bool has_base_cap;
  bool has_offset_cap;
  int base_cap;
  int offset_cap;

  TailTrimCaps()
      : has_base_cap(false), has_offset_cap(false),
        base_cap(0), offset_cap(0) {}

  TailTrimCaps(int base_cap_value, int offset_cap_value)
      : has_base_cap(true), has_offset_cap(true),
        base_cap(base_cap_value), offset_cap(offset_cap_value) {}
};




// RAII editor for BaseOffsetMap that validates the staircase invariant after
// every structural operation and reports violations with the caller name.
class BaseOffsetMapEditor {
public:
  explicit BaseOffsetMapEditor(
      BaseOffsetMap &pairs, const std::string &caller)
      : _pairs(pairs), _caller(caller) {}

  // Removes all entries up to (and including) the entries that have base <=
  // ls_i_base (if remove_base) and offset <= ls_i_offset, then re-indexes
  // both axes to start from zero and inserts staircase entries to cover the
  // range [0, max(ls_i_base, ls_i_offset)].
  //
  // Used after the head of a segment is trimmed to an intersection vertex.
  void trimHead(int ls_i_base, int ls_i_offset, bool remove_base)
  {
    BaseOffsetMap::iterator keep_it = _pairs.begin();
    if (remove_base) {
      keep_it = std::find_if(
          _pairs.begin(), _pairs.end(),
          [ls_i_base](const BaseOffsetPair &pair) {
            return pair.base > ls_i_base;
          });
      _pairs.erase(_pairs.begin(), keep_it);
    }
    keep_it = std::find_if(
        _pairs.begin(), _pairs.end(),
        [ls_i_offset](const BaseOffsetPair &pair) {
          return pair.offset > ls_i_offset;
        });
    _pairs.erase(_pairs.begin(), keep_it);

    // Shift all remaining indices so that the new start becomes (0, 0).
    for (auto &p : _pairs) {
      p.base -= ls_i_base;
      p.offset -= ls_i_offset;
    }

    // Insert staircase entries to bridge the gap created by the asymmetric
    // trim (one axis may have been trimmed more than the other).
    int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
    auto it = _pairs.begin();
    for (int k = 0; k < nv_diff; k++) {
      if (ls_i_base > ls_i_offset)
        it = _pairs.insert(it, BaseOffsetPair(0, nv_diff - k));
      else if (ls_i_base < ls_i_offset)
        it = _pairs.insert(it, BaseOffsetPair(nv_diff - k, 0));
    }
    _pairs.insert(it, BaseOffsetPair(0, 0));
    validate("trimHead");
  }

  // Removes all entries from the back that have base >= ls_i_base (if
  // remove_base) or offset >= ls_i_offset, then appends staircase entries up
  // to the trimmed intersection vertex.  The TailTrimCaps guard prevents the
  // filler from running past the already-computed cap vertices.
  //
  // Used after the tail of a segment is trimmed to an intersection vertex.
  void trimTail(
      int ls_i_base, int ls_i_offset, bool remove_base,
      const TailTrimCaps &caps)
  {
    if (remove_base) {
      while (_pairs.back().base >= ls_i_base)
        _pairs.pop_back();
    }
    while (_pairs.back().offset >= ls_i_offset)
      _pairs.pop_back();

    // Append staircase entries to bridge any asymmetric trim on base vs offset.
    int nv_diff = std::max(ls_i_base, ls_i_offset) - std::min(ls_i_base, ls_i_offset);
    int id_base = _pairs.back().base;
    int id_offset = _pairs.back().offset;
    for (int k = 0; k < nv_diff; k++) {
      if (caps.has_base_cap && caps.has_offset_cap
          && id_base == caps.base_cap - 2
          && id_offset == caps.offset_cap - 2)
        break;
      _pairs.push_back(BaseOffsetPair(id_base + 1, id_offset + 1));
      if (ls_i_base > ls_i_offset)      id_base++;
      else if (ls_i_base < ls_i_offset) id_offset++;
    }
    _pairs.push_back(BaseOffsetPair(id_base + 1, id_offset + 1));
    validate("trimTail");
  }

  // Adjusts the BaseOffsetMap after a style-1 (angle-bisector) intersection
  // is resolved on the offset curve only (the base curve is unchanged).
  //
  // is_new: non-zero if a new vertex was inserted into the offset curve
  //         (i.e. the intersection point was not already an endpoint).
  //
  // extending: true when the trimmed offset curve is longer than what the
  //            current map expects — this happens when the bisector clips the
  //            offset curve at a point beyond the existing last vertex.
  void adjustStyle1(
      int end, int ls_i, int is_new,
      std::size_t offset_vertex_count,
      std::size_t base_vertex_count)
  {
    const bool extending =
        offset_vertex_count > static_cast<std::size_t>(_pairs.back().offset + 1);

    if (is_new) {
      if (end == 0) {
        if (extending) {
          // The offset curve grew at the head; shift all offset indices up
          // and prepend a (0, 0) anchor.
          for (auto &pair : _pairs) {
            pair.offset++;
          }
          _pairs.insert(_pairs.begin(), BaseOffsetPair(0, 0));
        } else {
          // The offset curve was trimmed inward at the head; drop entries
          // whose offset index is below the new start and re-index.
          while (true) {
            const int id_offset_tmp = _pairs.front().offset;
            if (id_offset_tmp > (ls_i - 1)) {
              break;
            }
            _pairs.erase(_pairs.begin());
          }

          for (auto &pair : _pairs) {
            pair.offset -= (ls_i - 1);
          }

          // Insert staircase entries so the map still starts at base index 0.
          BaseOffsetMap::iterator it_tmp = _pairs.begin();
          const int id_base_tmp = _pairs.front().base;
          for (int k = 0; k < id_base_tmp; k++) {
            it_tmp = _pairs.insert(
                it_tmp, BaseOffsetPair(id_base_tmp - 1 - k, 0));
          }
        }
      } else if (end == 1) {
        if (extending) {
          // The offset curve grew at the tail; append one more entry.
          _pairs.push_back(
              BaseOffsetPair(_pairs.back().base, _pairs.back().offset + 1));
        } else {
          // The offset curve was trimmed inward at the tail; pop entries
          // beyond the new end and append entries to reach all base vertices.
          while (true) {
            const int id_offset_tmp = _pairs.back().offset;
            if (id_offset_tmp < ls_i) {
              break;
            }
            _pairs.pop_back();
          }

          for (int k = _pairs.back().base + 1;
               k < static_cast<int>(base_vertex_count); k++) {
            _pairs.push_back(BaseOffsetPair(k, ls_i));
          }
        }
      }
    } else {
      // The intersection landed exactly on an existing offset vertex; only
      // clamp the offset indices in the map without inserting anything.
      if (end == 0) {
        for (auto &pair : _pairs) {
          if (pair.offset <= ls_i) {
            pair.offset = 0;
          } else {
            pair.offset -= ls_i;
          }
        }
      } else if (end == 1) {
        for (auto &pair : _pairs) {
          if (pair.offset > ls_i) {
            pair.offset = ls_i;
          }
        }
      }
    }
    validate("adjustStyle1");
  }

private:
  void validate(const char *operation) const
  {
    assertValidBaseOffsetMap(_pairs, _caller + "::" + operation);
  }

  BaseOffsetMap &_pairs;
  std::string _caller;
};




// Intersects the offset curve of `seg` (from end `end`) with the two-vertex
// bound line defined by `bound`, finds the intersection location, inserts or
// reuses the intersection vertex on the offset curve, trims the offset curve
// at that vertex, and returns the vertex.
//
// Outputs:
//   ls_i   – line-segment index in the offset curve at the intersection
//   ls_bu  – parametric position on the bound segment
//   is_new – non-zero if a new vertex was inserted into the offset curve
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




// Style-1 (step) joint: the two offset curves are trimmed against a shared
// angle-bisector bound starting from the common base vertex `v`.
//
// The angle bisector is computed from the tangent vectors of the two segments
// at their respective joined ends.  Each offset curve is then intersected
// independently with that bound.
//
// If both curves hit the same point on the bound, the two new offset vertices
// are unified (v2_new = v1_new).  Otherwise the bound_vertices list records
// both in the order they appear along the bound, so DCEL edges can be added
// between them by the caller.
static void joinStyle1(
    Segment *s1, Segment *s2, int e1, int e2, PDCELVertex *v,
    PDCELVertex *&v1_new, PDCELVertex *&v2_new,
    std::vector<PDCELVertex *> &bound_vertices)
{
  PLOG(debug) << "  joinStyle1: computing angle bisector at " + v->printString();

  SVector3 t1 = e1 == 0 ? s1->getBeginTangent() : s1->getEndTangent();
  SVector3 t2 = e2 == 0 ? s2->getBeginTangent() : s2->getEndTangent();
  SVector3 b = calcAngleBisectVector(
      t1, t2, s1->getLayupside(), s2->getLayupside());

  PLOG(debug) << "  bisector direction: ("
              + std::to_string(b.x()) + ", "
              + std::to_string(b.y()) + ", "
              + std::to_string(b.z()) + ")";

  PDCELVertex tmp_v((v->point() + b).point());
  // Each segment gets its own copy of the 2-vertex bound so that the vertex
  // insertion performed by intersectAndTrimOffsetWithBound on s1's bound does
  // not corrupt s2's bound.
  std::vector<PDCELVertex *> tmp_bound_1;
  std::vector<PDCELVertex *> tmp_bound_2;
  tmp_bound_1.push_back(v);
  tmp_bound_1.push_back(&tmp_v);
  tmp_bound_2.push_back(v);
  tmp_bound_2.push_back(&tmp_v);

  int ls_i1 = 0, ls_i2 = 0;
  double ls_bu1 = 0.0, ls_bu2 = 0.0;
  int is_new_1 = 0, is_new_2 = 0;

  PLOG(debug) << "  intersecting offset of " + s1->getName() + " with bound";
  v1_new = intersectAndTrimOffsetWithBound(
      s1, e1, tmp_bound_1, ls_i1, ls_bu1, is_new_1);
  BaseOffsetMapEditor(s1->baseOffsetIndicesPairs(), "joinStyle1 s1")
      .adjustStyle1(
          e1, ls_i1, is_new_1,
          s1->curveOffset()->vertices().size(),
          s1->curveBase()->vertices().size());

  PLOG(debug) << "  intersecting offset of " + s2->getName() + " with bound";
  v2_new = intersectAndTrimOffsetWithBound(
      s2, e2, tmp_bound_2, ls_i2, ls_bu2, is_new_2);
  BaseOffsetMapEditor(s2->baseOffsetIndicesPairs(), "joinStyle1 s2")
      .adjustStyle1(
          e2, ls_i2, is_new_2,
          s2->curveOffset()->vertices().size(),
          s2->curveBase()->vertices().size());

  // Determine the order of bound vertices based on their position (ls_bu)
  // along the bisector.  If they coincide, unify them to a single vertex.
  if (fabs(ls_bu1 - ls_bu2) < TOLERANCE) {
    PLOG(debug) << "  both offsets hit the same bound point → unifying vertices";
    v2_new = v1_new;
    bound_vertices.push_back(v1_new);

    if (e2 == 0) {
      s2->curveOffset()->vertices()[0] = v1_new;
    } else if (e2 == 1) {
      s2->curveOffset()->vertices()[
          s2->curveOffset()->vertices().size() - 1] = v1_new;
    }
  } else {
    PLOG(debug) << "  ls_bu1 = " + std::to_string(ls_bu1)
                + ", ls_bu2 = " + std::to_string(ls_bu2);
    if (ls_bu1 < ls_bu2) {
      bound_vertices.push_back(v1_new);
      bound_vertices.push_back(v2_new);
    } else {
      bound_vertices.push_back(v2_new);
      bound_vertices.push_back(v1_new);
    }
  }
}




// Sweeps outward from end `e1` / `e2` of the two offset curves and finds the
// first acceptable intersection between their line segments.
//
// The sweep alternates adding one segment from each curve and then checks for
// intersection against the entire opposite list.  It stops as soon as one is
// found or both curves are exhausted.
//
// Outputs:
//   lss1_out / lss2_out – the segments tested from each curve (in sweep order)
//   ls1_out / ls2_out   – the winning segments
//   u1_out / u2_out     – parametric positions on ls1 / ls2
//   i1_out / i2_out     – position index within the sweep list (≈ segment index)
static bool findOffsetCurvesIntersection(
    Segment *s1, Segment *s2, int e1, int e2,
    std::vector<LineSegmentPtr> &lss1_out,
    std::vector<LineSegmentPtr> &lss2_out,
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

    // Build the next segment from curve 1 (from its endpoint outward).
    if (lss1_out.size() < n1 - 1) {
      if (e1 == 0) {
        v11 = s1->curveOffset()->vertices()[i1_out];
        v12 = s1->curveOffset()->vertices()[i1_out + 1];
      } else {
        v11 = s1->curveOffset()->vertices()[n1 - 1 - i1_out];
        v12 = s1->curveOffset()->vertices()[n1 - 2 - i1_out];
      }
      lss1_out.push_back(LineSegmentPtr(new PGeoLineSegment(v11, v12)));
      ls1_out = lss1_out.back().get();
    }

    // Build the next segment from curve 2 (from its endpoint outward).
    if (lss2_out.size() < n2 - 1) {
      if (e2 == 0) {
        v21 = s2->curveOffset()->vertices()[i2_out];
        v22 = s2->curveOffset()->vertices()[i2_out + 1];
      } else {
        v21 = s2->curveOffset()->vertices()[n2 - 1 - i2_out];
        v22 = s2->curveOffset()->vertices()[n2 - 2 - i2_out];
      }
      lss2_out.push_back(LineSegmentPtr(new PGeoLineSegment(v21, v22)));
      ls2_out = lss2_out.back().get();
    }

    // Check the newest segment of curve 1 against all of curve 2's segments.
    ls1_out = lss1_out.back().get();
    found = scanOffsetIntersectionCandidates(
        ls1_out, ls2_out, lss1_out, lss2_out, u1_out, u2_out, i2_out);

    // If not found, check the newest segment of curve 2 against all of curve 1.
    if (!found) {
      ls2_out = lss2_out.back().get();
      found = scanOffsetIntersectionCandidates(
          ls2_out, ls1_out, lss2_out, lss1_out, u2_out, u1_out, i1_out);
    }

    if (found || (i1_out == n1 - 2 && i2_out == n2 - 2)) {
      break;
    }

    // Advance the sweep index for each curve (clamped to the last segment).
    if (i1_out < n1 - 2) {
      i1_out++;
    }
    if (i2_out < n2 - 2) {
      i2_out++;
    }
  }

  return found;
}




// Converts the parametric intersection result (u1_tmp on ls1, u2_tmp on ls2)
// into a concrete DCEL vertex.  Also advances i1 / i2 from line-segment
// indices to vertex indices so they can be used as slice boundaries.
static PDCELVertex *resolveIntersectionParams(
    PGeoLineSegment *ls1, double u1_tmp, double u2_tmp,
    int &i1, int &i2)
{
  PDCELVertex *v_intersect = nullptr;

  if (fabs(u1_tmp) < TOLERANCE) {
    v_intersect = ls1->v1();
  }
  else if (fabs(1 - u1_tmp) < TOLERANCE) {
    v_intersect = ls1->v2();
  }
  else {
    v_intersect = ls1->getParametricVertex(u1_tmp);
  }
  i1 = advanceIntersectionVertexIndex(i1, u1_tmp, TOLERANCE);
  i2 = advanceIntersectionVertexIndex(i2, u2_tmp, TOLERANCE);

  return v_intersect;
}




// Rebuilds both offset baselines after the style-2 intersection vertex has
// been resolved.  Each curve is sliced at v_intersect: the portion from the
// scanned end outward is discarded and v_intersect is inserted as the new
// endpoint.
static void buildTrimmedOffsetBaselines(
    Segment *s1, Segment *s2, int e1, int e2,
    PDCELVertex *v_intersect, int i1, int i2)
{
  const int n1 = static_cast<int>(s1->curveOffset()->vertices().size());
  const int n2 = static_cast<int>(s2->curveOffset()->vertices().size());

  Baseline *bl1_off_new = new Baseline();
  if (e1 == 0) {
    // Head trim: prepend intersection vertex, then keep vertices from i1 onward.
    bl1_off_new->addPVertex(v_intersect);
    for (int i = i1; i < n1; ++i) {
      bl1_off_new->addPVertex(s1->curveOffset()->vertices()[i]);
    }
  }
  else if (e1 == 1) {
    // Tail trim: keep vertices up to (n1-i1), then append intersection vertex.
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




// Style-2 (smooth) joint: finds the direct geometric intersection between the
// two offset curves, trims both curves to that point, and records the shared
// vertex as the joint bound vertex.
//
// Returns false if no acceptable intersection is found (the join is skipped).
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
  std::vector<LineSegmentPtr> lss1;
  std::vector<LineSegmentPtr> lss2;

  PLOG(debug) << "  joinStyle2: searching for offset curve intersection";

  bool found = findOffsetCurvesIntersection(
      s1, s2, e1, e2, lss1, lss2, ls1, ls2, u1_tmp, u2_tmp, i1, i2);

  if (!found) {
    PLOG(debug) << "style 2: no intersection found between offset curves, skipping join.";
    return false;
  }

  PLOG(debug) << "  offset intersection found: u1=" + std::to_string(u1_tmp)
              + " u2=" + std::to_string(u2_tmp)
              + " i1=" + std::to_string(i1)
              + " i2=" + std::to_string(i2);

  PDCELVertex *v_intersect =
      resolveIntersectionParams(ls1, u1_tmp, u2_tmp, i1, i2);

  PLOG(debug) << "  intersection vertex: " + v_intersect->printString();

  buildTrimmedOffsetBaselines(s1, s2, e1, e2, v_intersect, i1, i2);

  v1_new = v_intersect;
  v2_new = v_intersect;
  bound_vertices.push_back(v_intersect);
  return true;
}




// Handles the endpoint (end `e`) of a single segment `s` that borders an
// already-built dependency component.
//
// Algorithm:
//   Phase 0 – shortcuts: no dependencies or free end → call createSegmentFreeEnd.
//   Phase 1 – initialise the interior reference vertex.
//   Phase 2 – collect DCEL half-edge loops from dependency components that
//              could border this segment.
//   Phase 3 – intersect the base curve with those loops; trim the base curve.
//   Phase 4 – intersect the offset curve with those loops; trim the offset curve.
//   Phase 5 – update the BaseOffsetMap to reflect the trimmed curves.
void PComponent::joinSegments(Segment *s, int e, const BuilderConfig &bcfg) {
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
  // The reference vertex is a known interior point of the segment used to
  // locate the enclosing DCEL face in Phase 2.
  if (_laminate.ref_vertex == nullptr) {
    _laminate.ref_vertex = _laminate.segments[0]->curveBase()->refVertex();
    if (_laminate.ref_vertex == nullptr) {
      int i = static_cast<int>(
          _laminate.segments[0]->curveBase()->vertices().size() / 2);
      _laminate.ref_vertex = _laminate.segments[0]->curveBase()->vertices()[i];
    }
  }
  PLOG(debug) << "ref vertex: " + _laminate.ref_vertex->printString();

  // Phase 2: collect boundary loops from every already-built dependency segment.
  PLOG(debug) << "step 2: collect boundary loops from dependency components";
  std::vector<PDCELHalfEdgeLoop *> hels =
      collectCandidateLoops(_dependencies);

  if (bcfg.debug) {
    std::cout << "\nhels:\n";
    PLOG(debug) << "found half edge loops";
    for (auto hel : hels) {
      hel->log();
    }
  }

  // Phase 3: intersect base curve with collected DCEL loops.
  PLOG(debug) << "step 3: calculate intersections";
  PLOG(debug) << "step 3.1: for the base curve";

  double u1 = 0.0, u2 = 0.0;
  int ls_i_prev = 0;
  PDCELHalfEdge *he_tool = findBestIntersection(
      s->curveBase()->vertices(), hels, e,
      static_cast<int>(s->curveBase()->vertices().size()) - 2,
      u1, u2, ls_i_prev, TOLERANCE);

  PLOG(debug) << "  u1 = " + std::to_string(u1);
  PLOG(debug) << "  u2 = " + std::to_string(u2);

  if (fabs(u1) == INF) {
    // No intersection found: the segment does not actually touch any
    // dependency component at this end → treat as a free end.
    PLOG(debug) << "making segment end because of not touching any other "
                   "components although with dependency.";
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  // Obtain or create the base-curve intersection vertex on the DCEL edge.
  PDCELVertex *v_base = getOrSplitVertex(he_tool, u2, TOLERANCE, bcfg.dcel);
  PLOG(debug) << "  v_new = " << v_base->printString();

  // Convert segment index → vertex index.
  // Add 1 at the tail end or when u1 exactly hits the far endpoint of ls_i_prev.
  int ls_i_base = ls_i_prev + ((fabs(1.0 - u1) <= TOLERANCE || e == 1) ? 1 : 0);
  PLOG(debug) << "  ls_i = " + std::to_string(ls_i_base);

  // Trim base curve to the intersection vertex.
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

  // Phase 4: intersect offset curve with collected DCEL loops.
  PLOG(debug) << "step 3.2: for the offset curve";

  he_tool = findBestIntersection(
      s->curveOffset()->vertices(), hels, e,
      static_cast<int>(s->curveOffset()->vertices().size()) - 2,
      u1, u2, ls_i_prev, TOLERANCE);

  PLOG(debug) << "  u1 = " + std::to_string(u1);
  PLOG(debug) << "  u2 = " + std::to_string(u2);

  if (fabs(u1) == INF) {
    PLOG(debug) << "offset curve: no intersection found, using free end.";
    createSegmentFreeEnd(s, e, bcfg);
    return;
  }

  PDCELVertex *v_offset = getOrSplitVertex(he_tool, u2, TOLERANCE, bcfg.dcel);
  PLOG(debug) << "  v_new = " << v_offset->printString();

  // Convert segment index → vertex index (same rule as for the base curve).
  int ls_i_offset = ls_i_prev + ((fabs(1.0 - u1) <= TOLERANCE || e == 1) ? 1 : 0);
  PLOG(debug) << "  ls_i = " + std::to_string(ls_i_offset);

  // Trim offset curve and record the new offset endpoint.
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

  // Phase 5: update the base-offset index map to reflect the trimmed curves.
  PLOG(debug) << "adjust base-offset linking indices";
  int nv_base = static_cast<int>(s->curveBase()->vertices().size());
  int nv_offset = static_cast<int>(s->curveOffset()->vertices().size());
  PLOG(debug) << "  nv_base = " + std::to_string(nv_base);
  PLOG(debug) << "  nv_offset = " + std::to_string(nv_offset);
  PLOG(debug) << "  ls_i_base = " + std::to_string(ls_i_base)
              + ", ls_i_offset = " + std::to_string(ls_i_offset);

  if (e == 0) {
    BaseOffsetMapEditor(s->baseOffsetIndicesPairs(), "joinSegments")
        .trimHead(ls_i_base, ls_i_offset, true);
  } else {
    BaseOffsetMapEditor(s->baseOffsetIndicesPairs(), "joinSegments")
        .trimTail(ls_i_base, ls_i_offset, true, TailTrimCaps());
  }

  s->printBaseOffsetPairs();
}




// Handles the joint between two consecutive segments s1 and s2 at their
// respective ends e1 / e2.  The joint vertex v is the shared base-curve vertex.
//
// Dispatches to joinStyle1 (step joint) or joinStyle2 (smooth joint) based on
// `style`, then:
//   - sets the offset endpoint on each segment,
//   - adds DCEL edges along the bound from v to the new offset vertices.
void PComponent::joinSegments(
  Segment *s1, Segment *s2, int e1, int e2,
  PDCELVertex *v, JointStyle style, const BuilderConfig &bcfg
  ) {
  MESSAGE_SCOPE(g_msg);

  PLOG(debug) <<
    "joining segments ends: "
    + s1->getName() + " " + std::to_string(e1) + ", "
    + s2->getName() + " " + std::to_string(e2)
  ;
  PLOG(debug) << "  joint style: "
              + std::string(style == JointStyle::step ? "step" : "smooth");
  PLOG(debug) << "  joint base vertex: " + v->printString();

  PDCELVertex *v1_new = nullptr, *v2_new = nullptr;
  // bound_vertices accumulates the vertices along the joint boundary: it
  // starts at the base vertex v and ends at the new offset vertices.
  std::vector<PDCELVertex *> bound_vertices;
  bound_vertices.push_back(v);

  if (style == JointStyle::step) {
    joinStyle1(s1, s2, e1, e2, v, v1_new, v2_new, bound_vertices);
  }
  else if (style == JointStyle::smooth) {
    if (!joinStyle2(s1, s2, e1, e2, v1_new, v2_new, bound_vertices)) {
      return;
    }
  }

  // Record the new offset endpoint on each segment.
  if (e1 == 0) s1->setHeadVertexOffset(v1_new);
  else if (e1 == 1) s1->setTailVertexOffset(v1_new);

  if (e2 == 0) s2->setHeadVertexOffset(v2_new);
  else if (e2 == 1) s2->setTailVertexOffset(v2_new);

  // Add DCEL edges along the bound (from base vertex to offset vertex/es).
  PLOG(debug) << "  adding " + std::to_string(bound_vertices.size() - 1)
              + " bound edge(s) to DCEL";
  for (int i = 0; i < static_cast<int>(bound_vertices.size()) - 1; ++i) {
    bcfg.dcel->addEdge(bound_vertices[i], bound_vertices[i + 1]);
  }

  PLOG(debug) << "  base -- offset index pairs of segment: " + s1->getName();
  for (auto i = 0; i < s1->baseOffsetIndicesPairs().size(); i++) {
    const BaseOffsetPair &pair = s1->baseOffsetIndicesPairs()[i];
    PLOG(debug) <<
      "  " + std::to_string(pair.base) + " : "
      + s1->curveBase()->vertices()[pair.base]->printString()
      + " -- " + std::to_string(pair.offset) + " : "
      + s1->curveOffset()->vertices()[pair.offset]->printString()
    ;
  }

  PLOG(debug) << "  base -- offset index pairs of segment: " + s2->getName();
  for (auto i = 0; i < s2->baseOffsetIndicesPairs().size(); i++) {
    const BaseOffsetPair &pair = s2->baseOffsetIndicesPairs()[i];
    PLOG(debug) <<
      "  " + std::to_string(pair.base) + " : "
      + s2->curveBase()->vertices()[pair.base]->printString()
      + " -- " + std::to_string(pair.offset) + " : "
      + s2->curveOffset()->vertices()[pair.offset]->printString()
    ;
  }

}




// Creates the open (free) end of a segment by connecting its base and offset
// endpoints with a DCEL edge.
//
// If the segment has a non-zero prevBound (e==0) or nextBound (e==1) vector,
// the offset curve is first trimmed against a bound line built from that
// vector (an auxiliary direction used to clip overhanging offset curves at
// open ends).  The base curve is not trimmed in this path.
//
// After any trimming, a single DCEL edge is added from the base endpoint to
// the offset endpoint to close the laminate cross-section at this end.
void PComponent::createSegmentFreeEnd(Segment *s, int e, const BuilderConfig &bcfg) {
  PLOG(debug) << "createSegmentFreeEnd: " + s->getName() + " end=" + std::to_string(e);

  // Trim head
  if (e == 0 && s->prevBound().normSq() != 0) {
    PLOG(debug) << "  trimming offset head against prevBound";

    SPoint3 sp0{s->curveBase()->vertices().front()->point()};
    SVector3 sv1{s->prevBound()};
    SPoint3 sp1{sp0 + sv1.point()};
    PDCELVertex *p = new PDCELVertex(sp1);

    // Build a 2-vertex bound from the base head vertex in the prevBound direction.
    std::vector<PDCELVertex *> b;
    b.assign({s->curveBase()->vertices().front(), p});

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
    PLOG(debug) << "  head trim intersection point: " + ip->printString();

    trimCurveAtVertex(s->curveOffset()->vertices(), ip, CurveEnd::Begin);

    // ls_i1 is the segment index; subtract 1 to get the vertex index of the
    // segment start, which becomes the new offset head index.
    int ls_i_offset = ls_i1 - 1;
    PLOG(debug) << "  ls_i_offset = " + std::to_string(ls_i_offset);
    BaseOffsetMapEditor(s->baseOffsetIndicesPairs(), "createSegmentFreeEnd head")
        .trimHead(0, ls_i_offset, false);

  }

  // Trim tail
  else if (e == 1 && s->nextBound().normSq() != 0) {
    PLOG(debug) << "  trimming offset tail against nextBound";

    SPoint3 sp0{s->curveBase()->vertices().back()->point()};
    SVector3 sv1{s->nextBound()};
    SPoint3 sp1{sp0 + sv1.point()};
    PDCELVertex *p = new PDCELVertex(sp1);

    // Build a 2-vertex bound from the base tail vertex in the nextBound direction.
    std::vector<PDCELVertex *> b;
    b.assign({s->curveBase()->vertices().back(), p});

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
    PLOG(debug) << "  tail trim intersection point: " + ip->printString();

    trimCurveAtVertex(s->curveOffset()->vertices(), ip, CurveEnd::End);

    PLOG(debug) << "curve base:";
    s->curveBase()->print();
    PLOG(debug) << "curve offset (after tail trim):";
    s->curveOffset()->print();

    int ls_i_base = static_cast<int>(s->curveBase()->vertices().size());
    int ls_i_offset = ls_i1;
    int _tmp_nv_offset = static_cast<int>(s->curveOffset()->vertices().size());

    PLOG(debug) << "  ls_i_base = " + std::to_string(ls_i_base)
                + ", ls_i_offset = " + std::to_string(ls_i_offset)
                + ", nv_offset = " + std::to_string(_tmp_nv_offset);

    s->printBaseOffsetPairs();
    BaseOffsetMapEditor(s->baseOffsetIndicesPairs(), "createSegmentFreeEnd tail")
        .trimTail(
            ls_i_base, ls_i_offset, false,
            TailTrimCaps(ls_i_base, _tmp_nv_offset));
    s->printBaseOffsetPairs();

  }

  PLOG(debug) << "curve base:";
  s->curveBase()->print();
  PLOG(debug) << "curve offset:";
  s->curveOffset()->print();
  s->printBaseOffsetPairs();

  // Add the closing DCEL edge between the base endpoint and the offset endpoint.
  if (e == 0) {
    PLOG(debug) << "  adding head edge: base.front() -- offset.front()";
    bcfg.dcel->addEdge(s->curveBase()->vertices().front(),
                             s->curveOffset()->vertices().front());
    s->setHeadVertexOffset(s->curveOffset()->vertices().front());
  }
  else if (e == 1) {
    PLOG(debug) << "  adding tail edge: base.back() -- offset.back()";
    bcfg.dcel->addEdge(s->curveBase()->vertices().back(),
                             s->curveOffset()->vertices().back());
    s->setTailVertexOffset(s->curveOffset()->vertices().back());
  }
}
