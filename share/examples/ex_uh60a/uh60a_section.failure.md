# PreVABS build failure report

- Input: `.\share\examples\ex_uh60a\uh60a_section.xml`
- Case: `uh60a_section`
- Time: 2026-07-02 09:45:14
- Failure: fatal exception: homogenization failed: layered offset[main_spar]: failed to tile closed layer 1 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `.\share\examples\ex_uh60a\uh60a_section.log`:

- xx  fatal exception: homogenization failed: layered offset[main_spar]: failed to tile closed layer 1 after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)

From `.\share\examples\ex_uh60a\uh60a_section.debug.log`:

- [warning]   offset (multi-vertex): offset distance 0.00565 is 7.10446x the shortest base segment length (0.000795275 at segment 38); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [38..38] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237
- [info]      creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:411
- [info]      writing gmsh .geo file: .\share\examples\ex_uh60a\uh60a_section_debug_after_main_spar.geo_unrolled  PModelIOGmsh.cpp:writeGmshGeo:347
- [warning]   offset (multi-vertex): offset distance 0.00342 is 6.45309x the shortest base segment length (0.000529978 at segment 17); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]   offset (multi-vertex): offset distance 0.00342 is 4.9254x the shortest base segment length (0.00069436 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]   offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:608
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:237

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `le_top_2`
- Adaptive thickness plan: unavailable (base_count must be positive)

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `.\share\examples\ex_uh60a\uh60a_section.log`
- Debug log: `.\share\examples\ex_uh60a\uh60a_section.debug.log`
- DCEL dump: `.\share\examples\ex_uh60a\uh60a_section.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
