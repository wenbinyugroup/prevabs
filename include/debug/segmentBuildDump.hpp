#pragma once

// segmentBuildDump — inspection dump for the "build a single laminate
// segment" core step. After Segment::buildAreas finishes, this writes a
// per-segment SVG + JSON describing the final tiled layer faces together
// with geometric-quality metrics (signed area, minimum interior angle,
// inverted / sliver / degenerate flags) that the end-to-end integration
// pass/fail signal does not expose.
//
// Enabled only when the env var PREVABS_SEGMENT_DUMP is set (to anything
// non-empty); otherwise dumpSegmentBuild is a no-op. Output goes next to
// the other run artifacts:
//   <file_directory><file_base_name>.<segment>.segment.svg
//   <file_directory><file_base_name>.<segment>.segment.json
//
// The gmsh mesh (.msh) and unrolled geometry are already written by the
// main pipeline; the test runner keeps those for visualization.

#include <string>
#include <vector>

class Segment;
struct BuilderConfig;

namespace prevabs {
namespace debug {

// True when PREVABS_SEGMENT_DUMP is set non-empty (cached). Gates the heavy
// per-segment SVG + JSON quality dump.
bool segmentDumpEnabled();

// True when either PREVABS_SEGMENT_DUMP or PREVABS_SEGMENT_PATH is set
// non-empty (cached). Gates the cheap build-path trace alone, so the
// integration suite can record each segment's branch trajectory
// (<case>.<segment>.segment.path.txt) without paying for the SVG/JSON dump.
bool segmentTraceEnabled();

// Build-path trace. Records the call stack a segment walks while its geometry
// is built — across BOTH lifecycle phases: the offset phase (offsetCurveBase ->
// geo::buildBaseOffsetMap -> geo::buildBaseOffsetMapFromOffsetPolygons) and the
// area phase (buildAreas -> buildLayeredOffsetAreas -> split*/offset helpers) —
// together with the conditional branch actually taken at each decision point
// (layered vs legacy, fallback reasons, single-layer / closed / vertex-mismatch
// / ambiguous-split, ...). This gives each case a clean "which path did this
// segment take" log without the debug-log noise.
//
// State is keyed by segment name, so the two phases (which run in separate
// all-segments passes — see PBuildComponentLaminate / PComponent) append to the
// SAME per-segment file <file_base>.<segment>.segment.path.txt even though they
// are not adjacent. Each step is indented by call-stack depth so the file reads
// as a call tree. Writes are incremental so the trace survives a crash mid-build
// (the dirty-fallback paths are exactly where the known heap-corruption bug
// strikes). All are cheap no-ops when the trace is disabled (segmentTraceEnabled).
//   - segmentTraceBegin(seg) : attach the active segment + reset depth; truncates
//                              the path file once per segment per process.
//   - segmentTracePush(s)     : append one step at the current depth.
//   - segmentTraceSteps()     : read back the active segment's steps (JSON dump).
void segmentTraceBegin(const std::string &segname);
void segmentTracePush(const std::string &step);
const std::vector<std::string> &segmentTraceSteps();

// RAII call-stack frame: pushes `label` at the current depth, then descends one
// level for the lifetime of the object (steps and nested scopes inside print one
// indent deeper). Use at the top of each instrumented function:
//     prevabs::debug::SegmentTraceScope _t("buildLayeredOffsetAreas");
// Cheap no-op (no depth change, no allocation) when the trace is disabled.
class SegmentTraceScope {
 public:
  explicit SegmentTraceScope(const std::string &label);
  ~SegmentTraceScope();
  SegmentTraceScope(const SegmentTraceScope &) = delete;
  SegmentTraceScope &operator=(const SegmentTraceScope &) = delete;

 private:
  bool active_;
};

// Write the SVG + JSON + path.txt for `seg`. No-op when the dump is disabled
// or the segment has no resolvable faces.
void dumpSegmentBuild(Segment &seg, const BuilderConfig &bcfg);

}  // namespace debug
}  // namespace prevabs
