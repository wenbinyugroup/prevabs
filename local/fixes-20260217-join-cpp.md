# Bug Fixes: `src/cs/join.cpp`

Date: 2026-02-17
Reviewed: [review-20260216-join-cpp.md](review-20260216-join-cpp.md)

---

## Fix 1 — `ls_i` → `ls_i_tmp` in intersection selection conditions

**Locations:** base-curve loop (×4 conditions), offset-curve loop (×4 conditions)

**Problem:** Local variable `ls_i` was used inside both intersection loops in the "inner line segment" and "same line segment" conditions, but `ls_i` is not assigned until *after* the loop exits (line 226/356). The intent is to compare the current candidate segment index against the best-so-far (`ls_i_prev`). The correct variable is `ls_i_tmp`, which `findCurvesIntersection` writes to on every call.

**Change:** Replaced all eight occurrences of `ls_i >`, `ls_i <`, and `ls_i ==` inside the loop bodies with `ls_i_tmp >`, `ls_i_tmp <`, and `ls_i_tmp ==`.

---

## Fix 2 — `u1_tmp` → `u1` in post-loop `ls_i` adjustment

**Locations:** lines 227 and 356 (one per curve)

**Problem:** After each intersection loop, `ls_i` is incremented by one if the accepted intersection lands at the end of a segment (`fabs(1 - u) <= TOLERANCE`). The code used `u1_tmp` (value from the last loop iteration, regardless of acceptance) instead of `u1` (the accepted best value).

**Change:**
```cpp
// before
if ((fabs(1 - u1_tmp) <= TOLERANCE) || e == 1) ls_i += 1;
// after
if ((fabs(1 - u1) <= TOLERANCE) || e == 1) ls_i += 1;
```

---

## Fix 3 — Missing `INF` guard before offset curve `he_tool` dereference

**Location:** before the offset curve `he_tool` dereference block (~line 365)

**Problem:** The base-curve section has an early-return guard when no intersection is found (`fabs(u1) == INF`). The offset-curve section resets `u1` for its own search but had no such guard. If the offset curve found no intersection, `he_tool` would be a stale or uninitialized pointer and the subsequent dereference would be UB.

**Change:** Added the same guard pattern used for the base curve immediately before the offset-curve `he_tool` dereference:
```cpp
if (fabs(u1) == INF) {
  PLOG(debug) << pmessage->message("offset curve: no intersection found, using free end.");
  createSegmentFreeEnd(s, e, bcfg, pmessage);
  pmessage->decreaseIndent();
  return;
}
```

---

## Fix 4 — Uninitialized `bl1_off_new` / `v1_new` when style-2 finds no intersection

**Location:** style-2 block, after the while loop (~line 1260)

**Problem:** When the while loop exhausted all segment pairs without setting `found = true`, `bl1_off_new`, `bl2_off_new`, `v1_new`, and `v2_new` were all uninitialized. Lines 1262–1276 unconditionally passed them to `setCurveOffset` and `setHeadVertexOffset`, causing UB.

**Change:** Added an `else` branch on the existing `if (found)` block that cleans up, logs, and returns early:
```cpp
else {
  for (auto p : lss1) delete p;
  for (auto p : lss2) delete p;
  PLOG(debug) << pmessage->message("style 2: no intersection found between offset curves, skipping join.");
  pmessage->decreaseIndent();
  return;
}
```

---

## Fix 5 — `tmp_hel_out->log()` → `hel->log()` in debug loop

**Location:** debug block ~line 166

**Problem:** The loop `for (auto hel : hels)` logged `tmp_hel_out` (the outer enclosing loop) on every iteration instead of the loop variable `hel`. Debug output showed the same loop N times.

**Change:**
```cpp
// before
for (auto hel : hels) { tmp_hel_out->log(); }
// after
for (auto hel : hels) { hel->log(); }
```

---

## Fix 6 — Initialize `u1_tmp` to avoid UB when `hels` is empty

**Location:** declaration at line ~58

**Problem:** When `hels` is empty the intersection loop does not execute, leaving `u1_tmp` uninitialized. Line 227 reads `u1_tmp` before the `fabs(u1) == INF` early-return guard triggers.

**Change:**
```cpp
// before
double u1, u2, u1_tmp, u2_tmp;
// after
double u1, u2, u1_tmp{0.0}, u2_tmp;
```

---

## Fix 7 — Remove unused `ls_b` allocation (memory leak)

**Location:** style-1 block, ~line 645

**Problem:** `PGeoLineSegment *ls_b = new PGeoLineSegment(v, b)` was allocated but never referenced again, leaking on every style-1 call.

**Change:** Line deleted entirely.

---

## Fix 8 — Delete `lss1` / `lss2` contents to prevent leak

**Location:** style-2 block, after `v_intersect` is resolved (~line 1228) and in the `!found` early-return branch

**Problem:** `lss1` and `lss2` accumulate `new PGeoLineSegment` raw pointers during the while loop and were never freed.

**Change:** Added cleanup at both exit paths:
```cpp
// in the found branch (after v_intersect is obtained, before Baseline construction)
for (auto p : lss1) delete p;
for (auto p : lss2) delete p;

// in the !found early-return branch (Fix 4 else block)
for (auto p : lss1) delete p;
for (auto p : lss2) delete p;
```
