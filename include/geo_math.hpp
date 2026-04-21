#pragma once

#include "Material.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo_common.hpp"

#include <string>
#include <vector>

bool isClose(
  const double&, const double&,
  const double&, const double&,
  double, double);

int getTurningSide(SVector3, SVector3);

double calcDistanceSquared(PDCELVertex *, PDCELVertex *);

SVector3 getVectorFromAngle(double angle, AnglePlane plane);

SPoint3 getParametricPoint(const SPoint3 &p1, const SPoint3 &p2, double u);

bool isParallel(PGeoLineSegment *, PGeoLineSegment *);
bool isCollinear(PGeoLineSegment *, PGeoLineSegment *);
bool isOverlapped(PGeoLineSegment *, PGeoLineSegment *);

SVector3 calcAngleBisectVector(const SPoint3 &, const SPoint3 &,
                               const SPoint3 &);
SVector3 calcAngleBisectVector(const SVector3 &, const SVector3 &,
                               const std::string &, const std::string &);

int calcBoundVertices(std::vector<PDCELVertex *> &, const SVector3 &,
                      const SVector3 &, Layup *);
