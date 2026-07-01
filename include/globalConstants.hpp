#pragma once

#include <limits>
#include <string>
#include <vector>

// #include "version.h"

// #define VERSION_MAJOR "1"
// #define VERSION_MINOR "6"
// #define VERSION_PATCH "0"

// const std::string VERSION_STRING{
//   VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH
// };

const std::string vabs_version = "4.1";
const std::string sc_version = "2.1";

const double INF{std::numeric_limits<double>::infinity()};
const double PI{3.141592653589793};

// ===========================================================================
// Tolerance architecture (see dev-docs/geometry-tolerances.md)
//
//   C — Dimensionless RELATIVE tolerances (this block). The fundamental,
//       user-configurable layer: "how many parts of the characteristic
//       length". A small ladder of rungs.
//   A/B — Absolute LENGTH tolerances, DERIVED as  L_char * rel  (next block).
//   D — A single machine-precision guard (GEO_EPS_MACHINE), conceptually
//       distinct from geometric tolerance, never scaled by L_char.
//
// L_char = min(min positive point spacing, min positive lamina thickness),
// computed from the post-transform model by the cross-section reader.
// ===========================================================================

// C: dimensionless relative ladder (ascending rungs spanning ~1e-3..1e-1 of
// L_char). Defaults are chosen so the B noise floors govern at test-suite
// scales (zero change there) while the relative terms scale up large models.
extern double REL_PREDICATE;     // 1e-3  strict predicates; u-endpoint tests
extern double REL_MERGE_FINE;    // 2e-3  coincidence / nesting
extern double REL_EXPORT_MERGE;  // 4e-3  export-time vertex merge
extern double REL_RESAMPLE;      // 2e-2  resample minimum separation
extern double REL_MERGE_COARSE;  // 1e-1  area-build vertex merge

// C (parameter-space): dimensionless thresholds compared DIRECTLY against a
// normalized curve/ray parameter (u, t in [0,1]); NOT multiplied by L_char.
// They guard parameter-space FP noise — "is the parameter essentially at an
// endpoint / corner?" — so they stay tight and fixed (not feature-scale).
extern double PARAM_ENDPOINT_SLACK;  // 1e-12  ray_t / segment_u endpoint slack
extern double PARAM_CORNER_EPS;      // 1e-9   projection-foot at-corner test

// D: machine-precision guard. Rejects determinants / denominators that are
// numerically indistinguishable from zero (det/len of O(1) quantities, ~45x
// DBL_EPSILON). NOT a geometric tolerance; never scaled by L_char.
extern double GEO_EPS_MACHINE;   // 1e-14

// A: runtime model-scale length tolerances, derived from the C ladder. The
// input reader updates them after geometry and lamina definitions are
// available, before component geometry is built:
//   GEO_TOL       = L_char * REL_PREDICATE     strict length-predicate
//                   tolerance (same-point / on-segment / vertex-coincidence).
//   GEO_MERGE_TOL = L_char * REL_MERGE_COARSE  loose de-duplication tolerance
//                   for two independently-computed copies of the same point
//                   (Clipper2 grid noise, divergent parametric interpolations);
//                   100x looser than GEO_TOL, yet far below any real feature.
// TOLERANCE, ABS_TOL, and REL_TOL are references kept for existing call sites.
// Note: several call sites compare a dimensionless segment parameter u in [0,1]
// against TOLERANCE (e.g. `fabs(u) < TOLERANCE` to detect an intersection at a
// segment endpoint). This is intentional: for a characteristic-scale segment,
// u * length ~ GEO_TOL corresponds to a physical endpoint-proximity of ~GEO_TOL,
// so reusing the scale-tied length tolerance keeps the endpoint test tied to the
// model scale. A fixed dimensionless value would NOT scale with the model.
extern double GEO_TOL;
extern double GEO_MERGE_TOL;
extern double& TOLERANCE;
extern double& ABS_TOL;
extern double& REL_TOL;

void setGeometryTolerance(double tolerance);

// ---------------------------------------------------------------------------
// Merge / resample / coincidence length ladder.
//
// These tolerances fold or separate vertices during the layered-offset build
// and at mesh export. They form an ordered ladder that MUST be preserved:
//
//   Clipper2 grid (~1e-8) < GEO_COINCIDENCE_TOL < GEO_EXPORT_MERGE_TOL
//                         < GEO_RESAMPLE_MIN_SEP < smallest real feature
//
// Each rung is  max(absolute noise floor, L_char * rel)  — see
// setGeometryTolerance. The floor (Clipper2 precision-8 grid ~1e-8 and FP
// interpolation gaps ~1e-6 are ABSOLUTE artifacts, independent of L_char)
// governs at the airfoil-scale cross-sections in the test suite, so behaviour
// there is unchanged; the relative term scales the ladder up for large-feature
// models. See local/issue-20260630-geo-tolerance-consolidation.md.
// ---------------------------------------------------------------------------

// Fold two consecutive layered-offset staircase steps whose offset points are
// within this distance (a coincident "zero-length" vertical step). Also used
// as the coincident-centroid threshold in the post-split degeneracy self-check.
// = max(1e-6 floor, L_char * REL_MERGE_FINE).
extern double GEO_COINCIDENCE_TOL;

// Export-time DCEL->Gmsh vertex sharing: distinct DCEL vertices closer than
// this collapse onto one Gmsh point, avoiding zero-area (degenerate) elements.
// = max(2e-6 floor, L_char * REL_EXPORT_MERGE).
extern double GEO_EXPORT_MERGE_TOL;

// Floor on the strictly-increasing minimum arc spacing between consecutive
// resampled outer (shell) vertices, keeping the layered staircase 1:1. The
// scaled term (L_sh * 1e-5) may exceed this floor.
// = max(1e-5 floor, L_char * REL_RESAMPLE).
extern double GEO_RESAMPLE_MIN_SEP;

// Slack on layered-offset nesting / zero-gap validation distance comparisons.
// = max(1e-6 floor, L_char * REL_MERGE_FINE).
extern double GEO_NESTING_SLACK;

// Log severity levels (match spdlog mapping in plog.cpp)
constexpr int LOG_LEVEL_TRACE   = 0;
constexpr int LOG_LEVEL_DEBUG   = 1;
constexpr int LOG_LEVEL_INFO    = 2;
constexpr int LOG_LEVEL_WARNING = 3;
constexpr int LOG_LEVEL_ERROR   = 4;
constexpr int LOG_LEVEL_FATAL   = 5;

// Analysis tool selection
enum class AnalysisTool {
  VABS      = 1,
  SwiftComp = 2
};

// Analysis mode — mutually exclusive, maps to the CLI mode flags
enum class AnalysisMode {
  Homogenization,
  Dehomogenization,
  DehomogenizationNL,   // reserved, not yet exposed in CLI
  FailureStrength,      // SwiftComp only
  FailureEnvelope,      // SwiftComp only
  FailureIndex
};

// Helper: true when the mode is a dehomogenization or failure-type analysis
inline bool isRecoveryMode(AnalysisMode m) {
  return m != AnalysisMode::Homogenization;
}

// Helper: true when the mode involves failure analysis
inline bool isFailureMode(AnalysisMode m) {
  return m == AnalysisMode::FailureStrength ||
         m == AnalysisMode::FailureEnvelope  ||
         m == AnalysisMode::FailureIndex;
}

enum class SnapshotMode {
  never,
  phase,
  component,
  all
};

/// Debug verbosity levels for --debug.
/// Each level is a superset of the one above it.
enum class DebugLevel {
  off,      ///< No debug output (default)
  summary,  ///< Summaries + warning + error (info-level, same as off in practice)
  phase,    ///< + phase entry/exit diagnostics
  join,     ///< + join/segment algorithm details (current --debug default)
  geo,      ///< + DCEL geometry operations in offset.cpp (very verbose)
  all       ///< Alias for geo
};

enum GeoConst {
  DEGREE,
  RADIAN,
  CLOCKWISE,
  COUNTER_CLOCKWISE,
  LEFT_TURN,
  RIGHT_TURN
};

// enum CrossSectionType {
//   GENERAL,
//   AIRFOIL
// };

enum MaterialType {
  ISOTROPIC,
  ENGINEERING_CONSTANTS,
  ORTHOTROPIC,
  ANISOTROPIC
};

const std::string singleLine(32, '-');
const std::string doubleLine(32, '=');
const std::string singleLine80(80, '-');
const std::string doubleLine80(80, '=');
const std::string markInfo(2, '>');
const std::string markWarning(2, '!');
const std::string markError(2, 'X');

const std::vector<std::string> elasticLabelIso{"e", "nu"};
const std::vector<std::string> elasticLabelOrtho{
    "e1", "e2", "e3", "g12", "g13", "g23", "nu12", "nu13", "nu23"};
const std::vector<std::string> elasticLabelAniso{
    "c11", "c12", "c13", "c14", "c15", "c16", "c22", "c23", "c24", "c25", "c26",
    "c33", "c34", "c35", "c36", "c44", "c45", "c46", "c55", "c56", "c66"};

const std::vector<std::string> TAG_NAME_CTE_ORTHO{"a11", "a22", "a33"};
