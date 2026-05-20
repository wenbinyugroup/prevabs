# Repro attempt: open multi-vertex with near-degenerate segment

Tracks [issue-20260518-clipper2-airfoil-gmsh-sensitivity §5.1 G.2](../../../../rnd/notes/dev/prevabs/dev-notes/todo/issue-20260518-clipper2-airfoil-gmsh-sensitivity.md).

## Status (2026-05-18) — **does NOT reproduce in isolation**

| Commit | Behaviour |
|---|---|
| `bc2229c` (Phase A-I done) | ✅ PASS — splitEdge warnings, finishes 0.04 s |
| `2aaa40b` HEAD | ✅ PASS — splitEdge warnings, finishes 0.03 s |

Both commits succeed despite the same `splitEdge: repeated splits`
warnings on the tiny segment. The downstream layup-intersection
crash that affects `t9_airfoil/main_cs_set1` does NOT trigger in
this isolated geometry.

## What's still missing

The production `main_cs_set1/seg_front_le` failure depends on
**context that this minimal repro doesn't capture**:

- Multi-component join (le component has multiple segments;
  seg_front_le is joined with seg_back_le or similar adjacents)
- Closed surrounding airfoil component that the segment ends touch
- Layup transitions across joins

Single isolated segment with collinear 8-vertex baseline + one tiny
0.005-millimetre segment does NOT crash, even though the same
`splitEdge` mechanism fires.

## Until G.2 minimal repro is found

Use `t9_airfoil/main_cs_set1.xml` itself as the canonical repro for
the G.2 sub-issue. Compare:

```bash
# At bc2229c source:
prevabs.exe -i test/integration/t9_airfoil/main_cs_set1.xml --hm  # passes

# At HEAD:
prevabs.exe -i test/integration/t9_airfoil/main_cs_set1.xml --hm  # SEH or Gmsh fail
```

## What this repro DOES show

The minimal geometry confirms that:

1. **A single near-degenerate segment alone is NOT sufficient** to
   trigger the bug. The Clipper2 backend + Phase 1 resample handles
   isolated tiny segments correctly (output 8 offset vertices,
   downstream tolerates).
2. The G.2 root cause is therefore **interaction with surrounding
   geometry** (join / multi-component / closed-loop boundary).

This narrows the fix scope: G.2 likely needs a tweak in
`PSegment::build` join handling, not in `extractOpenRuns`.

## Related

- [issue-20260518](../../../../rnd/notes/dev/prevabs/dev-notes/todo/issue-20260518-clipper2-airfoil-gmsh-sensitivity.md)
- [G.1 minimal repro](../clipper2_open_thin_te/) — does reproduce its sub-issue
