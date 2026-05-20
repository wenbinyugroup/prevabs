# Minimal repro: open multi-vertex U-bend with thin-TE drop fan

Tracks [issue-20260518-clipper2-airfoil-gmsh-sensitivity §5.1 G.1](../../../../rnd/notes/dev/prevabs/dev-notes/todo/issue-20260518-clipper2-airfoil-gmsh-sensitivity.md).

## Status (2026-05-18)

| Commit | Behaviour |
|---|---|
| `bc2229c` (Phase A-I done, closed-only routing) | ✅ PASS — `splitEdge: repeated splits` warnings only; finishes 0.26 s |
| `2aaa40b` HEAD (Phase 1-4 + Phase R + bug fixes) | ❌ **CRASH** — SEH access violation 0xC0000005 |

## What it tests

7-vertex U-bend open polyline with parallel sides at z = ±0.065. Layup
total thickness 0.080. Default `side` (left of walk) puts the offset
on the OUTSIDE of the U — covered but with thin local headroom at the
bend apex p3, where 4 base vertices end up in the
`[|dist|, 2*|dist|)` band of Stage E precheck.

After Phase 1's `extractOpenRuns` resample, base vertices near the
apex (p2/p3/p4) get forward-filled to the same boundary offset
vertex. Downstream `PSegment::build` then crowds many layup
intersections onto the single boundary edge, splitting it 6+ times
and eventually producing degenerate edges that segfault later.

## Run

```bash
cd test/debug/clipper2_open_thin_te
../../../build_msvc/Release/prevabs.exe -i thin_te_ubend.xml --hm
```

Expected at HEAD: `xx fatal exception: homogenization failed: SEH exception 0xC0000005`.
Expected at bc2229c: `OK prevabs finished` after multiple `splitEdge`
warnings.

## Why this matters

This is a tiny isolated baseline that exhibits the **same failure
mode** as the 3 production cases that fail today:

- `t9_airfoil/mh104` (sg_te)
- `t9_airfoil/airfoil_skin_only` (sg_te)
- `t9_airfoil/airfoil_skin_only_normalized` (sg_te)

All four have an open multi-vertex baseline that wraps around a
sharp turn / TE region thin enough that some interior base vertices
exceed local headroom. Fixing this repro should fix the three
production cases too.

## Related

- [issue-20260518](../../../../rnd/notes/dev/prevabs/dev-notes/todo/issue-20260518-clipper2-airfoil-gmsh-sensitivity.md)
- [plan-20260518](../../../../rnd/notes/dev/prevabs/dev-notes/todo/plan-20260518-open-polyline-butt-side-filter.md)
- Path A / B findings: [issue §2.1–2.4](../../../../rnd/notes/dev/prevabs/dev-notes/todo/issue-20260518-clipper2-airfoil-gmsh-sensitivity.md#21-实测最大偏差)
