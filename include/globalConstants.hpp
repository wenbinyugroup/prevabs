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

const std::string vabs_version = "4.0";
const std::string sc_version = "2.1";

const double INF{std::numeric_limits<double>::infinity()};
const double PI{3.141592653589793};
// GEO_TOL is the single source of truth for geometric comparisons.
// Its default (1e-9) matches AppConfig::geo_tol so the compile-time fallback
// stays in sync with the user-tunable runtime value.
// TOLERANCE, ABS_TOL, REL_TOL are kept as aliases for existing call sites;
// prefer GEO_TOL in new code.
const double GEO_TOL{1e-9};
const double TOLERANCE{GEO_TOL};
const double ABS_TOL{GEO_TOL};
const double REL_TOL{GEO_TOL};

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
