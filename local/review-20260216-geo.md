# DCEL Review: Goals, Issues, and Library Alternatives

Date: 2026-02-16

---

## What the DCEL Does

The DCEL represents the **2D planar geometry of a beam cross-section** — a layered material map. The full pipeline is:

1. **Baseline curves** (polylines) are inserted as edges into the DCEL
2. **Faces** are created/split for each material region (filling, laminate layers)
3. **Sweep-line queries** determine face containment when adding new regions
4. **Face metadata** (material, angles, mesh size) is assigned
5. The final DCEL is exported to **Gmsh** for meshing

The DCEL is not a general-purpose library — it is tightly coupled to the cross-section build pipeline. Its topology operations form the backbone of the entire `prevabs` tool.

---

## Key Files

| File | Role |
| --- | --- |
| `include/PDCEL.hpp` / `src/geo/PDCEL.cpp` | DCEL container: all topology algorithms |
| `include/PDCELVertex.hpp` / `src/geo/PDCELVertex.cpp` | Vertex: operators, degree, transforms |
| `include/PDCELHalfEdge.hpp` / `src/geo/PDCELHalfEdge.cpp` | Half-edge: angle, toVector, loop assignment |
| `include/PDCELFace.hpp` / `src/geo/PDCELFace.cpp` | Face: boundary traversal, material metadata |
| `include/PDCELHalfEdgeLoop.hpp` / `src/geo/PDCELHalfEdgeLoop.cpp` | Loop: direction computation, bottom-left tracking |
| `include/PBST.hpp` / `src/geo/PBST.cpp` | Custom AVL tree for sweep-line vertex ordering |
| `src/geo/intersect.cpp` | `calcLineIntersection2D`, `findAllIntersections`, `findCurvesIntersection` |
| `src/geo/offset.cpp` | Polyline offset with degenerate handling, partial homog2d integration |
| `src/geo/geo.cpp` | Geometry utilities: polyline ops, curve joining, trim |
| `include/globalConstants.hpp` | `TOLERANCE = 1e-15`, `INF`, `PI`, `ABS_TOL`, `REL_TOL` |

---

## Robustness Issues (Ordered by Severity)

### 1. Degenerate tolerance (`TOLERANCE = 1e-15`)

**File:** `include/globalConstants.hpp`

The snap tolerance is at machine epsilon. Any floating-point arithmetic will produce errors of this magnitude routinely. Near-parallel line detection in `calcLineIntersection2D` uses `|dnm| <= 1e-15` — this will pass for lines that are geometrically nearly parallel, producing wildly incorrect intersection points.

This is the **most critical** robustness issue. It affects every intersection computation in `intersect.cpp` and `PDCEL.cpp`.

### 2. AVL tree `remove` bug

**File:** `src/geo/PBST.cpp`

In the "two children" case, the in-order successor loop always re-reads from `node_to_remove->left()` instead of following `successor_inorder->left()`. It finds the wrong node, potentially corrupting the sweep-line vertex ordering and causing wrong face containment results.

### 3. No coincident vertex detection on insertion

**File:** `src/geo/PDCEL.cpp` — `addEdge`

`addEdge` checks `v->dcel() == nullptr` but does not check for an existing vertex at the same coordinates. Two vertices at identical (or near-identical) coordinates can coexist, breaking face traversal, degree counting, and angular sorting.

### 4. Exact-equality vertex comparison

**Files:** `include/PDCELVertex.hpp`, `src/geo/PBST.cpp`

`operator==` on vertices uses raw `double ==` with no tolerance. The AVL tree's equal-check also uses exact bit-equality. Vertices computed via different arithmetic paths that are geometrically coincident will not be recognized as the same.

### 5. Memory leaks in `splitEdge`

**File:** `src/geo/PDCEL.cpp`

Old half-edges are removed from `_halfedges` but not `delete`d (the `delete` calls are commented out). `findLineSegmentBelowVertex` also allocates a temporary `PDCELVertex` and `PGeoLineSegment` that are never freed.

### 6. `findCurvesIntersection` handles only 2 intersections

**File:** `src/geo/PDCEL.cpp`

The PDCEL-level curve-intersection routine only populates `vlist1/vlist2` for the first two intersection points. Concave faces or re-entrant boundaries will silently produce wrong results.

### 7. `splitFace` has no boundary membership validation

**File:** `src/geo/PDCEL.cpp`

`splitFace(f, v1, v2)` does not verify that `v1` and `v2` actually lie on the boundary of face `f`. When geometric computations produce slightly off-boundary points, the resulting topology can be invalid.

### 8. Wrap-around edge case in `updateEdgeNeighbors`

**File:** `src/geo/PDCEL.cpp`

The angular sort that maintains radial edge order around a vertex splits the edge list at the ±π boundary. If two edges have angles exactly at ±π (collinear, opposite directions), the split-point logic can misbehave and produce incorrect `prev/next` linkage.

---

## Candidate for Replacement: What to Target

The DCEL itself is hard to replace wholesale (it is too domain-coupled), but three distinct concerns can be addressed independently:

| Concern | Current code | Issue |
| --- | --- | --- |
| 2D arrangement / Boolean ops | Hand-rolled DCEL ops | All the issues above |
| Segment intersection | `calcLineIntersection2D` + partial `homog2d` | Tolerance, no robustness guarantees |
| Spatial index / sweep-line | Custom AVL tree | Known bug, reimplements `std::set` |

---

## Suggested Third-Party Libraries

### Option A: CGAL 2D Arrangements (best robustness, heaviest)

- Exact arithmetic via CGAL's `EPECK` kernel eliminates all floating-point topology failures
- `CGAL::Arrangement_2` with `Segment_traits_2` directly models the planar subdivision the DCEL builds
- Boolean set operations (`CGAL::Boolean_set_operations_2`) replace `splitFace` + intersection logic
- **Weight:** Large (~200 MB headers + link dependencies); requires several CGAL components
- **Verdict:** Best correctness. Overkill in terms of build complexity unless robustness is the top priority.

### Option B: Boost.Polygon (lighter, integer-only)

- `boost::polygon::polygon_set_data` supports Boolean operations on polygonal regions
- Header-only within Boost.Geometry
- Limitation: integer coordinates only — requires coordinate scaling (e.g., ×1e6 and round), which is standard practice for this domain
- **Weight:** Header-only; zero link dependencies
- **Verdict:** Good fit if geometry is simple (straight segments only, no arcs) — matches current use case.

### Option C: Clipper2 (recommended — lightweight, robust)

- C++17, single `Clipper2Lib` CMake target; ~5 source files, trivially vendored or via `FetchContent`
- Implements the Vatti clipping algorithm with exact integer arithmetic internally (scaled from doubles transparently)
- Supports: polygon clipping (union, intersection, difference, XOR), polygon offsetting, `PolyTree` output (nested outer/inner loop hierarchy — maps directly to DCEL face structure)
- Actively maintained; used in Inkscape, OpenSCAD, and others
- **Verdict:** Strong fit. Can replace `offset.cpp` and Boolean face operations. Does not replace the full DCEL topology but eliminates the highest-risk custom code.

### Option D: homog2d (already vendored — expand usage)

- Already used in `offset.cpp` for segment intersection (`h2d::Segment::intersects`)
- Provides more robust intersection than `calcLineIntersection2D` with proper degenerate handling
- **Quick win:** Replace all `calcLineIntersection2D` call sites with `h2d::Segment` intersection — zero new dependency
- **Verdict:** Immediate low-risk improvement without any architectural change.

### For the AVL tree: replace with `std::set`

The `PAVLTreeVertex` is used only as an ordered container for the sweep-line. A `std::set<PDCELVertex*, CompareByPosition>` with a proper tolerance-aware comparator replaces it entirely, eliminates the known `remove` bug, and uses the battle-tested standard library implementation.

---

## Recommended Action Plan

### Short-term (low risk, high impact)

1. Fix the AVL tree `remove` bug — or replace `PAVLTreeVertex` with `std::set`
2. Replace all `calcLineIntersection2D` calls with `h2d::Segment` intersection (homog2d already vendored)
3. Raise `TOLERANCE` from `1e-15` to `1e-9` to match `geo_tol`; unify all tolerance constants into one configurable value
4. Add coincident vertex lookup in `addEdge` — query the AVL/set for a vertex within tolerance before inserting a new one

### Medium-term (moderate effort, architectural improvement)

1. Vendor Clipper2 and replace `offset.cpp` with `Clipper2Lib::InflatePaths` — the offset code is the most complex and fragile custom code in the pipeline
2. Use Clipper2's `PolyTree` output to rebuild the face-loop hierarchy, replacing the sweep-line containment query (`findLineSegmentBelowVertex`) and `linkHalfEdgeLoops`

### Long-term (if correctness is critical)

1. Replace the intersection + arrangement core with CGAL `Arrangement_2` behind an adapter layer, keeping the existing face metadata API (`setMaterial`, `setTheta1`, `setLayerType`, etc.) intact
