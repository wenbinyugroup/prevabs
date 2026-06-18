#include "catch_amalgamated.hpp"

#include "variable_offset.hpp"

#include <cmath>

namespace {

void requireNear(double actual, double expected) {
  REQUIRE(std::fabs(actual - expected) < 1e-12);
}

void requirePointNear(const SPoint2 &actual, double x, double y) {
  requireNear(actual.x(), x);
  requireNear(actual.y(), y);
}

}  // namespace

TEST_CASE("variable offset: straight line with constant thickness",
          "[variable_offset]") {
  prevabs::geo::VariableOffsetInput input;
  input.base = {SPoint2(0.0, 0.0), SPoint2(2.0, 0.0)};
  input.half_thickness_by_base = {0.5, 0.5};
  input.side = 1;

  const auto result = prevabs::geo::offsetVariableDistance(input);

  REQUIRE(result.ok);
  REQUIRE(result.offset.size() == 2);
  requirePointNear(result.offset[0], 0.0, 0.5);
  requirePointNear(result.offset[1], 2.0, 0.5);
  REQUIRE(result.id_pairs.size() == 2);
  CHECK(result.id_pairs[0].base == 0);
  CHECK(result.id_pairs[0].offset == 0);
  CHECK(result.id_pairs[1].base == 1);
  CHECK(result.id_pairs[1].offset == 1);
}

TEST_CASE("variable offset: straight line with variable thickness",
          "[variable_offset]") {
  prevabs::geo::VariableOffsetInput input;
  input.base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(2.0, 0.0)};
  input.half_thickness_by_base = {0.2, 0.5, 0.8};
  input.side = 1;

  const auto result = prevabs::geo::offsetVariableDistance(input);

  REQUIRE(result.ok);
  REQUIRE(result.offset.size() == 3);
  requirePointNear(result.offset[0], 0.0, 0.2);
  requirePointNear(result.offset[1], 1.0, 0.5);
  requirePointNear(result.offset[2], 2.0, 0.8);
}

TEST_CASE("variable offset: L shape uses adjacent offset-line intersection",
          "[variable_offset]") {
  prevabs::geo::VariableOffsetInput input;
  input.base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(1.0, 1.0)};
  input.half_thickness_by_base = {0.1, 0.1, 0.1};
  input.side = 1;

  const auto result = prevabs::geo::offsetVariableDistance(input);

  REQUIRE(result.ok);
  REQUIRE(result.offset.size() == 3);
  requirePointNear(result.offset[0], 0.0, 0.1);
  requirePointNear(result.offset[1], 0.9, 0.1);
  requirePointNear(result.offset[2], 0.9, 1.0);
}

TEST_CASE("variable offset: sharp cusp caps far miter intersection",
          "[variable_offset]") {
  prevabs::geo::VariableOffsetInput input;
  input.base = {
      SPoint2(0.0, 0.0), SPoint2(1.0, 0.0), SPoint2(0.1, 0.05)};
  input.half_thickness_by_base = {0.1, 0.1, 0.1};
  input.side = -1;
  input.miter_limit = 2.0;

  const auto result = prevabs::geo::offsetVariableDistance(input);

  REQUIRE(result.ok);
  REQUIRE(result.offset.size() == 3);
  const double dx = result.offset[1].x() - input.base[1].x();
  const double dy = result.offset[1].y() - input.base[1].y();
  CHECK(std::sqrt(dx * dx + dy * dy) <= 0.2 + 1e-12);
}

TEST_CASE("variable offset: invalid input fails", "[variable_offset]") {
  prevabs::geo::VariableOffsetInput input;
  input.base = {SPoint2(0.0, 0.0), SPoint2(0.0, 0.0)};
  input.half_thickness_by_base = {0.1, 0.1};
  input.side = 1;

  auto result = prevabs::geo::offsetVariableDistance(input);
  CHECK_FALSE(result.ok);
  CHECK_FALSE(result.error.empty());

  input.base = {SPoint2(0.0, 0.0), SPoint2(1.0, 0.0)};
  input.half_thickness_by_base = {0.1};
  result = prevabs::geo::offsetVariableDistance(input);
  CHECK_FALSE(result.ok);
}
