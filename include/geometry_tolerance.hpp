#pragma once

#include "geo_types.hpp"

#include <vector>

namespace prevabs {
namespace geo {

struct ModelGeometryTolerance {
  double min_point_distance = 0.0;
  double min_lamina_thickness = 0.0;
  double characteristic_length = 0.0;
  double tolerance = 0.0;

  bool valid() const { return tolerance > 0.0; }
};

/// Compute a model-scale absolute geometry tolerance. `points` must contain
/// post-transform coordinates, so the XML scale factor is already applied.
/// Exact duplicate coordinates and non-positive lamina thicknesses do not
/// define a feature size. The tolerance is:
///   coefficient * min(min positive point distance,
///                     min positive lamina thickness).
ModelGeometryTolerance computeModelGeometryTolerance(
    const std::vector<SPoint2>& points,
    const std::vector<double>& lamina_thicknesses,
    double coefficient = 1e-3);

}  // namespace geo
}  // namespace prevabs
