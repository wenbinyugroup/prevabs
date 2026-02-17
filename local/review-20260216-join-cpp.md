# Code Review: `src/cs/join.cpp`

Date: 2026-02-16

---

## Overview

`join.cpp` implements three `PComponent` methods that trim and connect segment ends in the DCEL-based cross-section builder:

| Function | Lines | Purpose |
|---|---|---|
| `joinSegments(s, e, v, bcfg, pmessage)` | 27–563 | Trim one segment end against surrounding geometry |
| `joinSegments(s1, s2, e1, e2, v, style, bcfg, pmessage)` | 583–1320 | Join two segment ends with style 1 (bisector) or style 2 (curve intersection) |
| `createSegmentFreeEnd(s, e, bcfg, pmessage)` | 1340–1592 | Create a free (unconstrained) segment end |

The overall algorithmic design is sound. The main concerns are: several cases of undefined behavior from uninitialized local variables, a missing null-check before dereferencing in the offset curve section, wrong variables used in comparison conditions (copy-paste bugs), a completely unused allocation, and pervasive residual debug noise.

---

## Issues

### `joinSegments(s, e, v, bcfg, pmessage)` — single-segment overload

---

#### [Bug] `ls_i` used uninitialized in intersection condition — lines 199–200, 213–214, 328–330, 342–343

`ls_i` is declared at line 57 with no initializer:
```cpp
int ls_i, ls_i_prev, ls_i_tmp;
```
`ls_i_prev` is set before each loop (lines 62–68, 302–309), but `ls_i` is **never** assigned until after the loop (line 226). Inside both the base-curve and offset-curve intersection loops, the condition reads `ls_i`:

```cpp
|| (...&& ls_i > ls_i_prev)   // line 199 — ls_i is uninitialized
|| (...&& ls_i == ls_i_prev && u1_tmp > u1)  // line 200 — same
```

The intent of the "inner line segment" branch is to accept intersections that are further inward than the current best. The correct variable is `ls_i_tmp` (the current candidate's segment index, set by `findCurvesIntersection`). Using the uninitialized `ls_i` is undefined behavior; in practice this branch silently never fires or fires spuriously depending on stack state.

**Fix:** Replace `ls_i` with `ls_i_tmp` in all four condition lines (199, 200, 213, 214 for base curve; 328, 329, 342, 343 for offset curve).

---

#### [Bug] `u1_tmp` used instead of `u1` for post-loop `ls_i` adjustment — lines 227, 356

After each intersection loop, `ls_i` is adjusted by one if the best intersection landed at the *end* of a segment:
```cpp
ls_i = ls_i_prev;
if ((fabs(1 - u1_tmp) <= TOLERANCE) || e == 1) ls_i += 1;  // lines 227, 356
```
`u1_tmp` holds the raw value from the **last call** to `findCurvesIntersection`, regardless of whether that call produced the accepted best intersection. The adjustment should use `u1` (the accepted best parametric value). Using `u1_tmp` causes the wrong segment index whenever the last-tried intersection is not the best one.

**Fix:** Replace `u1_tmp` with `u1` on both lines 227 and 356.

---

#### [Bug] No `u1 == INF` guard before dereferencing `he_tool` in offset curve section — lines 366–377

For the base curve, there is a guard (lines 236–241) that returns early if no intersection was found (`fabs(u1) == INF`). The offset curve section resets `u1` at lines 302–309 for its own search but has **no equivalent guard**. If the offset curve loop finds no intersection, `u1` remains `±INF`, `he_tool` still points to the base curve's last accepted half-edge (or is uninitialized if the base curve had no match), and lines 366–377 dereference it unconditionally:

```cpp
// No guard here — he_tool may be stale or uninitialized
if (fabs(u2) <= TOLERANCE) {
  v_new = he_tool->source();    // line 367 — crash / wrong result
}
```

**Fix:** Add a guard identical to lines 236–241 before line 365, logging and calling `createSegmentFreeEnd` or returning appropriately.

---

#### [Bug] Wrong variable logged in debug half-edge loop — line 166

```cpp
for (auto hel : hels) {
  tmp_hel_out->log();   // BUG: should be hel->log()
}
```
The loop variable `hel` is never used. `tmp_hel_out` (the outer enclosing loop) is logged N times — one for each entry in `hels` — rather than logging each distinct loop. This produces misleading debug output.

**Fix:** Replace `tmp_hel_out->log()` with `hel->log()`.

---

#### [Bug] `u1_tmp` read uninitialized when `hels` is empty — line 227

If `hels` is empty the intersection loop body never runs, so `u1_tmp` is never written. Line 227 reads `u1_tmp` to decide whether to increment `ls_i`:
```cpp
if ((fabs(1 - u1_tmp) <= TOLERANCE) || e == 1) ls_i += 1;
```
Although the subsequent `fabs(u1) == INF` check (line 236) would trigger and return, the UB occurs first on line 227.

**Fix:** Initialize `u1_tmp = 0.0` at the declaration on line 58, or restructure so the `ls_i` adjustment is inside a block guarded by `fabs(u1) != INF`.

---

### `joinSegments(s1, s2, e1, e2, v, style, bcfg, pmessage)` — two-segment overload

---

#### [Bug] `bl1_off_new`, `bl2_off_new`, `v1_new`, `v2_new` uninitialized when style==2 finds no intersection — lines 1197–1256

When `found == false` after the style-2 while loop breaks at line 1185, `bl1_off_new` and `bl2_off_new` are never constructed, and `v1_new` / `v2_new` are never assigned. Lines 1255–1256 unconditionally use them:
```cpp
s1->setCurveOffset(bl1_off_new);  // UB — uninitialized pointer
s2->setCurveOffset(bl2_off_new);  // UB
```
And lines 1264–1276 set head/tail offset vertices from `v1_new`/`v2_new`. All are UB.

**Fix:** Add a `!found` early-return (or error log) before line 1255, or initialize `bl1_off_new = nullptr` / `bl2_off_new = nullptr` and guard the `setCurveOffset` calls.

---

#### [Memory Leak] `PGeoLineSegment` objects in `lss1` / `lss2` never deleted — lines 1106, 1119

```cpp
ls1 = new PGeoLineSegment(v11, v12);
lss1.push_back(ls1);
// ...
ls2 = new PGeoLineSegment(v21, v22);
lss2.push_back(ls2);
```
`lss1` and `lss2` accumulate raw owning pointers. Neither vector has its contents deleted at any exit point of the style-2 block.

**Fix:** Use `std::unique_ptr<PGeoLineSegment>` in the vectors, or add a cleanup loop before each `break` / function return.

---

#### [Memory Leak] `ls_b` allocated but never used — line 638

```cpp
PGeoLineSegment *ls_b = new PGeoLineSegment(v, b);
```
`ls_b` is never referenced after this line. It is a dead allocation that leaks on every call with style==1.

**Fix:** Delete this line entirely.

---

### `createSegmentFreeEnd` — lines 1340–1592

No new categories of bugs beyond what is already present in the first overload. The index-pair adjustment logic (steps 1–3) mirrors the single-segment `joinSegments` pattern and has the same structural correctness; no additional UB issues observed here.

---

## Style / Maintenance

#### [Style] `ls_i` used in conditions should be `ls_i_tmp` — also a readability problem

Even setting aside the UB, substituting `ls_i_tmp` makes the intent of the three-clause OR condition self-evident without requiring readers to track variable lifetimes across the loop boundary.

#### [Style] Magic integers for `e` and `style` parameters

`e == 0` / `e == 1` (head / tail) and `style == 1` / `style == 2` (step / non-step) appear in 30+ branch conditions with no named constants. Define an enum or `static constexpr int` for each.

#### [Style] "Old method" dead code blocks

Throughout all three functions, every index-pair adjustment step has a corresponding `// Old method` block of commented-out `baseOffsetIndicesLink` manipulations (e.g., lines 459–467, 515–520, 738–753, 787–799, 814–840, 931–946, 1011–1018, 1028–1034). These are ~100 lines of residue. Remove them.

#### [Style] Pervasive commented-out `std::cout` debug statements

~60 commented-out `std::cout` / `pmessage->print()` lines exist throughout (e.g., lines 85, 101, 107–110, 116, 190–193, 229–232, 253–254, 261–267, 319–321, 358–361, 381–384, 495–496, 527–531). Remove them or promote to `PLOG(debug)` — the infrastructure is already present.

#### [Style] Excess blank lines between functions

Groups of 8–18 blank lines separate the three top-level functions (lines 565–582, 1322–1339). 1–2 blank lines is the convention used elsewhere in the codebase.

---

## Summary Table

| Severity | Count | Examples |
|---|---|---|
| **Bug / UB** | 5 | `ls_i` uninitialized in condition; `u1_tmp` vs `u1` post-loop; no INF guard for offset; wrong variable in debug loop; uninitialized `bl1_off_new` when !found |
| **Memory Leak** | 2 | `lss1`/`lss2` raw pointers; `ls_b` unused allocation |
| **Style / Maintenance** | 4 | Magic integers; dead "old method" blocks; commented-out cout; excess blank lines |

### Priority Fixes

| # | Location | Issue | Action |
|---|---|---|---|
| 1 | Lines 199–200, 213–214, 328–330, 342–343 | `ls_i` should be `ls_i_tmp` in loop conditions | Replace `ls_i` with `ls_i_tmp` in all condition branches |
| 2 | Lines 227, 356 | `u1_tmp` should be `u1` for post-loop `ls_i` adjustment | Change `u1_tmp` → `u1` |
| 3 | Lines 365–377 | No guard for offset curve finding no intersection | Add `if (fabs(u1) == INF) { createSegmentFreeEnd / return; }` before line 365 |
| 4 | Lines 1197–1256 | Uninitialized `bl1_off_new`, `v1_new`, etc. when style-2 finds no intersection | Add `!found` guard / early return after the while loop |
| 5 | Line 166 | `tmp_hel_out->log()` should be `hel->log()` | Replace with loop variable |
| 6 | Line 58 | `u1_tmp` uninitialized when `hels` is empty | Initialize `u1_tmp = 0.0` |
| 7 | Line 638 | `ls_b` allocated but never used | Delete the line |
| 8 | Lines 1106, 1119 | `lss1`/`lss2` raw pointer leaks | Use `unique_ptr` or manual cleanup |
