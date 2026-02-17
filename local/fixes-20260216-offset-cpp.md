# Bug Fixes: `src/geo/offset.cpp`

Date: 2026-02-16
Source review: `local/review-20260216-offset-cpp.md`

---

## Bug 1 — Memory leak: intermediate `PGeoLineSegment` in `offsetCurve`

**Location:** `offsetCurve` lines 152–153 (size==2 branch) and 160–161 (loop body).

**Root cause:** The pattern `ls = new PGeoLineSegment(...); ls = offsetLineSegment(ls, ...)` overwrites `ls` before the original allocation is freed. `offsetLineSegment(PGeoLineSegment*, SVector3&)` always allocates a new segment and does not free its input.

**Fix:** Introduce a named temporary `ls_raw`, delete it after `offsetLineSegment` returns:

```cpp
// Before:
ls = new PGeoLineSegment(curve->vertices()[i], curve->vertices()[i + 1]);
ls = offsetLineSegment(ls, side, distance);

// After:
PGeoLineSegment *ls_raw = new PGeoLineSegment(curve->vertices()[i], curve->vertices()[i + 1]);
ls = offsetLineSegment(ls_raw, side, distance);
delete ls_raw;
```

Also initialised `ls_prev = nullptr` (was uninitialised) while touching this section.

---

## Bug 2 — Memory leak: intermediate offset vertices in the main `offset()`

**Location:** Step 1 loop in `offset(const std::vector<PDCELVertex*>&, ...)`.

**Root cause:** Every loop iteration allocates `v1_tmp` and `v2_tmp`. Only a subset of these end up in `vertices_tmp`; the rest (used only for intersection point computation) were silently dropped.

**Fix:** Track every allocation in `allocated_tmp`, then delete anything not retained in `vertices_tmp` after the loop:

```cpp
std::vector<PDCELVertex *> allocated_tmp;
// ... in loop:
v1_tmp = new PDCELVertex(); v2_tmp = new PDCELVertex();
allocated_tmp.push_back(v1_tmp); allocated_tmp.push_back(v2_tmp);
// ... after loop:
for (auto vv : allocated_tmp) {
  if (std::find(vertices_tmp.begin(), vertices_tmp.end(), vv) == vertices_tmp.end())
    delete vv;
}
```

Added `#include <algorithm>` for `std::find`.

---

## Bug 3 — Out-of-bounds access: `i2s[j1]` when no intersections found

**Locations:**
- Step 3 consecutive-pair loop (around line 601).
- Closed-curve head-tail handling (around line 705).

**Root cause:** `j1` is written by `getIntersectionLocation`. If `i1s` is empty (no intersections), `j1` is undefined and `i2s[j1]` is out-of-bounds UB.

**Fix:** Guard both sites with an `i1s.empty()` check. If empty, log a warning and skip the trim. Also add a `j1 < 0 || j1 >= i2s.size()` range check after `getIntersectionLocation` returns.

```cpp
if (i1s.empty()) {
  PLOG(warning) << pmessage->message("no intersection found ...; skipping trim");
} else {
  ls_u1 = getIntersectionLocation(..., j1, pmessage);
  if (j1 < 0 || j1 >= static_cast<int>(i2s.size())) {
    PLOG(warning) << pmessage->message("intersection index j1 out of range; skipping trim");
  } else {
    ls_i2 = i2s[j1]; ls_u2 = u2s[j1];
    // ... trim / link-adjust ...
  }
}
```

---

## Bug 4 — Fragile closed-curve vertex extraction (single sub-line case)

**Location:** `else` branch of `if (lines_group.size() > 1)` inside the closed-curve handling section.

**Root cause:** The `keep`/`check` loop tried to extract the sub-sequence between the first and second occurrences of `v0`. However, `check` is reset to `true` at the start of each iteration, and the first `if` sets it to `false` — so the second `if (check && keep && v == v0)` never fires in the same iteration. If `v0` appears only once, `keep` is never reset and the entire tail is included.

**Fix:** Replaced with an explicit `std::find`-based range slice:

```cpp
auto it_begin = std::find(lg0.begin(), lg0.end(), v0);
if (it_begin != lg0.end()) {
  auto it_end = std::find(std::next(it_begin), lg0.end(), v0);
  if (it_end != lg0.end()) {
    lg0 = std::vector<PDCELVertex *>(it_begin, std::next(it_end));
  } else {
    // v0 appears only once: keep from v0 to the end
    lg0 = std::vector<PDCELVertex *>(it_begin, lg0.end());
  }
} else {
  PLOG(warning) << pmessage->message("closed curve: intersection vertex not found in single sub-line");
}
```

---

## Bug 5 — `link_to_2` sentinel collision with valid index 0

**Location:** Step 4 in the main `offset()` function.

**Root cause:** `link_to_2` was initialised to all-zeros. The forward-fill condition `link_to_2[i] == 0` was intended to detect "unmapped" entries, but `0` is also the index of the first offset vertex — any base vertex that legitimately maps to offset vertex 0 would be incorrectly forward-filled.

**Fix:** Changed sentinel to `-1` (not a valid vertex index):

```cpp
// Before:
link_to_2 = std::vector<int>(base.size(), 0);
...
if (i > 0 && link_to_2[i] == 0) { link_to_2[i] = link_to_2[i-1]; }

// After:
link_to_2 = std::vector<int>(base.size(), -1);
...
if (link_to_2[i] == -1) {
  link_to_2[i] = (i > 0 && link_to_2[i-1] >= 0) ? link_to_2[i-1] : 0;
}
```

The `i > 0` guard was also removed — the new sentinel correctly handles index 0 without a special case.

---

## Bug 6 — `dr` silently normalised through a non-const reference

**Location:** `offsetLineSegment(SPoint3, SPoint3, SVector3&, double&, SPoint3, SPoint3)` line 50.

**Root cause:** `dr.normalize()` mutates the caller's vector through the `SVector3 &dr` parameter.

**Fix:** Normalise a local copy; the parameter signature is preserved for backward compatibility:

```cpp
SVector3 dr_norm = dr;  // do not mutate the caller's vector
dr_norm.normalize();
q1 = (SVector3(p1) + dr_norm * ds).point();
q2 = (SVector3(p2) + dr_norm * ds).point();
```

---

## Bug 7 — No null-pointer guard on `offset()` output parameters

**Location:** `offset(PDCELVertex*, PDCELVertex*, int, double, PDCELVertex*, PDCELVertex*)`.

**Root cause:** `v1_off->setPoint(p1)` is UB if `v1_off` is null. All current callers allocate before calling, so this is not triggered today, but the absence of any guard makes the function fragile.

**Fix:** Added null check at function entry:

```cpp
if (!v1_off || !v2_off) {
  PLOG(error) << "offset: null output vertex pointer";
  return 0;
}
```
