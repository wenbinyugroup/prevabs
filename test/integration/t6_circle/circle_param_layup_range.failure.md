# PreVABS build failure report

- Input: `test/integration/t6_circle/circle_param_layup_range.xml`
- Case: `circle_param_layup_range`
- Time: 2026-06-25 18:10:18
- Failure: fatal exception: homogenization failed: layered offset[sgm_blk_1_2]: failed to split layer 2 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `test/integration/t6_circle/circle_param_layup_range.log`:

- xx  fatal exception: homogenization failed: layered offset[sgm_blk_1_2]: failed to split layer 2 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)

From `test/integration/t6_circle/circle_param_layup_range.debug.log`:

- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.4 is 5.7314x the shortest base segment length (0.069791 at segment 14); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.6 is 5.7314x the shortest base segment length (0.104687 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.8 is 22.9256x the shortest base segment length (0.0348955 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:647
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.6 is 3.43884x the shortest base segment length (0.174478 at segment 3); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.4 is 3.82093x the shortest base segment length (0.104687 at segment 4); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.2 is 2.8657x the shortest base segment length (0.069791 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex): offset distance 0.4 is 11.4628x the shortest base segment length (0.0348955 at segment 0); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:123
- [warning]  [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component: wall] offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:647
- [error]    [initialize model > homogenization pipeline > homogenize > build shape > build geometry > cross section build > component details: wall > segment areas: sgm_blk_1_2] splitFaceByPolyline: path endpoints must lie on the face outer boundary  PDCEL.cpp:splitFaceByPolyline:1364

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Adaptive thickness plan: unavailable (base_count must be positive)

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `test/integration/t6_circle/circle_param_layup_range.log`
- Debug log: `test/integration/t6_circle/circle_param_layup_range.debug.log`
- DCEL dump: `test/integration/t6_circle/circle_param_layup_range.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
