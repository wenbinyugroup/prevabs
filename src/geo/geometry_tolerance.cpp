#include "geometry_tolerance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace prevabs {
namespace geo {

ModelGeometryTolerance computeModelGeometryTolerance(
    const std::vector<SPoint2>& points,
    const std::vector<double>& lamina_thicknesses,
    double coefficient) {
  if (!(coefficient > 0.0) || !std::isfinite(coefficient)) {
    throw std::invalid_argument(
        "geometry tolerance coefficient must be positive and finite");
  }

  double min_point = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      const double distance = points[i].distance(points[j]);
      if (distance > 0.0 && distance < min_point) {
        min_point = distance;
      }
    }
  }

  double min_lamina = std::numeric_limits<double>::infinity();
  for (const double thickness : lamina_thicknesses) {
    if (thickness > 0.0 && std::isfinite(thickness)
        && thickness < min_lamina) {
      min_lamina = thickness;
    }
  }

  ModelGeometryTolerance result;
  if (std::isfinite(min_point)) result.min_point_distance = min_point;
  if (std::isfinite(min_lamina)) result.min_lamina_thickness = min_lamina;

  if (std::isfinite(min_point) || std::isfinite(min_lamina)) {
    result.characteristic_length = std::min(min_point, min_lamina);
    result.tolerance = result.characteristic_length * coefficient;
  }
  return result;
}

}  // namespace geo
}  // namespace prevabs
