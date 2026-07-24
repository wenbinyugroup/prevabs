# PreVABS build failure report

- Input: `.\mh104_9webs.xml`
- Case: `mh104_9webs`
- Time: 2026-07-24 16:12:01
- Failure: fatal exception: homogenization failed: DCEL loop walk exceeded 65536 iterations | start=he#258 v#130 -> v#129 | loop: loop#14 | current=he#284 v#136 -> v#135 | loop: loop#14 | face=f#6 [sgm_1_2_face]
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `.\mh104_9webs.log`:

- xx  fatal exception: homogenization failed: DCEL loop walk exceeded 65536 iterations | start=he#258 v#130 -> v#129 | loop: loop#14 | current=he#284 v#136 -> v#135 | loop: loop#14 | face=f#6 [sgm_1_2_face]

From `.\mh104_9webs.debug.log`:

- [warning]   offset (multi-vertex): offset distance 0.012551 is 4.51714x the shortest base segment length (0.00277853 at segment 33); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:125
- [warning]   offset (multi-vertex, closed): 4 base vertex/vertices have local half-thickness < |dist| = 0.012551 (first at base[1] = v#0( 0 , 1.41777 , 0.000323893 )); skin will be locally dropped at those locations  geo_diagnostics.cpp:warnLocalThinRegions:150
- [warning]   offset (multi-vertex, closed): 4 base vertex/vertices have local half-thickness in [|dist|, 2*|dist|) = [0.012551, 0.025102); offset will be valid but locally thin  geo_diagnostics.cpp:warnLocalThinRegions:158
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [34..34] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [62..65] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [warning]   offset (multi-vertex, closed): skin dropped over base indices [0..3] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:239
- [info]      creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:413
- [info]      writing gmsh .geo file: .\mh104_9webs_debug_after_surface.geo_unrolled  PModelIOGmsh.cpp:writeGmshGeo:348
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:239

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `sg_le`
- Adaptive thickness plan: unavailable (base_count must be positive)
- First thin base index: base[1]
- Current offset distance: 0.012551

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `.\mh104_9webs.log`
- Debug log: `.\mh104_9webs.debug.log`
- DCEL dump: `.\mh104_9webs.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
