# PreVABS build failure report

- Input: `test\integration\t9_airfoil\airfoil_skin_only_normalized.xml`
- Case: `airfoil_skin_only_normalized`
- Time: 2026-06-18 17:32:25
- Failure: fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 35854 (39/65) on curve 496 (on surface 166) (faces=313, global_mesh_size=0.005000)
- Catch context: initialize model

## What failed

Gmsh could not recover one generated mesh edge. This usually means the generated laminate face is too thin, self-intersecting, or topologically inconsistent near a local offset/join region.

## Diagnostic evidence

From `test\integration\t9_airfoil\airfoil_skin_only_normalized.log`:

- !!  offset (multi-vertex, open): only 19 offset verts produced for 29 base verts (M/N=0.66). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- xx  fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 35854 (39/65) on curve 496 (on surface 166) (faces=313, global_mesh_size=0.005000)

From `test\integration\t9_airfoil\airfoil_skin_only_normalized.debug.log`:

- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.010431 is 7.6944x the shortest base segment length (0.00135566 at segment 8); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.018381 is 1.71739x the shortest base segment length (0.0107029 at segment 6); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.061256 is 3.10895x the shortest base segment length (0.0197031 at segment 10); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.021506 is 3.12438x the shortest base segment length (0.00688329 at segment 13); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): 6 base vertex/vertices have local half-thickness < |dist| = 0.021506 (first at base[11] = v#0( 0 , 1.36757 , -0.00764419 )); skin will be locally dropped at those locations  offset.cpp:offset:542
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): 4 base vertex/vertices have local half-thickness in [|dist|, 2*|dist|) = [0.021506, 0.043012); offset will be valid but locally thin  offset.cpp:offset:550
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): Stage E pre-trim removed base[10..18] (9 vertices) due to local half-thickness < 1.2 * |dist| = 0.0258072; skin will be reported dropped over these indices  offset.cpp:offset:640
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): skin dropped over base indices [10..19] due to insufficient local thickness  offset.cpp:offset:715
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): only 19 offset verts produced for 29 base verts (M/N=0.66). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  offset.cpp:offset:756
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.061256 is 1.47583x the shortest base segment length (0.041506 at segment 9); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `sg_te`
- Repair range: base[10..19]
- Suggested taper range: base[8..21]
- Stage E pre-trim range: base[10..18]
- First thin base index: base[11]
- Design half-thickness: 0.02
- Current offset distance: 0.021506
- Initial trial local half-thickness `t'`: 0.018 (safety = 0.9)
- Offset/base vertex ratio: 19/29 = 0.66

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `test\integration\t9_airfoil\airfoil_skin_only_normalized.log`
- Debug log: `test\integration\t9_airfoil\airfoil_skin_only_normalized.debug.log`
- DCEL dump: `test\integration\t9_airfoil\airfoil_skin_only_normalized.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
