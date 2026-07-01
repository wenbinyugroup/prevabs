#include "adaptive_thickness.hpp"

#include <algorithm>
#include <cmath>

namespace prevabs {
namespace geo {

namespace {

bool finitePositive(double value) {
  return std::isfinite(value) && value > 0.0;
}

bool finiteNonNegative(double value) {
  return std::isfinite(value) && value >= 0.0;
}

double stationCoordinate(
    const LinearAdaptiveThicknessInput &input,
    int base_index) {
  if (!input.arc_lengths.empty()) {
    return input.arc_lengths[base_index];
  }
  return static_cast<double>(base_index);
}

double lerp(double a, double b, double t) {
  return a + (b - a) * t;
}

double thicknessAtBaseIndex(
    const AdaptiveThicknessRange &range,
    int base_index) {
  const double t_design = range.design_half_thickness;
  const double t_repair = range.repaired_half_thickness;

  if (base_index < range.taper_lo || base_index > range.taper_hi) {
    return t_design;
  }
  if (base_index >= range.repair_lo && base_index <= range.repair_hi) {
    return t_repair;
  }
  if (base_index < range.repair_lo) {
    const int span = range.repair_lo - range.taper_lo;
    if (span <= 0) return t_repair;
    const double u = static_cast<double>(base_index - range.taper_lo)
                   / static_cast<double>(span);
    return lerp(t_design, t_repair, u);
  }

  const int span = range.taper_hi - range.repair_hi;
  if (span <= 0) return t_repair;
  const double u = static_cast<double>(base_index - range.repair_hi)
                 / static_cast<double>(span);
  return lerp(t_repair, t_design, u);
}

LinearAdaptiveThicknessPlan failPlan(const std::string &error) {
  LinearAdaptiveThicknessPlan plan;
  plan.ok = false;
  plan.error = error;
  return plan;
}

}  // namespace

LinearAdaptiveThicknessPlan buildLinearAdaptiveThicknessPlan(
    const LinearAdaptiveThicknessInput &input) {
  if (input.base_count <= 0) {
    return failPlan("base_count must be positive");
  }
  if (input.repair_lo < 0 || input.repair_hi < input.repair_lo
      || input.repair_hi >= input.base_count) {
    return failPlan("repair range must be inside base indices");
  }
  if (!finitePositive(input.design_half_thickness)) {
    return failPlan("design_half_thickness must be positive");
  }
  if (!finitePositive(input.safety) || input.safety > 1.0) {
    return failPlan("safety must be in (0, 1]");
  }
  if (input.repair_base_padding < 0) {
    return failPlan("repair_base_padding must be non-negative");
  }
  if (input.transition_base_count < 0) {
    return failPlan("transition_base_count must be non-negative");
  }
  if (!finiteNonNegative(input.min_half_thickness)) {
    return failPlan("min_half_thickness must be non-negative");
  }
  if (input.min_half_thickness > input.design_half_thickness) {
    return failPlan(
        "min_half_thickness must not exceed design_half_thickness");
  }
  if (!input.arc_lengths.empty()) {
    if (static_cast<int>(input.arc_lengths.size()) != input.base_count) {
      return failPlan("arc_lengths size must match base_count");
    }
    for (int i = 0; i < input.base_count; ++i) {
      if (!std::isfinite(input.arc_lengths[i])) {
        return failPlan("arc_lengths must be finite");
      }
      if (i > 0 && input.arc_lengths[i] < input.arc_lengths[i - 1]) {
        return failPlan("arc_lengths must be non-decreasing");
      }
    }
  }

  LinearAdaptiveThicknessPlan plan;
  plan.ok = true;
  plan.range.segment_name = input.segment_name;
  plan.range.repair_lo = std::max(
      0, input.repair_lo - input.repair_base_padding);
  plan.range.repair_hi = std::min(
      input.base_count - 1, input.repair_hi + input.repair_base_padding);
  plan.range.taper_lo = std::max(
      0, plan.range.repair_lo - input.transition_base_count);
  plan.range.taper_hi = std::min(
      input.base_count - 1,
      plan.range.repair_hi + input.transition_base_count);
  plan.range.design_half_thickness = input.design_half_thickness;
  plan.range.repaired_half_thickness = std::max(
      input.min_half_thickness,
      input.design_half_thickness * input.safety);
  plan.range.safety = input.safety;

  plan.stations.reserve(input.base_count);
  for (int i = 0; i < input.base_count; ++i) {
    ThicknessStation station;
    station.base_index = i;
    station.s = stationCoordinate(input, i);
    station.half_thickness = thicknessAtBaseIndex(plan.range, i);
    plan.stations.push_back(station);
  }

  return plan;
}

}  // namespace geo
}  // namespace prevabs
