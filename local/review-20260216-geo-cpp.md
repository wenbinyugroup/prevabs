# Code Review: `src/geo/geo.cpp`

Date: 2026-02-16

---

## Overview

`geo.cpp` is a collection of standalone geometry utility functions — polyline arithmetic, curve joining, vector/angle helpers, vertex list merging, and trimming. It is consumed by the DCEL pipeline and the cross-section builder. The file is ~830 lines, well-commented, and mostly readable. The issues below range from outright bugs to API design and style concerns.

---

## Issues by Function

### `isClose` (line 27)

No issues. This is the only function in the file that properly handles both absolute and relative tolerance. The same pattern should be applied wherever `TOLERANCE` is currently used raw.

---

### `calcPolylineLength` (line 52)

**[Minor] Parameter passed by value — unnecessary copy**

```cpp
double calcPolylineLength(const std::vector<PDCELVertex *> ps)
```

`ps` is a pointer vector but the vector itself is copied on every call. Change to `const std::vector<PDCELVertex*> &ps`.

**[Minor] Signed/unsigned comparison**

```cpp
for (auto i = 1; i < ps.size(); ++i)
```

`i` is deduced as `int`; `ps.size()` returns `std::size_t`. Use `std::size_t i = 1`.

---

### `findPointOnPolylineByCoordinate` (line 77) — primary overload

**[Bug] Uninitialized variables when `by` is invalid**

```cpp
double left_x2, left_x3, right_x2, right_x3;
...
if (by == "x2") { ... }
else if (by == "x3") { ... }
else { std::cout << markError << ...; }
// falls through — variables still uninitialized, used below
```

If `by` is anything other than `"x2"` or `"x3"`, the error is printed but execution continues into the comparison block using uninitialized values. This is undefined behaviour. Should `return nullptr` (or throw) in the `else` branch.

**[Minor] `by` as a stringly-typed API**

Two magic strings `"x2"` / `"x3"` appear across multiple call sites. An `enum class SearchAxis { X2, X3 }` would prevent silent bugs from typos.

**[Minor] Unsigned underflow if `ps` is empty**

```cpp
for (auto i = 0; i < ps.size() - 1; i++)
```

If `ps` is empty, `ps.size() - 1` wraps to `SIZE_MAX` (unsigned arithmetic). Add a guard: `if (ps.size() < 2) return pv;`.

**[Minor] Memory leak on not-found path**

`new PDCELVertex(label)` is allocated unconditionally. If the loop exhausts without a `break`, the function returns a newly-allocated vertex whose position is never set, and `param` is set to `0/length` = 0. No caller can distinguish "not found" from "found at param=0".

**[Design] `count` parameter is never documented to be 1-based**

The semantics of `count` (find the `count`-th occurrence) are only visible by reading the body. A `count=0` call will never match, silently. Document the 1-based convention or add an assertion.

---

### `findParamPointOnPolyline` (line 207)

**[Minor] Parameter passed by value**

```cpp
PDCELVertex *findParamPointOnPolyline(const std::vector<PDCELVertex *> ps, ...)
```

Same vector copy issue as `calcPolylineLength`. Should be `const std::vector<PDCELVertex*> &ps`.

**[Bug] `li` may be uninitialized if `ps` has fewer than 2 vertices**

```cpp
double ui = 0, li;
for (seg = 0; seg < nlseg; ++seg) { li = ...; ... }
ui = ulength / li;  // li uninitialized if nlseg == 0
```

If `ps.size() <= 1`, the loop body never runs and `li` is uninitialized. Add a size guard.

**[Bug] `ui == 0.5` edge case falls through to `new PDCELVertex`**

```cpp
if (!is_new) {
    if (ui < 0.5)  return ps[seg];
    else if (ui > 0.5) return ps[seg+1];
}
PDCELVertex *newv = new PDCELVertex(newp);  // reached when !is_new && ui == 0.5
```

When `is_new` is false but `ui` is exactly 0.5, neither branch returns, and the function allocates a new vertex for what is geometrically an existing midpoint. This is likely a latent issue because floating-point 0.5 is exact, but it indicates the logic is incomplete. Should be `ui <= 0.5` / `ui > 0.5` or an explicit midpoint return.

**[Minor] No clamping when `u > 1`**

If the caller passes `u > 1`, `ulength` exceeds total length, the loop exits early with excess distance, and `ui > 1`. The resulting point lies outside the polyline. No assertion or clamp is present.

---

### `getTurningSide` (line 251)

No significant issues. Uses `TOLERANCE = 1e-15` (flagged in the DCEL review), but the effect here is minor — it only affects the collinear classification threshold.

---

### `joinCurves` — both overloads (lines 301, 361)

**[Bug] Infinite loop when curves are disconnected**

```cpp
for (blit = curves.begin(); blit != curves.end(); ++blit) {
    if (...match...) { ...; break; }
}
curves.remove(*blit);
```

If no curve in the remaining list shares an endpoint with `line`, the inner loop runs to `end()` and `*blit` is undefined behaviour. `curves.remove(*blit)` with a dereferenced end iterator is UB. The outer `while (!curves.empty())` then loops forever or corrupts memory.

Add a guard: if `blit == curves.end()` after the inner loop, break with an error.

**[Minor] `curves` passed by value**

Both overloads take `std::list<Baseline*>` by value. This is intentional (they pop from the list), but the API makes it non-obvious that the caller's list is not modified. A comment or `std::move` from the call site would clarify intent.

---

### `adjustCurveEnd` (line 418)

**[Bug] `ls_end` is never deleted — memory leak**

```cpp
PGeoLineSegment *ls_end;
if (end == 0) { ls_end = new PGeoLineSegment(...); }
else if (end == 1) { ls_end = new PGeoLineSegment(...); }
// ...
// ls_end is never deleted
```

**[Bug] Uninitialized `ls_end` if `end` is not 0 or 1**

If `end` has any other value, `ls_end` is uninitialized and the subsequent `calcLineIntersection2D(ls_end, ...)` is UB.

**[Design]** The `end` parameter is a magic `int` (0 = begin, 1 = end). Use `enum class End { Begin, End }` for clarity and to make the uninitialized case impossible.

---

### `getVectorFromAngle` (line 448)

**[API] Mutates the caller's `angle` via non-const reference without clear documentation**

```cpp
SVector3 getVectorFromAngle(double &angle, const int &plane)
```

The function normalizes `angle` into (-90, 270] and writes back through the reference. Whether this mutation is intentional at call sites is unclear. If the normalization is a detail, take by value; if the normalized value is needed by the caller, document this explicitly.

**[Minor] Float equality comparison**

```cpp
if (angle == 90.0 || angle == 270.0)
```

`angle` is the result of `+= 360` / `-= 360` operations on an arbitrary input. The equality check is unsafe in general; use `std::abs(angle - 90.0) < 1e-9` etc.

---

### `calcAngleBisectVector` — point overload (line 592)

**[Bug] Wrong normalization: divides by `normSq` instead of `norm`**

```cpp
v1 *= 1 / v1.normSq();
v2 *= 1 / v2.normSq();
```

To normalize two vectors to equal length (for an unweighted bisector), divide by `norm()`, not `normSq()`. Dividing by `normSq()` effectively scales each vector by `1/|v|²`, so shorter vectors receive more weight. The resulting bisector is biased toward the shorter edge.

Fix: `v1.normalize(); v2.normalize();` (or `v1 *= 1 / v1.norm()`).

---

### `calcAngleBisectVector` — vector overload (line 613)

**[Minor] Parameters should be `const`**

```cpp
SVector3 calcAngleBisectVector(SVector3 &v1, SVector3 &v2, std::string s1, std::string s2)
```

`v1`, `v2` are not modified; should be `const SVector3 &`. `s1`, `s2` should be `const std::string &` (or `std::string_view`).

---

### `calcBoundVertices` (line 639)

**[Bug] Division by zero when `sv_bound` is perpendicular to `p`**

```cpp
thkp = thk / dot(sv_bound, p);
```

If the bound direction is exactly perpendicular to the local normal, `dot == 0` and `thkp` becomes ±∞. No guard is present.

**[Minor] Signed/unsigned loop**

```cpp
for (int i = 0; i < layup->getLayers().size(); ++i)
```

`getLayers().size()` returns `std::size_t`. Use `std::size_t i` or a range-for.

---

### `combineVertexLists` (line 678)

**[Bug] `abs()` used on `double` instead of `std::abs()`**

```cpp
if (abs(vl_1.front()->y() - vl_1.back()->y()) >=
    abs(vl_1.front()->z() - vl_1.back()->z()))
```

`abs()` without namespace qualification resolves to the C integer `abs` if `<cstdlib>` is included (and it is, at line 20). For doubles, this truncates to integer before taking absolute value, which gives wrong results for values in (-1, 0). Use `std::abs()` throughout.

**[Minor] Complexity** — the merge-sort-style combine is correct but hard to follow. A comment block explaining the invariants of `vi_1` / `vi_2` (they are indices into `vl_c`) would help reviewers.

---

### `trim` (line 787)

**[Minor] Silent no-op if vertex not found**

If `v` is not in `c`, after the loop `s` remains 0, so `tmp_c1 == c` and `tmp_c2` is empty. With `remove == 0`, `c` is replaced with an empty vector. No warning is emitted. At minimum, assert or log when `v` is not found.

**[Minor] Signed/unsigned comparison**

```cpp
for (auto i = 0; i < c.size(); i++)
```

**[Style]** The inner push/copy logic can be replaced with `std::find` + `std::copy`:

```cpp
auto it = std::find(c.begin(), c.end(), v);
if (it == c.end()) { /* warn */ return -1; }
if (remove == 0) c = std::vector<PDCELVertex*>(it, c.end());
else             c = std::vector<PDCELVertex*>(c.begin(), it + 1);
```

---

## Summary Table

| Severity | Count | Examples |
|---|---|---|
| **Bug** | 7 | UB from uninitialized vars (`findPointOnPolylineByCoordinate`, `adjustCurveEnd`, `findParamPointOnPolyline`); wrong normalization (`calcAngleBisectVector`); infinite loop (`joinCurves`); division by zero (`calcBoundVertices`); `abs()` vs `std::abs()` (`combineVertexLists`) |
| **Minor / Design** | 10 | Value copies of vectors, stringly-typed `by` / `end` params, float equality, unsigned underflow, silent no-ops |

### Priority fixes — applied 2026-02-16

| # | Bug | Fix applied |
| --- | --- | --- |
| 1 | `calcAngleBisectVector`: divides by `normSq` instead of `norm` — bisector is geometrically wrong | Replaced `v1 *= 1/v1.normSq()` with `v1.normalize()` |
| 2 | `findPointOnPolylineByCoordinate`: falls through with uninitialized `left_x2`/`left_x3`/`right_x2`/`right_x3` when `by` is invalid | Added `delete pv; return nullptr;` in the `else` branch |
| 3 | `joinCurves` (both overloads): UB dereference of `end()` + infinite loop when curves are disconnected | Added `if (blit == curves.end()) { log error; break; }` after inner search loop |
| 4 | `adjustCurveEnd`: `ls_end` never deleted (leak); uninitialized when `end` ∉ {0,1} | Added `else { log error; return; }` and `delete ls_end` before function exit |
| 5 | `combineVertexLists`: bare `abs()` on `double` truncates to `int` via C `abs()` | Changed both call sites to `std::abs()` |
| 6 | `calcBoundVertices`: `thk / dot(sv_bound, p)` — division by zero when perpendicular | Added `std::abs(dp) < TOLERANCE` guard; skips the layer with an error log |
| 7 | `findParamPointOnPolyline`: `li` uninitialized when `ps.size() < 2` | Added size guard at function entry; returns `nullptr` with `is_new = false` |
