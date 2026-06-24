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

// True when PREVABS_SEGMENT_DUMP is set non-empty (cached).
bool segmentDumpEnabled();

// Build-path trace. buildAreas records the conditional branch actually taken
// at each decision point (layered vs legacy, fallback reasons, single-layer /
// closed / vertex-mismatch / ambiguous-split, ...) so each case gets a clean
// "which path did this segment take" log without the debug-log noise.
//
// The trace is written incrementally to <file_base>.<segment>.segment.path.txt
// so it survives a crash mid-build (the dirty-fallback paths are exactly where
// the known heap-corruption bug strikes — the end-of-buildAreas dump would
// never be reached there). All are cheap no-ops when the dump is disabled.
//   - segmentTraceBegin(seg) : top of buildAreas; truncates the path file.
//   - segmentTracePush(s)     : append one step (also flushed to the file).
//   - segmentTraceSteps()     : read back (consumed by dumpSegmentBuild JSON).
void segmentTraceBegin(const std::string &segname);
void segmentTracePush(const std::string &step);
const std::vector<std::string> &segmentTraceSteps();

// Write the SVG + JSON + path.txt for `seg`. No-op when the dump is disabled
// or the segment has no resolvable faces.
void dumpSegmentBuild(Segment &seg, const BuilderConfig &bcfg);

}  // namespace debug
}  // namespace prevabs
