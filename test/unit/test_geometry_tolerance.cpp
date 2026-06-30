#include "catch_amalgamated.hpp"

#include "geometry_tolerance.hpp"
#include "globalConstants.hpp"

using prevabs::geo::computeModelGeometryTolerance;

TEST_CASE("model geometry tolerance uses the smallest physical feature",
          "[geo][tolerance]") {
  const std::vector<SPoint2> points = {
      SPoint2(0.0, 0.0), SPoint2(3.0, 4.0), SPoint2(10.0, 0.0),
  };
  const std::vector<double> thicknesses = {2.0, 0.25};

  const auto result = computeModelGeometryTolerance(points, thicknesses);

  CHECK(result.min_point_distance == Catch::Approx(5.0));
  CHECK(result.min_lamina_thickness == Catch::Approx(0.25));
  CHECK(result.characteristic_length == Catch::Approx(0.25));
  CHECK(result.tolerance == Catch::Approx(2.5e-4));
}

TEST_CASE("model geometry tolerance sees scaled imported coordinates",
          "[geo][tolerance]") {
  // The caller supplies post-transform coordinates, so the XML scale factor
  // is already reflected in this 0.2 spacing.
  const std::vector<SPoint2> points = {
      SPoint2(0.0, 0.0), SPoint2(0.2, 0.0), SPoint2(0.6, 0.0),
  };

  const auto result = computeModelGeometryTolerance(points, {1.0});

  CHECK(result.min_point_distance == Catch::Approx(0.2));
  CHECK(result.characteristic_length == Catch::Approx(0.2));
  CHECK(result.tolerance == Catch::Approx(2e-4));
}

TEST_CASE("model geometry tolerance ignores exact duplicate coordinates",
          "[geo][tolerance]") {
  const std::vector<SPoint2> points = {
      SPoint2(0.0, 0.0), SPoint2(0.0, 0.0), SPoint2(0.5, 0.0),
  };

  const auto result = computeModelGeometryTolerance(points, {});

  CHECK(result.min_point_distance == Catch::Approx(0.5));
  CHECK(result.tolerance == Catch::Approx(5e-4));
}

TEST_CASE("runtime geometry tolerance updates all legacy aliases",
          "[geo][tolerance]") {
  const double original = GEO_TOL;

  setGeometryTolerance(2.5e-7);
  CHECK(GEO_TOL == Catch::Approx(2.5e-7));
  CHECK(TOLERANCE == Catch::Approx(GEO_TOL));
  CHECK(ABS_TOL == Catch::Approx(GEO_TOL));
  CHECK(REL_TOL == Catch::Approx(GEO_TOL));

  setGeometryTolerance(original);
}
