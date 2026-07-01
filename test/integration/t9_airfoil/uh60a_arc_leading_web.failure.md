# PreVABS build failure report

- Input: `.\test\integration\t9_airfoil\uh60a_arc_leading_web.xml`
- Case: `uh60a_arc_leading_web`
- Time: 2026-06-25 15:44:52
- Failure: fatal exception: homogenization failed: bad allocation
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `.\test\integration\t9_airfoil\uh60a_arc_leading_web.log`:

- xx  fatal exception: homogenization failed: bad allocation

From `.\test\integration\t9_airfoil\uh60a_arc_leading_web.debug.log`:

- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: spar] offset (multi-vertex): offset distance 0.2834 is 6.83379x the shortest base segment length (0.0414704 at segment 15); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: spar] offset (multi-vertex): offset distance 0.2834 is 6.83485x the shortest base segment length (0.041464 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: spar] offset (multi-vertex): offset distance 0.2834 is 5.24313x the shortest base segment length (0.0540517 at segment 3); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [info]     [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build] creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237
- [info]     [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build] creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:379
- [info]     [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build] writing gmsh .geo file: .\test\integration\t9_airfoil\uh60a_arc_leading_web_debug_after_spar.geo_unrolled  PModelIOGmsh.cpp:writeGmshGeo:347
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: le] offset (multi-vertex): offset distance 0.1062 is 74.9253x the shortest base segment length (0.00141741 at segment 27); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: le] offset (multi-vertex): offset distance 0.1062 is 1.9145x the shortest base segment length (0.0554713 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: le] offset (multi-vertex): offset distance 0.1062 is 2.07768x the shortest base segment length (0.0511147 at segment 16); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [info]     [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build] creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237

## Related files

- Log: `.\test\integration\t9_airfoil\uh60a_arc_leading_web.log`
- Debug log: `.\test\integration\t9_airfoil\uh60a_arc_leading_web.debug.log`
- DCEL dump: `.\test\integration\t9_airfoil\uh60a_arc_leading_web.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
