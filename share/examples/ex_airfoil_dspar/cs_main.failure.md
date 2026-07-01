# PreVABS build failure report

- Input: `share\examples\ex_airfoil_dspar\cs_main.xml`
- Case: `cs_main`
- Time: 2026-06-30 11:55:21
- Failure: fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 1584 (1/2) on curve 345 (on surface 7) (faces=578, global_mesh_size=0.01, elapsed_s=0.207893) | gmsh log: Warning: Impossible to recover edge 5 5 (error tag -1) | Error: Unable to recover the edge 1584 (1/2) on curve 345 (on surface 7)
- Catch context: initialize model

## What failed

Gmsh could not recover one generated mesh edge. This usually means the generated laminate face is too thin, self-intersecting, or topologically inconsistent near a local offset/join region.

## Diagnostic evidence

From `share\examples\ex_airfoil_dspar\cs_main.log`:

- !!  offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- !!  offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.00 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- xx  fatal exception: homogenization failed: gmsh mesh generation failed: Unable to recover the edge 1584 (1/2) on curve 345 (on surface 7) (faces=578, global_mesh_size=0.01, elapsed_s=0.207893) | gmsh log: Warning: Impossible to recover edge 5 5 (error tag -1) | Error: Unable to recover the edge 1584 (1/2) on curve 345 (on surface 7)

From `share\examples\ex_airfoil_dspar\cs_main.debug.log`:

- [warning]   offset (multi-vertex): offset distance 0.0152834 is 7.87621x the shortest base segment length (0.00194045 at segment 35); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]   offset (multi-vertex, open): skin dropped over base indices [34..37] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  geo_diagnostics.cpp:warnLowOffsetMNRatio:193
- [warning]   offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.00 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  geo_diagnostics.cpp:warnLowOffsetMNRatio:193
- [warning]   splitEdge: repeated splits on edge lineage #5 (3 splits in current phase), edge=he#1055 v#6( 0 , 1.48578 , -0.0502117 ) -> v#410( 0 , 1.47498 , -0.0607598 ) | loop: loop#235 | face: f#229  PDCEL.cpp:splitEdge:488
- [warning]   splitEdge: repeated splits on edge lineage #7 (3 splits in current phase), edge=he#1061 v#411( 0 , 1.47498 , 0.0873917 ) -> v#9( 0 , 1.48578 , 0.0769483 ) | loop: loop#235 | face: f#229  PDCEL.cpp:splitEdge:488
- [warning]   splitEdge: repeated splits on edge lineage #5 (4 splits in current phase), edge=he#1501 v#6( 0 , 1.48578 , -0.0502117 ) -> v#505( 0 , 1.47858 , -0.0572438 ) | loop: loop#487 | face: f#481  PDCEL.cpp:splitEdge:488
- [warning]   splitEdge: repeated splits on edge lineage #7 (4 splits in current phase), edge=he#1507 v#506( 0 , 1.47858 , 0.0839105 ) -> v#9( 0 , 1.48578 , 0.0769483 ) | loop: loop#487 | face: f#481  PDCEL.cpp:splitEdge:488

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Adaptive thickness plan: unavailable (design_half_thickness must be positive)
- Offset/base vertex ratio: 2/3 = 0.67

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `share\examples\ex_airfoil_dspar\cs_main.log`
- Debug log: `share\examples\ex_airfoil_dspar\cs_main.debug.log`
- DCEL dump: `share\examples\ex_airfoil_dspar\cs_main.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
