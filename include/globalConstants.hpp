#pragma once

#include <limits>
#include <string>
#include <vector>

#define VERSION_MAJOR "1"
#define VERSION_MINOR "6"
#define VERSION_PATCH "0"

const std::string version{
  VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH
};

const std::string vabs_version = "4.0";
const std::string sc_version = "2.1";

const double INF{std::numeric_limits<double>::infinity()};
const double PI{3.14159265};
const double TOLERANCE{1e-12};

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
