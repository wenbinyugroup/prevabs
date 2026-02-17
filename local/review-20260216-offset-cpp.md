# Code Review: `src/geo/offset.cpp`

Date: 2026-02-16

---

## Overview

`offset.cpp` provides three groups of functions: low-level segment offsetting helpers, a `Baseline`-level `offsetCurve`, and the main `offset` function that builds an offset vertex chain with intersection trimming and degenerate-segment elimination. The file is ~820 lines. Core algorithmic intent is sound; the main concerns are pervasive memory leaks (raw `new` without ownership), numerous dead variables, and a fragile closed-curve extraction loop.

---

## Issues by Function

### `offsetLineSegment(SPoint3, SPoint3, SVector3, double, SPoint3, SPoint3)` (line 48)

**[Minor] Non-const `dr` mutated in-place**

```cpp
void offsetLineSegment(SPoint3 &p1, SPoint3 &p2, SVector3 &dr, double &ds, ...)
```

`dr` is normalized inside the function (`dr.normalize()`), silently mutating the caller's vector. `ds` is never modified; it should be `const double`. Change `dr` to `const SVector3 &dr` and normalize a local copy, or document the mutation contract clearly.

---

### `offsetLineSegment(PGeoLineSegment*, int, double)` (line 77)

**[Style] Dead commented-out code (lines 81–85)**

The original string-based `if (side == "left")` block is still present as a comment. Delete it.

**[Minor] `side` convention implicit**

The magic int convention (positive = left, negative = right) is described only in the docstring. At line 79, `SVector3(side, 0, 0)` hardcodes Y=0, Z=0, silently assuming 2D geometry in the X-plane. Add a static assertion or a comment at the construction site.

---

### `offsetLineSegment(PGeoLineSegment*, SVector3&)` (line 114)

**[Minor] `offset` parameter should be `const SVector3&`**

The vector is only read; there is no reason to allow mutation.

**[Bug] Allocated input `ls` leaked at every call site**

This function's contract is `new PGeoLineSegment` → return. The pattern at every call site is:

```cpp
ls = new PGeoLineSegment(...);   // (A)
ls = offsetLineSegment(ls, ...); // (B) — (A) is now unreachable, leaked
```

The first allocation at `(A)` is overwritten by `(B)` without being deleted. This affects both `offsetCurve` (lines 152–153, 160–161) and any future callers. Either change `offsetLineSegment(PGeoLineSegment*, SVector3&)` to take the existing segment and mutate it in place, or have callers delete the original before reassigning.

---

### `offsetCurve(Baseline*, int, double)` (line 146)

**[Bug] Memory leak — `ls` always leaked (lines 152–153, 160–161)**

See above. Every loop iteration allocates a `PGeoLineSegment` that is immediately lost.

**[Bug] `ls_prev` uninitialized on first use**

```cpp
PGeoLineSegment *ls_prev, *ls_first_off;  // line 158 — uninitialized
for (int i = 0; i < ...; ++i) {
    ls = ...;
    if (i == 0) { ls_first_off = ls; ... }
    if (i > 0) { calcLineIntersection2D(ls, ls_prev, ...); }  // ls_prev OK here?
    ls_prev = ls;
}
```

`ls_prev` is only read when `i > 0`, and is assigned at the end of each iteration, so it works correctly today. However, the uninitialized declaration is fragile — a future refactor that changes the branch order could introduce UB. Initialise to `nullptr`.

**[Style] Large block of commented-out debug output (lines 200–204)**

Remove or convert to a `PLOG(debug)` block.

---

### `offset(PDCELVertex*, PDCELVertex*, int, double, PDCELVertex*, PDCELVertex*)` (line 230)

**[Minor] Return value is always 1 and ignored at all call sites**

Change to `void`.

**[Bug] No null-pointer guard on output parameters**

`v1_off->setPoint(p1)` at line 248 is UB if `v1_off` is null. Callers allocate before calling, so this is not currently triggered, but there is no assertion.

---

### `offset(const std::vector<PDCELVertex*>&, int, double, ...)` (line 281) — main function

This is the most complex function and has the most issues.

**[Bug] Numerous declared-but-unused variables — compiler warnings as silent correctness risk**

| Variable | Declared | Status |
|---|---|---|
| `v0_prev` | line 528 | Assigned `lines_group[0][0]`, never read again |
| `found` | line 529 | Set `false`, later assigned inside loop body but never checked |
| `i`, `j`, `i_prev` | line 532 | Set to 0, then shadowed by `auto i`/`auto j` in later loops — never used |
| `trim_index_begin_this` | line 533 | Assigned 0, never referenced |
| `trim_index_begin_next` | line 533 | Declared, never assigned or read |
| `tmp_trimmed_subline` | line 537 | Cleared each iteration, never populated with data that is consumed |
| `tmp_trimmed_link_to_base_index` | line 538 | Same as above |

These are remnants of an earlier design and add significant noise. Remove them all.

**[Bug] Potential out-of-bounds access at line 586**

```cpp
ls_u1 = getIntersectionLocation(..., i1s, u1s, ..., ls_i1, j1, pmessage);
ls_i2 = i2s[j1];   // line 586
ls_u2 = u2s[j1];   // line 587
```

If `i1s` is empty (no intersections found), the return value of `getIntersectionLocation` and the value written to `j1` are unspecified. Accessing `i2s[j1]` would then be out-of-bounds. The same pattern is repeated at lines 679–680 for the closed-curve case. Add a guard: if `i1s.empty()`, log and handle gracefully.

**[Bug] Leaked intermediate offset vertices in Step 1**

In the loop (lines 327–426), every iteration allocates `v1_tmp` and `v2_tmp`. Only a subset of these pointers end up in `vertices_tmp`; the rest are silently dropped. The intermediate allocation-only vertices (e.g., `v1_tmp` when `i > 0` whose point was used only to derive an intersection) are leaked on every pass.

**[Bug] Fragile closed-curve vertex extraction (lines 703–720)**

```cpp
bool keep = false, check;
for (auto v : lines_group[0]) {
    check = true;
    if (check && !keep && v == v0) { keep = true; check = false; }
    if (keep) { _tmp.push_back(v); }
    if (check && keep && v == v0) { keep = false; check = false; }
}
```

The intent is to extract the sub-sequence between the first and second occurrences of `v0`. However, the first `if` sets `check = false`, preventing the second `if` from ever firing *in the same iteration*. On the next iteration `check` is immediately reset to `true`, so the second occurrence of `v0` will eventually stop the loop — but only if `v0` appears **twice**. If `v0` appears only once (e.g., the intersection lands at an endpoint), `keep` is never set back to `false` and the entire tail of the list is included. The variable name `check` adds no clarity. Replace this with a well-named helper or a standard `std::find`-based slice.

**[Minor] Step 3 code duplication (lines 552–617 vs lines 641–755)**

The logic for "find intersection between neighbouring sub-lines, trim, adjust link indices" is repeated almost verbatim for the consecutive-pair case and the head-tail closed case. Extract into a shared helper.

**[Minor] `link_to_2` forward-fill (line 786–789)**

```cpp
for (auto i = 0; i < link_to_2.size(); i++) {
    if (i > 0 && link_to_2[i] == 0) {
        link_to_2[i] = link_to_2[i-1];
    }
    ...
}
```

This assumes index 0 is a sentinel for "no mapping". If a genuine offset vertex has index 0 (which it can — the first offset vertex is at index 0), the condition fires incorrectly. Use a sentinel value of `-1` or `std::optional<int>` to distinguish "unmapped" from "maps to vertex 0".

**[Style] Massive commented-out blocks**

~80 lines of commented-out debug `std::cout` statements and the old intersection method (lines 348–364, 428–448, 506–513, 621–636, etc.) add significant noise. Either remove them or replace with `if (config.debug) PLOG(debug) << ...` using the existing pattern already present in the function.

**[Style] Excess blank lines between functions**

Groups of 8–10 blank lines separate each function (e.g., lines 56–64, 92–101, 122–131). 1–2 blank lines is the convention in the rest of the codebase.

---

## Summary Table

| Severity | Count | Examples |
|---|---|---|
| **Bug** | 7 | Memory leaks (leaked `ls`, leaked `v1_tmp`/`v2_tmp`); out-of-bounds on `i2s[j1]`; fragile closed-curve loop; `link_to_2` false-zero sentinel; null-ptr `setPoint` |
| **Minor / Design** | 6 | `dr` mutated via reference; always-1 return value; dead variables × 7; Step 3 duplication; `const` correctness |
| **Style** | 3 | Commented-out debug blocks; dead `if (side == "left")` block; excess blank lines |

### Priority Fixes

| # | Issue | Recommended action |
|---|---|---|
| 1 | `offsetLineSegment(PGeoLineSegment*, SVector3&)`: callers leak the original `ls` | Delete `ls` before reassignment at all call sites, or change to in-place mutation |
| 2 | Step 1: `v1_tmp` / `v2_tmp` not pushed into `vertices_tmp` are leaked | Track which pointers are "owned" by `vertices_tmp` and delete the rest |
| 3 | Step 3: out-of-bounds on `i2s[j1]` when `i1s` is empty | Add empty-check guard on `i1s` before accessing `i2s[j1]` |
| 4 | Closed-curve single-group extraction (lines 703–720): `v0` must appear twice | Replace with `std::find`-based range slice |
| 5 | `link_to_2` forward-fill uses `0` as unmapped sentinel | Use `-1` as sentinel; update consumers |
| 6 | 7 dead variables in the main `offset` function | Remove all listed in the table above |
