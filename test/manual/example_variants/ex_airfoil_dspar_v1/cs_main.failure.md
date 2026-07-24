# PreVABS build failure report

- Input: `.\cs_main.xml`
- Case: `cs_main`
- Time: 2026-07-24 16:06:00
- Failure: fatal exception: homogenization failed: layered offset[sgm_blk_1]: failed to split layer 3 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)
- Catch context: initialize model

## What failed

PreVABS stopped while building the cross-section. See the fatal message and nearby diagnostic log lines below.

## Diagnostic evidence

From `.\cs_main.log`:

- !!  offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- !!  offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.00 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.
- xx  fatal exception: homogenization failed: layered offset[sgm_blk_1]: failed to split layer 3 into cells after the shared DCEL was already mutated; aborting (a legacy fallback here would corrupt the cross-section mesh)

From `.\cs_main.debug.log`:

- [warning]   offset (multi-vertex): offset distance 0.0161834 is 8.34002x the shortest base segment length (0.00194045 at segment 33); offset construction may be numerically fragile — consider <normalize>1</normalize>, a larger <scale>, or a denser baseline  geo_diagnostics.cpp:checkOffsetDistanceVsShortestEdge:125
- [warning]   offset (multi-vertex, open): skin dropped over base indices [32..35] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [warning]   offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [warning]   offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.02 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  geo_diagnostics.cpp:warnLowOffsetMNRatio:195
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:239
- [info]      creating gmsh edges  PModelBuildGmsh.cpp:createGmshEdges:413
- [info]      writing gmsh .geo file: .\cs_main_debug_after_spar.geo_unrolled  PModelIOGmsh.cpp:writeGmshGeo:348
- [warning]   offset (multi-vertex, open): skin dropped over base indices [1..1] due to insufficient local thickness  offset.cpp:buildBaseOffsetMap:610
- [warning]   offset (multi-vertex, open): only 2 offset verts produced for 3 base verts (M/N=0.67). The layup half-thickness 0.00 likely exceeds the local base curvature radius along part of the curve — Clipper2 has merged base corners during inset. Downstream mesh recovery typically fails below M/N=0.7. Consider reducing layup thickness, splitting the layup over a denser baseline, or excluding the thin region from the layup.  geo_diagnostics.cpp:warnLowOffsetMNRatio:195
- [info]      creating gmsh vertices  PModelBuildGmsh.cpp:createGmshVertices:239

## Adaptive thickness suggestion

This is a dry-run suggestion only. The model was not modified.

- Suggested mode: `linear`
- Segment: `sgm_1`
- Adaptive thickness plan: unavailable (design_half_thickness must be positive)
- Offset/base vertex ratio: 2/3 = 0.67

Use this as the audit trail for the opt-in adaptive thickness repair. A stricter automatic value still requires logging the minimum local available half-thickness in the failed range.

## Related files

- Log: `.\cs_main.log`
- Debug log: `.\cs_main.debug.log`
- DCEL dump: `.\cs_main.dcel_dump.txt`

## User action

The build was stopped instead of silently dropping local faces or elements. Inspect the reported segment/offset region and adjust the cross-section geometry or layup so the local laminate faces remain meshable and connected.
