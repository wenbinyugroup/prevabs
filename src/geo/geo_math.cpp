#include "geo_math.hpp"

#include "globalConstants.hpp"
#include "globalVariables.hpp"

#include <algorithm>
#include <cstdlib>

bool isClose(
  const double& p1x, const double& p1y,
  const double& p2x, const double& p2y,
  double absTol, double relTol) {
    auto isCloseValue = [absTol, relTol](double a, double b) -> bool {
        double diff = std::abs(a - b);
        double maxAbs = std::max(std::abs(a), std::abs(b));
        return diff <= std::max(absTol, relTol * maxAbs);
    };

    return isCloseValue(p1x, p2x) && isCloseValue(p1y, p2y);
}

int getTurningSide(SVector3 vec1, SVector3 vec2) {
  SVector3 n;
  n = crossprod(vec1, vec2);

  if (fabs(n[0]) < TOLERANCE) {
    return 0;
  } else if (n[0] > 0) {
    return 1;
  } else {
    return -1;
  }
}

double calcDistanceSquared(PDCELVertex *v1, PDCELVertex *v2) {
  double dx = v1->x() - v2->x();
  double dy = v1->y() - v2->y();
  double dz = v1->z() - v2->z();

  return dx * dx + dy * dy + dz * dz;
}

SVector3 getVectorFromAngle(double angle, AnglePlane plane) {
  SVector3 v;

  while (angle <= -90 || angle > 270) {
    if (angle <= -90) {
      angle += 360;
    } else if (angle > 270) {
      angle -= 360;
    }
  }

  double reverse = 1.0;
  if (angle > 90) {
    reverse = -1.0;
  }

  double d1, d2;
  if (angle == 90.0 || angle == 270.0) {
    d1 = 0.0;
    d2 = 1.0;
  } else {
    d1 = 1.0;
    d2 = tan(deg2rad(angle));
  }

  if (plane == AnglePlane::YZ) {
    v = SVector3(0, d1, d2);
  } else if (plane == AnglePlane::ZX) {
    v = SVector3(d2, 0, d1);
  } else {
    v = SVector3(d1, d2, 0);
  }

  return v * reverse;
}

SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u) {
  if (fabs(u) < TOLERANCE) {
    return SPoint3(p1);
  } else if (fabs(u - 1) < TOLERANCE) {
    return SPoint3(p2);
  } else {
    double x, y, z;

    x = p1.x() + u * (p2.x() - p1.x());
    y = p1.y() + u * (p2.y() - p1.y());
    z = p1.z() + u * (p2.z() - p1.z());

    return SPoint3(x, y, z);
  }
}

bool isParallel(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  SVector3 vec1, vec2, vecn;
  vec1 = ls1->toVector();
  vec2 = ls2->toVector();

  vecn = crossprod(vec1, vec2);
  if (vecn.normSq() < TOLERANCE * TOLERANCE) {
    return true;
  } else {
    return false;
  }
}

bool isCollinear(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  if (!isParallel(ls1, ls2)) {
    return false;
  }

  SVector3 vec1, vec2, vecn;
  vec1 = ls1->toVector();
  vec2 = SVector3(ls1->v1()->point(), ls2->v1()->point());
  vecn = crossprod(vec1, vec2);
  if (vecn.normSq() < TOLERANCE * TOLERANCE) {
    return true;
  } else {
    return false;
  }
}

bool isOverlapped(PGeoLineSegment *ls1, PGeoLineSegment *ls2) {
  if (ls1->vout()->point() < ls2->vin()->point() ||
      ls2->vout()->point() < ls1->vin()->point()) {
    return false;
  }

  if (!isCollinear(ls1, ls2)) {
    return false;
  }

  return true;
}

SVector3 calcAngleBisectVector(
  const SPoint3 &p0, const SPoint3 &p1, const SPoint3 &p2) {
  SVector3 v1{p0, p1}, v2{p0, p2}, vb;
  v1.normalize();
  v2.normalize();
  vb = v1 + v2;

  return vb;
}

SVector3 calcAngleBisectVector(
  const SVector3 &v1, const SVector3 &v2,
  const std::string &s1, const std::string &s2) {
  SVector3 n1{1, 0, 0}, n2{1, 0, 0}, p1, p2;
  if (s1 == "right") {
    n1 = -1 * n1;
  }
  p1 = crossprod(n1, v1).unit();

  if (s2 == "right") {
    n2 = -1 * n2;
  }
  p2 = crossprod(n2, v2).unit();

  return p1 + p2;
}

int calcBoundVertices(std::vector<PDCELVertex *> &vertices,
                      const SVector3 &sv_baseline,
                      const SVector3 &sv_bound, Layup *layup) {
  if (layup == nullptr) {
    std::cout << markError
              << " calcBoundVertices: layup must be valid" << std::endl;
    return -1;
  }
  if (vertices.empty()) {
    std::cout << markError
              << " calcBoundVertices: vertices must contain the starting point"
              << std::endl;
    return -1;
  }

  SVector3 bound_dir = sv_bound;
  SVector3 n = crossprod(bound_dir, sv_baseline);
  SVector3 p = crossprod(sv_baseline, n);
  p.normalize();
  bound_dir.normalize();

  double dp = dot(bound_dir, p);
  if (std::abs(dp) < TOLERANCE) {
    std::cout << markError << " calcBoundVertices: bound direction is"
              << " perpendicular to the layer normal" << std::endl;
    return -1;
  }

  PDCELVertex *pv_prev, *pv_new;
  double thk, thkp;
  for (int i = 0; i < layup->getLayers().size(); ++i) {
    thk = layup->getLayers()[i].getLamina()->getThickness() *
          layup->getLayers()[i].getStack();
    thkp = thk / dp;
    pv_prev = vertices.back();
    SPoint3 p_new = (SVector3(pv_prev->point()) + thkp * bound_dir).point();
    pv_new = new PDCELVertex();
    pv_new->setPoint(p_new);
    vertices.push_back(pv_new);
  }
  return 0;
}
