# Bug Fixes: `src/geo/intersect.cpp`

Date: 2026-02-16
Branch: task/refactor

---

## Fix 1 — Uninitialized `d1`/`d2` in plane-projection overloads

**Files:** `src/geo/intersect.cpp` (lines ~131 and ~193)

**Problem:** When `plane` ∉ {0, 1, 2}, `d1` and `d2` were left uninitialised.
The subsequent array access `l1p1[d1]` etc. was undefined behaviour.

**Fix:** Added `else` branch to both `if/else if` chains:
```cpp
} else {
  PLOG(error) << "calcLineIntersection2D: invalid plane index " << plane;
  return false;
}
```

Applied to both the `PGeoPoint3` overload and the `SPoint3` overload.

**Also:** Changed `int &plane` to `const int &plane` in the `SPoint3` overload
signature — the parameter was never modified.

---

## Fix 2 — `intersect()`: output pointer passed by value (critical)

**Files:** `src/geo/intersect.cpp` (line ~297), `include/geo.hpp` (line ~251)

**Problem:** `PDCELVertex *intersect` was a pointer passed by value. The
assignment `intersect = subject->getParametricVertex(us)` only updated the
local copy; the caller's pointer was never written. The allocated vertex was
also immediately leaked.

**Fix:** Changed parameter to a reference-to-pointer in both the definition and
the declaration:
```cpp
// before
int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
              PDCELVertex *intersect);
// after
int intersect(PGeoLineSegment *subject, PGeoLineSegment *tool,
              PDCELVertex *&intersect);
```

---

## Fix 3 — `lsi` leaked on every iteration in Baseline `findCurvesIntersection`

**File:** `src/geo/intersect.cpp` (lines ~619–669)

**Problem:** `lsi = new PGeoLineSegment(v1, v2)` was allocated each iteration
but never deleted before any of the `continue` or `break` exits.

**Fix:** Added `delete lsi` before every loop exit:
- Before `continue` when `u1 > 1`
- Before `break` when `fabs(u1) < TOLERANCE` (intersection at segment start)
- Before `break` when `fabs(u1-1) < TOLERANCE` (intersection at segment end)
- After `lsi->getParametricVertex(u1)` and before `break` when `u1 < 0`
- After `lsi->getParametricVertex(u1)` and before `break` in the inner
  (0 < u1 < 1) case
- Before `continue` in the parallel (`else`) branch

---

## Fix 4 — `bl_new` leaked on null return

**File:** `src/geo/intersect.cpp` (line ~671)

**Problem:** `Baseline *bl_new = new Baseline()` was allocated before the
`vlist.size() < 2` guard. On the null-return path the object was never freed.

**Fix:**
```cpp
if (vlist.size() < 2) {
  delete bl_new;   // added
  return nullptr;
}
```

---

## Fix 5 — `iold`/`inew` index flip executed inside loop for `end==1`

**File:** `src/geo/intersect.cpp` (lines ~683–691)

**Problem:** Inside the second `while (!vlist.empty())` loop the lines
```cpp
iold = n - 1 - iold;
inew = bl_new->vertices().size() - 1 - inew;
```
ran on every iteration. Because the flip is its own inverse, the final values
oscillated with loop parity (odd number of pops → flipped; even → not flipped).
Additionally, `bl_new->vertices().size()` changed each iteration, so `inew` was
computed against a moving target.

**Fix:** Moved both expressions to after the loop, wrapped in `if (end == 1)`:
```cpp
  }  // end while

  if (end == 1) {
    iold = static_cast<int>(n) - 1 - iold;
    inew = static_cast<int>(bl_new->vertices().size()) - 1 - inew;
  }
```

---

## Fix 6 — `getIntersectionLocation` ignores `inner_only` for the seed element

**File:** `src/geo/intersect.cpp` (lines ~850–852)

**Problem:** The function always initialised `ls_i`, `u`, `j` from element 0
regardless of the `inner_only` flag, then applied the filter only from element 1
onward. If `uu[0]` was outside [0, 1] and `inner_only == 1`, the returned
values were incorrect.

**Fix:** Replaced the unconditional seed initialisation with a sentinel
(`j = -1`) and changed the loop to start from `k = 0`. The first element that
satisfies the `inner_only` condition is accepted as the new seed via a `j == -1`
guard, after which the normal `which_end` comparison logic applies.

```cpp
ls_i = -1;
double u = 0.0;
j = -1;

for (auto k = 0; k < (int)ii.size(); k++) {
  if ((inner_only && uu[k] >= 0 && uu[k] <= 1) || !inner_only) {
    if (j == -1) {
      ls_i = ii[k]; u = uu[k]; j = k;
    } else if (which_end == 0) { ... }
    else if (which_end == 1) { ... }
  }
}
```
