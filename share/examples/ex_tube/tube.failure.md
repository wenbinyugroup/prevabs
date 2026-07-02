# PreVABS build failure report

- Input: `.\share\examples\ex_tube\tube.xml`
- Case: `tube`
- Time: 2026-07-02 09:44:13
- Failure: fatal exception: homogenization failed: layered offset[sgm_1]: failed to tile closed layer 2 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `.\share\examples\ex_tube\tube.log`:

- xx  fatal exception: homogenization failed: layered offset[sgm_1]: failed to tile closed layer 2 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)

From `.\share\examples\ex_tube\tube.debug.log`:

- [warning]   offset (multi-vertex): offset distance 1.4 is 2.67411x the shortest base segment length (0.523539 at segment 77); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237
- [info]      creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:411
- [info]      writing gmsh .geo file: .\share\examples\ex_tube\tube_debug_after_cmp_1.geo_unrolled  PModelIOGmsh.cpp:writeGmshGeo:347
- [debug]     buildAreas: derived staircase: 121 pairs (was) -> 121 pairs (derived); dropped ranges (was)=0, (derived)=0  PBuildSegmentAreas.cpp:buildAreas:2284
- [error]     fatal exception: homogenization failed: layered offset[sgm_1]: failed to tile closed layer 2 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)  main.cpp:logFatalWithProgress:723

## Related files

- Log: `.\share\examples\ex_tube\tube.log`
- Debug log: `.\share\examples\ex_tube\tube.debug.log`
- DCEL dump: `.\share\examples\ex_tube\tube.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
