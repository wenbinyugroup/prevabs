#include "catch_amalgamated.hpp"

#include "adaptive_thickness.hpp"

#include <cmath>

namespace {

void requireNear(double actual, double expected) {
  REQUIRE(std::fabs(actual - expected) < 1e-12);
}

}  // namespace

TEST_CASE("adaptive thickness: middle repair range builds linear taper",
          "[adaptive_thickness]") {
  prevabs::geo::LinearAdaptiveThicknessInput input;
  input.segment_name = "sg_te";
  input.base_count = 18;
  input.repair_lo = 5;
  input.repair_hi = 14;
  input.design_half_thickness = 0.02;
  input.safety = 0.90;
  input.transition_base_count = 2;

  const auto plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);

  REQUIRE(plan.ok);
  CHECK(plan.range.segment_name == "sg_te");
  CHECK(plan.range.repair_lo == 5);
  CHECK(plan.range.repair_hi == 14);
  CHECK(plan.range.taper_lo == 3);
  CHECK(plan.range.taper_hi == 16);
  requireNear(plan.range.repaired_half_thickness, 0.018);
  REQUIRE(plan.stations.size() == 18);
  requireNear(plan.stations[2].half_thickness, 0.02);
  requireNear(plan.stations[3].half_thickness, 0.02);
  requireNear(plan.stations[4].half_thickness, 0.019);
  requireNear(plan.stations[5].half_thickness, 0.018);
  requireNear(plan.stations[14].half_thickness, 0.018);
  requireNear(plan.stations[15].half_thickness, 0.019);
  requireNear(plan.stations[16].half_thickness, 0.02);
  requireNear(plan.stations[17].half_thickness, 0.02);
}

TEST_CASE("adaptive thickness: taper clamps at open segment endpoints",
          "[adaptive_thickness]") {
  prevabs::geo::LinearAdaptiveThicknessInput input;
  input.base_count = 8;
  input.repair_lo = 0;
  input.repair_hi = 2;
  input.design_half_thickness = 0.04;
  input.safety = 0.50;
  input.transition_base_count = 3;

  const auto plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);

  REQUIRE(plan.ok);
  CHECK(plan.range.taper_lo == 0);
  CHECK(plan.range.taper_hi == 5);
  requireNear(plan.stations[0].half_thickness, 0.02);
  requireNear(plan.stations[1].half_thickness, 0.02);
  requireNear(plan.stations[2].half_thickness, 0.02);
  requireNear(plan.stations[3].half_thickness, 0.02666666666666667);
  requireNear(plan.stations[4].half_thickness, 0.03333333333333333);
  requireNear(plan.stations[5].half_thickness, 0.04);
}

TEST_CASE("adaptive thickness: repair padding expands before taper",
          "[adaptive_thickness]") {
  prevabs::geo::LinearAdaptiveThicknessInput input;
  input.base_count = 18;
  input.repair_lo = 5;
  input.repair_hi = 14;
  input.design_half_thickness = 0.02;
  input.safety = 0.50;
  input.repair_base_padding = 2;
  input.transition_base_count = 1;

  const auto plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);

  REQUIRE(plan.ok);
  CHECK(plan.range.repair_lo == 3);
  CHECK(plan.range.repair_hi == 16);
  CHECK(plan.range.taper_lo == 2);
  CHECK(plan.range.taper_hi == 17);
  requireNear(plan.stations[2].half_thickness, 0.02);
  requireNear(plan.stations[3].half_thickness, 0.01);
  requireNear(plan.stations[16].half_thickness, 0.01);
  requireNear(plan.stations[17].half_thickness, 0.02);
}

TEST_CASE("adaptive thickness: min half-thickness clamps repaired value",
          "[adaptive_thickness]") {
  prevabs::geo::LinearAdaptiveThicknessInput input;
  input.base_count = 5;
  input.repair_lo = 2;
  input.repair_hi = 2;
  input.design_half_thickness = 0.02;
  input.safety = 0.10;
  input.transition_base_count = 1;
  input.min_half_thickness = 0.008;
  input.arc_lengths = {0.0, 0.5, 1.2, 2.0, 3.5};

  const auto plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);

  REQUIRE(plan.ok);
  requireNear(plan.range.repaired_half_thickness, 0.008);
  requireNear(plan.stations[2].half_thickness, 0.008);
  requireNear(plan.stations[3].s, 2.0);
}

TEST_CASE("adaptive thickness: invalid inputs fail with an error",
          "[adaptive_thickness]") {
  prevabs::geo::LinearAdaptiveThicknessInput input;
  input.base_count = 4;
  input.repair_lo = 3;
  input.repair_hi = 2;
  input.design_half_thickness = 0.02;

  auto plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);
  CHECK_FALSE(plan.ok);
  CHECK_FALSE(plan.error.empty());

  input.repair_lo = 1;
  input.repair_hi = 2;
  input.safety = 1.2;
  plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);
  CHECK_FALSE(plan.ok);

  input.safety = 0.9;
  input.repair_base_padding = -1;
  plan = prevabs::geo::buildLinearAdaptiveThicknessPlan(input);
  CHECK_FALSE(plan.ok);
}
