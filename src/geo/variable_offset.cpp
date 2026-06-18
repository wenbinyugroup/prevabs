#include "variable_offset.hpp"

#include <algorithm>
#include <cmath>

namespace prevabs {
namespace geo {

namespace {

struct Vec2 {
  double x = 0.0;
  double y = 0.0;
};

Vec2 diff(const SPoint2 &a, const SPoint2 &b) {
  return Vec2{a.x() - b.x(), a.y() - b.y()};
}

SPoint2 operator+(const SPoint2 &p, const Vec2 &v) {
  return SPoint2(p.x() + v.x, p.y() + v.y);
}

Vec2 operator+(const Vec2 &a, const Vec2 &b) {
  return Vec2{a.x + b.x, a.y + b.y};
}

Vec2 operator*(const Vec2 &v, double s) {
  return Vec2{v.x * s, v.y * s};
}

double cross(const Vec2 &a, const Vec2 &b) {
  return a.x * b.y - a.y * b.x;
}

double norm(const Vec2 &v) {
  return std::sqrt(v.x * v.x + v.y * v.y);
}

Vec2 normalForSegment(const SPoint2 &a, const SPoint2 &b, int side) {
  const Vec2 t = diff(b, a);
  const double len = norm(t);
  return Vec2{-static_cast<double>(side) * t.y / len,
              static_cast<double>(side) * t.x / len};
}

bool lineIntersection(
    const SPoint2 &a0,
    const SPoint2 &a1,
    const SPoint2 &b0,
    const SPoint2 &b1,
    double tol,
    SPoint2 *out) {
  const Vec2 r = diff(a1, a0);
  const Vec2 s = diff(b1, b0);
  const double denom = cross(r, s);
  if (std::fabs(denom) <= tol) return false;

  const Vec2 qmp = diff(b0, a0);
  const double u = cross(qmp, s) / denom;
  *out = a0 + r * u;
  return true;
}

bool finitePositive(double value) {
  return std::isfinite(value) && value > 0.0;
}

SPoint2 averageCandidateAtVertex(
    const SPoint2 &base,
    const SPoint2 &prev_offset,
    const SPoint2 &next_offset) {
  const Vec2 avg =
      (diff(prev_offset, base) + diff(next_offset, base)) * 0.5;
  return base + avg;
}

VariableOffsetResult failResult(const std::string &error) {
  VariableOffsetResult result;
  result.ok = false;
  result.error = error;
  return result;
}

}  // namespace

VariableOffsetResult offsetVariableDistance(
    const VariableOffsetInput &input) {
  const int n = static_cast<int>(input.base.size());
  if (n < 2) {
    return failResult("base must contain at least two points");
  }
  if (input.side != 1 && input.side != -1) {
    return failResult("side must be +1 or -1");
  }
  if (static_cast<int>(input.half_thickness_by_base.size()) != n) {
    return failResult("half_thickness_by_base size must match base size");
  }
  if (!finitePositive(input.tol)) {
    return failResult("tol must be positive");
  }
  if (!finitePositive(input.miter_limit)) {
    return failResult("miter_limit must be positive");
  }
  for (int i = 0; i < n; ++i) {
    if (!finitePositive(input.half_thickness_by_base[i])) {
      return failResult("all half-thickness values must be positive");
    }
  }

  std::vector<Vec2> normals;
  normals.reserve(n - 1);
  for (int i = 0; i + 1 < n; ++i) {
    const Vec2 edge = diff(input.base[i + 1], input.base[i]);
    if (norm(edge) <= input.tol) {
      return failResult("base contains a degenerate segment");
    }
    normals.push_back(
        normalForSegment(input.base[i], input.base[i + 1], input.side));
  }

  std::vector<SPoint2> left;
  std::vector<SPoint2> right;
  left.reserve(n - 1);
  right.reserve(n - 1);
  for (int i = 0; i + 1 < n; ++i) {
    left.push_back(
        input.base[i] + normals[i] * input.half_thickness_by_base[i]);
    right.push_back(
        input.base[i + 1]
        + normals[i] * input.half_thickness_by_base[i + 1]);
  }

  VariableOffsetResult result;
  result.ok = true;
  result.offset.resize(n);
  result.offset[0] = left.front();
  result.offset[n - 1] = right.back();

  for (int i = 1; i + 1 < n; ++i) {
    SPoint2 p;
    if (lineIntersection(
            left[i - 1], right[i - 1], left[i], right[i], input.tol, &p)) {
      const double max_miter =
          input.miter_limit * input.half_thickness_by_base[i];
      if (norm(diff(p, input.base[i])) <= max_miter + input.tol) {
        result.offset[i] = p;
      } else {
        result.offset[i] = averageCandidateAtVertex(
            input.base[i], right[i - 1], left[i]);
      }
    } else {
      result.offset[i] = averageCandidateAtVertex(
          input.base[i], right[i - 1], left[i]);
    }
  }

  result.id_pairs.reserve(n);
  for (int i = 0; i < n; ++i) {
    result.id_pairs.push_back(BaseOffsetPair(i, i));
  }
  return result;
}

}  // namespace geo
}  // namespace prevabs
