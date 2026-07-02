# PreVABS build failure report

- Input: `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.xml`
- Case: `uh60a_arc_leading_web`
- Time: 2026-07-01 22:22:57
- Failure: fatal exception: homogenization failed: SEH exception 0xC0000005 (access violation) at 00007FF7403CFDD6 while reading address 0x0
- Catch context: initialize model

## What failed

PreVABS hit a low-level runtime exception while building the cross-section geometry.

## Diagnostic evidence

From `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.log`:

- xx  fatal exception: homogenization failed: SEH exception 0xC0000005 (access violation) at 00007FF7403CFDD6 while reading address 0x0

From `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.debug.log`:

- [warning]   offset (multi-vertex): offset distance 0.2834 is 6.83485x the shortest base segment length (0.041464 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]   offset (multi-vertex, closed): 4 base vertex/vertices have local half-thickness < |dist| = 0.2834 (first at base[1] = v#0( 0 , 18.7255 , -0.803391 )); skin will be locally dropped at those locations  geo_diagnostics.cpp:warnLocalThinRegions:148
- [warning]   offset (multi-vertex, closed): 2 base vertex/vertices have local half-thickness in [|dist|, 2*|dist|) = [0.2834, 0.5668); offset will be valid but locally thin  geo_diagnostics.cpp:warnLocalThinRegions:156
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [17..17] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [58..58] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [72..77] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [90..92] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [0..2] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237
- [info]      creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:411

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `sgm_blk_1`
- Adaptive thickness plan: unavailable (base_count must be positive)
- First thin base index: base[1]
- Current offset distance: 0.2834

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.log`
- Debug log: `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.debug.log`
- DCEL dump: `.\share\examples\ex_airfoil_curve_web\uh60a_arc_leading_web.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
