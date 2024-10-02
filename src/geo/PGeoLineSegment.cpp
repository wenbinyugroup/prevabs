#include "PGeoLineSegment.hpp"

#include "globalConstants.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <iostream>

std::ostream &operator<<(std::ostream &out, PGeoLineSegment *ls) {
  out << ls->_v1 << " - " << ls->_v2;
  return out;
}

PGeoLineSegment::PGeoLineSegment(PDCELVertex *v1, PDCELVertex *v2) {
  _v1 = v1;
  _v2 = v2;
  _he12 = nullptr;
  _he21 = nullptr;
}

PGeoLineSegment::PGeoLineSegment(PDCELVertex *v1, SVector3 d) {
  _v1 = v1;
  _v2 = new PDCELVertex(v1->point() + d.point());
  _he12 = nullptr;
  _he21 = nullptr;
}

PGeoLineSegment::PGeoLineSegment(PGeoLineSegment *ls) {
  _v1 = ls->v1();
  _v2 = ls->v2();
  _he12 = ls->he12();
  _he21 = ls->he21();
}

std::string PGeoLineSegment::printString() {
  std::string s = "";
  s = _v1->printString() + " -> " + _v2->printString();
  return s
}

void PGeoLineSegment::print2() {
  std::cout << _v1->point2() << " - " << _v2->point2();
}

PDCELVertex *PGeoLineSegment::vin() {
  return _v1->point() < _v2->point() ? _v1 : _v2;
}

PDCELVertex *PGeoLineSegment::vout() {
  return _v1->point() < _v2->point() ? _v2 : _v1;
}

SVector3 PGeoLineSegment::toVector() {
  return SVector3(_v1->point(), _v2->point());
}

PDCELVertex *PGeoLineSegment::getParametricVertex(double u) {
  if (fabs(u) < TOLERANCE) {
    return _v1;
  } else if (fabs(u - 1) < TOLERANCE) {
    return _v2;
  } else {
    double x, y, z;

    x = _v1->x() + u * (_v2->x() - _v1->x());
    y = _v1->y() + u * (_v2->y() - _v1->y());
    z = _v1->z() + u * (_v2->z() - _v1->z());

    return new PDCELVertex(x, y, z);
  }
}

double PGeoLineSegment::getParametricLocation(PDCELVertex *v) {
  double u;

  SPoint3 p1 = _v1->point();
  SPoint3 p2 = _v2->point();
  SPoint3 p = v->point();

  if (fabs(p1[0] - p2[0]) > TOLERANCE) {
    u = (p[0] - p1[0]) / (p2[0] - p1[0]);
  } else if (fabs(p1[1] - p2[1]) > TOLERANCE) {
    u = (p[1] - p1[1]) / (p2[1] - p1[1]);
  } else {
    u = (p[2] - p1[2]) / (p2[2] - p1[2]);
  }

  return u;
}

PDCELHalfEdge *PGeoLineSegment::getHalfEdgeWithSource(PDCELVertex *source) {
  if (source == _v1) {
    return _he12;
  } else if (source == _v2) {
    return _he21;
  }
}

void PGeoLineSegment::setHalfEdge(PDCELHalfEdge *he) {
  if (he->source() == _v1) {
    _he12 = he;
  } else if (he->source() == _v2) {
    _he21 = he;
  }
}
