#pragma once

#include <string>
#include <vector>

namespace prevabs {
namespace geo {

struct AdaptiveThicknessRange {
  std::string segment_name;
  int repair_lo = -1;
  int repair_hi = -1;
  int taper_lo = -1;
  int taper_hi = -1;
  double design_half_thickness = 0.0;
  double repaired_half_thickness = 0.0;
  double safety = 0.0;
};

struct ThicknessStation {
  int base_index = -1;
  double s = 0.0;
  double half_thickness = 0.0;
};

struct LinearAdaptiveThicknessInput {
  std::string segment_name;
  int base_count = 0;
  int repair_lo = -1;
  int repair_hi = -1;
  double design_half_thickness = 0.0;
  double safety = 0.90;
  int repair_base_padding = 0;
  int transition_base_count = 2;
  double min_half_thickness = 0.0;
  std::vector<double> arc_lengths;
};

struct LinearAdaptiveThicknessPlan {
  bool ok = false;
  std::string error;
  AdaptiveThicknessRange range;
  std::vector<ThicknessStation> stations;
};

LinearAdaptiveThicknessPlan buildLinearAdaptiveThicknessPlan(
    const LinearAdaptiveThicknessInput &input);

}  // namespace geo
}  // namespace prevabs
