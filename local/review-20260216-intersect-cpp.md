# Code Review: `src/geo/intersect.cpp`

Date: 2026-02-16

---

## Overview

`intersect.cpp` provides 2D line/segment intersection utilities and higher-level
curve–DCEL intersection helpers. It sits between the low-level `homog2d` wrapper
(recently migrated from the raw cross-product formula) and the DCEL / Baseline
pipeline. The file is ~1060 lines. Issues range from a critical silent bug in
`intersect()` to memory leaks, uninitialized variables, and API inconsistencies.

---

## Issues by Function

### `calcLineIntersection2D` — scalar overload (line 33)

**[Minor] `tol` parameter is silently ignored**

```cpp
bool calcLineIntersection2D(..., const double &tol)
// tol is kept for API compatibility ...
```

The comment at line 38 explains the intent, but callers that pass a meaningful
`tol` receive no error or warning. Every call site in this file passes
`TOLERANCE`, so the mismatch is harmless today, but it makes the API
misleading. Either remove the parameter (breaking change) or add a `(void)tol;`
and an in-code doc note that the homog2d threshold governs parallelism.

**[Minor] Degenerate-segment guard uses exact float equality**

```cpp
if (dx1 == 0.0 && dy1 == 0.0) return false;
```

For computed coordinates this can miss near-zero segments. A tolerance-based
guard (`dx1*dx1 + dy1*dy1 < tol*tol`) would be consistent with the rest of the
codebase. At minimum, document why exact zero is sufficient here.

**[Minor] `u1`, `u2` are left indeterminate on `return false`**

On the parallel / degenerate paths, the output parameters are not zeroed.
Callers that do not check the return value will read garbage. Convention
(set to 0 or NaN on failure) should be documented or enforced.

---

### `calcLineIntersection2D` — `PGeoPoint3` overload (line 131)

**[Bug] `d1`, `d2` uninitialized when `plane` is invalid**

```cpp
int d1, d2;
if (plane == 0) { d1 = 1; d2 = 2; }
else if (plane == 1) { d1 = 2; d2 = 0; }
else if (plane == 2) { d1 = 0; d2 = 1; }
// No else — d1/d2 uninitialized; array access below is UB
return calcLineIntersection2D(l1p1[d1], ...);
```

Add `else { /* log error */ return false; }`.  The identical pattern appears in
the `SPoint3` overload (line 193).

---

### `calcLineIntersection2D` — `SPoint3` overload (line 193)

**[Bug] Same uninitialized `d1`, `d2`** — see above.

**[Minor] `plane` taken by non-const reference but never modified**

```cpp
bool calcLineIntersection2D(..., int &plane, ...)
```

Should be `const int &plane` (or just `int plane`). The non-const reference
misleads callers into thinking the plane index may be mutated.

---

### `intersect` (line 291)

**[Critical Bug] Output parameter `intersect` is a pass-by-value pointer — the
caller's pointer is never updated**

```cpp
int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
              PDCELVertex *intersect) {
  ...
  intersect = subject->getParametricVertex(us);  // assigns the LOCAL copy
  result = 1;
```

`PDCELVertex *intersect` is a pointer passed **by value**. The assignment on
line 300 updates the local copy only; the caller's pointer remains unchanged.
Every call site that relies on the filled vertex reads an unmodified pointer.

Fix: change the signature to `PDCELVertex *&intersect` (reference to pointer).

**[Minor] Memory leak from the above bug**

`getParametricVertex` typically heap-allocates a new vertex. Because the
assignment does not escape the function, that vertex is immediately leaked.

**[Design] Magic return codes with no enum**

`-1`, `0`, `1`, `2` encode intersection state. An `enum class IntersectResult`
would make call sites self-documenting and prevent silent misuse.

---

### `findCurvesIntersection` — `PDCELHalfEdgeLoop` overload (line 339)

**[Minor] `vertices` copied by value**

```cpp
PDCELHalfEdge *findCurvesIntersection(
  std::vector<PDCELVertex *> vertices, ...)
```

This copies a potentially large vector on every call. Change to
`const std::vector<PDCELVertex *> &vertices`.

**[Minor] Signed/unsigned comparisons**

- Line 394: `for (auto k = 0; k < c_is.size(); k++)` — `k` is `int`.
- Line 474: `ls_i == vertices.size() - 2` — `ls_i` is `int`; if `vertices.size() < 2` the right side underflows to `SIZE_MAX`.

Use `std::size_t` loop indices or cast carefully.

**[Minor] `c_is.size() > 0` (line 402)**

Prefer `!c_is.empty()` — conventional and avoids the unsigned comparison.

**[Style] Large commented-out blocks (lines 456–497)**

The old single-expression form was refactored into the `update` flag. The
commented-out code is now dead and should be deleted.

---

### `findCurvesIntersection` — `Baseline` overload (line 560)

**[Bug] `lsi` is leaked on every loop iteration**

```cpp
lsi = new PGeoLineSegment(v1, v2);
...
// lsi is never deleted
```

This leaks one `PGeoLineSegment` per segment traversed. Add `delete lsi;`
before every `continue`, `break`, and at the end of the loop body.

**[Bug] `bl_new` leaked when `vlist.size() < 2`**

```cpp
Baseline *bl_new = new Baseline();
...
if (vlist.size() < 2) {
  return nullptr;   // bl_new is never freed
}
```

Add `delete bl_new; return nullptr;`.

**[Bug] `iold` and `inew` updated inside the loop for `end == 1`**

```cpp
} else if (end == 1) {
  bl_new->addPVertex(vlist.back());
  vlist.pop_back();
  link_to_list_new.push_back(link_to_list_copy.back());
  link_to_list_copy.pop_back();
  iold = n - 1 - iold;          // ← runs every iteration
  inew = bl_new->vertices().size() - 1 - inew;  // ← runs every iteration
}
```

Both index-flip expressions should execute **once** after the loop completes,
not on each pop. Running them repeatedly causes `iold` and `inew` to oscillate,
leaving them at the wrong value when the loop exits.

---

### `findAllIntersections` (line 714)

**[Minor] Signed/unsigned loop indices**

```cpp
for (int i = 0; i < c1.size() - 1; ++i)
for (int j = 0; j < c2.size() - 1; ++j)
```

If either container is empty, `size() - 1` underflows to `SIZE_MAX`. Guard with
`if (c1.size() < 2 || c2.size() < 2) return 0;`, or use `std::size_t` indices.

**[Minor] Return type should be `void`**

The function always returns `0` and has four output parameters. The `int`
return type adds noise; callers ignore it. Change to `void`.

---

### `getIntersectionLocation` (line 821)

**[Bug] First element always used regardless of `inner_only`**

```cpp
ls_i = ii[0];
double u = uu[0];    // unconditional — ignores inner_only
j = 0;

for (auto k = 1; k < ii.size(); k++) {
  if ((inner_only && uu[k] >= 0 && uu[k] <= 1) || !inner_only) { ... }
```

If `inner_only == 1` and `uu[0]` is outside [0,1], the returned `u` is
incorrect. The initialization should apply the same `inner_only` guard, or the
loop should start from index 0 with a "best so far" sentinel.

**[Minor] Signed/unsigned comparison** (line 836)

`for (auto k = 1; k < ii.size(); k++)` — same pattern as above.

**[Design] `inner_only` as `int`**

This is used as a boolean flag throughout. `bool inner_only` is clearer.

---

### `getIntersectionVertex` (line 915)

**[Minor] Wrong doc comment at line 984**

```cpp
// Intersecting point is the beginning point (v22)
```

Should read "ending point (v22)" — `v22` is the second endpoint of segment j.

---

## Summary Table

| Severity | Count | Examples |
|---|---|---|
| **Critical Bug** | 1 | `intersect()`: output pointer passed by value — caller never receives the vertex |
| **Bug** | 5 | Uninitialized `d1`/`d2` (plane overloads); `lsi` and `bl_new` leaks; wrong `iold`/`inew` loop update; `getIntersectionLocation` ignores `inner_only` on first element |
| **Minor / Design** | 11 | `tol` silently ignored; non-const `plane` ref; signed/unsigned comparisons; `size() > 0` vs `empty()`; `int` return on `findAllIntersections`; dead commented code; `inner_only` as `int` |

## Priority Fix List

| # | Issue | Fix |
|---|---|---|
| 1 | `intersect()`: pointer passed by value — output vertex never written to caller | Change `PDCELVertex *intersect` → `PDCELVertex *&intersect` |
| 2 | `calcLineIntersection2D` (PGeoPoint3/SPoint3): `d1`, `d2` uninitialized for invalid `plane` | Add `else { log; return false; }` |
| 3 | `findCurvesIntersection` (Baseline): `lsi` leaked each iteration | Add `delete lsi` before every loop exit |
| 4 | `findCurvesIntersection` (Baseline): `bl_new` leaked on early return | Add `delete bl_new` before `return nullptr` |
| 5 | `findCurvesIntersection` (Baseline): `iold`/`inew` flip runs every iteration for `end==1` | Move both assignments outside the `while` loop |
| 6 | `getIntersectionLocation`: ignores `inner_only` for the seed (index 0) element | Start loop from 0 with a valid sentinel; apply the guard uniformly |
