# PreVABS build failure report

- Input: `test\integration\t9_airfoil\mh104_te_only.xml`
- Case: `mh104_te_only`
- Time: 2026-06-17 10:06:25
- Failure: fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 3376 (45/63) on curve 69 (on surface 16) (faces=30, global_mesh_size=0.005000)
- Catch context: initialize model

## What failed

Gmsh could not recover one generated mesh edge. This usually means the generated laminate face is too thin, self-intersecting, or topologically inconsistent near a local offset/join region.

## Diagnostic evidence

From `test\integration\t9_airfoil\mh104_te_only.log`:

- !!  offset (multi-vertex, open): only 8 offset verts produced for 18 base verts (M/N=0.44). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- xx  fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 3376 (45/63) on curve 69 (on surface 16) (faces=30, global_mesh_size=0.005000)

From `test\integration\t9_airfoil\mh104_te_only.debug.log`:

- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex): offset distance 0.021506 is 3.12442x the shortest base segment length (0.0068832 at segment 8); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  offset.cpp:offset:456
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): 6 base vertex/vertices have local half-thickness < |dist| = 0.021506 (first at base[6] = v#0( 0 , 1.36757 , -0.0076441 )); skin will be locally dropped at those locations  offset.cpp:offset:542
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): 4 base vertex/vertices have local half-thickness in [|dist|, 2*|dist|) = [0.021506, 0.043012); offset will be valid but locally thin  offset.cpp:offset:550
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): Stage E pre-trim removed base[5..13] (9 vertices) due to local half-thickness < 1.2 * |dist| = 0.0258072; skin will be reported dropped over these indices  offset.cpp:offset:640
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): skin dropped over base indices [5..14] due to insufficient local thickness  offset.cpp:offset:715
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: surface] offset (multi-vertex, open): only 8 offset verts produced for 18 base verts (M/N=0.44). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  offset.cpp:offset:756
- [debug]    [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component details: surface > segment areas: sg_te] buildAreas: derived staircase: 18 pairs (was) -> 18 pairs (derived); dropped ranges (was)=1, (derived)=1  PBuildSegmentAreas.cpp:buildAreas:734
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component details: surface > segment areas: sg_te] splitEdge: repeated splits on edge lineage #5 (3 splits in current phase), edge=he#65 v#6( 0 , 1.04293 , -0.0212941 ) -> v#32( 0 , 1.04491 , -0.0418133 ) | loop: loop#2 | face: f#2 [sg_te_face]  PDCEL.cpp:splitEdge:405
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component details: surface > segment areas: sg_te] splitEdge: repeated splits on edge lineage #5 (4 splits in current phase), edge=he#69 v#6( 0 , 1.04293 , -0.0212941 ) -> v#33( 0 , 1.04405 , -0.0328452 ) | loop: loop#2 | face: f#2 [sg_te_face]  PDCEL.cpp:splitEdge:405
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component details: surface > segment areas: sg_te] splitEdge: repeated splits on edge lineage #31 (3 splits in current phase), edge=he#85 v#36( 0 , 1.06711 , -0.0396668 ) -> v#25( 0 , 1.06508 , -0.0191514 ) | loop: loop#4 | face: f#3 [sg_te_area_1]  PDCEL.cpp:splitEdge:405

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `sg_te`
- Repair range: base[5..14]
- Suggested taper range: base[3..16]
- Stage E pre-trim range: base[5..13]
- First thin base index: base[6]
- Design half-thickness: 0.02
- Current offset distance: 0.021506
- Initial trial local half-thickness `t'`: 0.018 (safety = 0.9)
- Offset/base vertex ratio: 8/18 = 0.44

Use this as the starting point for a future opt-in adaptive thickness repair. A strict automatic value requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `test\integration\t9_airfoil\mh104_te_only.log`
- Debug log: `test\integration\t9_airfoil\mh104_te_only.debug.log`
- DCEL dump: `test\integration\t9_airfoil\mh104_te_only.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
