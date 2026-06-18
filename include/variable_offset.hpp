#pragma once

#include "geo_types.hpp"

#include <string>
#include <vector>

namespace prevabs {
namespace geo {

struct VariableOffsetInput {
  std::vector<SPoint2> base;
  std::vector<double> half_thickness_by_base;
  int side = 1;
  double tol = 1e-12;
  double miter_limit = 2.0;
};

struct VariableOffsetResult {
  bool ok = false;
  std::string error;
  std::vector<SPoint2> offset;
  BaseOffsetMap id_pairs;
};

VariableOffsetResult offsetVariableDistance(
    const VariableOffsetInput &input);

}  // namespace geo
}  // namespace prevabs
