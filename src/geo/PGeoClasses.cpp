#include "PGeoClasses.hpp"

#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "utilities.hpp"

#include "gmsh/SPoint3.h"
#include "gmsh/STensor3.h"
#include "gmsh/SVector3.h"

#include <cmath>
#include <iostream>

PGeoPoint2::PGeoPoint2(const PGeoPoint3 &p, const int &plane) {
  if (plane == 0) {
    _c1 = p[1];
    _c2 = p[2];
  }
  else if (plane == 1) {
    _c1 = p[2];
    _c2 = p[0];
  }
  else if (plane == 2) {
    _c1 = p[0];
    _c2 = p[1];
  }
}

PGeoPoint2 &PGeoPoint2::operator=(const PGeoPoint2 &p) {
  _c1 = p.c1();
  _c2 = p.c2();
  return *this;
}

bool operator<(const PGeoPoint2 &p1, const PGeoPoint2 &p2) {
  if (p1.c1() < p2.c1()) return true;
  if (p1.c1() > p2.c1()) return false;
  if (p1.c2() < p2.c2()) return true;
  return false;
}

bool operator>(const PGeoPoint2 &p1, const PGeoPoint2 &p2) {
  return p2 < p1;
}

PGeoPoint2 operator+(const PGeoPoint2 &p1, const PGeoPoint2 &p2) {
  return PGeoPoint2(p1.c1()+p2.c1(), p1.c2()+p2.c2());
}

PGeoPoint2 operator-(const PGeoPoint2 &p1, const PGeoPoint2 &p2) {
  return PGeoPoint2(p1.c1()-p2.c1(), p1.c2()-p2.c2());
}










PGeoPoint3 &PGeoPoint3::operator=(const PGeoPoint3 &p) {
  _c1 = p.c1();
  _c2 = p.c2();
  _c3 = p.c3();
  return *this;
}

bool operator<(const PGeoPoint3 &p1, const PGeoPoint3 &p2) {
  if (p1.c1() < p2.c1()) return true;
  if (p1.c1() > p2.c1()) return false;
  if (p1.c2() < p2.c2()) return true;
  if (p1.c2() > p2.c2()) return false;
  if (p1.c3() < p2.c3()) return true;
  return false;
}

bool operator>(const PGeoPoint3 &p1, const PGeoPoint3 &p2) {
  return p2 < p1;
}

PGeoPoint3 operator+(const PGeoPoint3 &p1, const PGeoPoint3 &p2) {
  return PGeoPoint3(p1.c1()+p2.c1(), p1.c2()+p2.c2(), p1.c3()+p2.c3());
}

PGeoPoint3 operator+(const PGeoPoint3 &p, const PGeoVector3 &v) {
  return PGeoPoint3(p.c1()+v.c1(), p.c2()+v.c2(), p.c3()+v.c3());
}

// PGeoPoint3 operator-(const PGeoPoint3 &p1, const PGeoPoint3 &p2) {
//   return PGeoPoint3(p1.c1()-p2.c1(), p1.c2()-p2.c2(), p1.c3()-p2.c3());
// }










PGeoVector3 &PGeoVector3::operator=(const PGeoVector3 &v) {
  _p = v._p;
  return *this;
}

PGeoVector3 operator-(const PGeoPoint3 &p1, const PGeoPoint3 &p2) {
  return PGeoVector3(p1.c1()-p2.c1(), p1.c2()-p2.c2(), p1.c3()-p2.c3());
}

PGeoVector3 operator*(const double &m, const PGeoVector3 &v) {
  return PGeoVector3(m*v.c1(), m*v.c2(), m*v.c3());
}










// ===================================================================
//                                                       Class Matrix2
// std::vector<double> PGeoMatrix2::operator[](int index) const {
//   if (index == 0)
//     return mr1;
//   else if (index == 1)
//     return mr2;
// }

// std::ostream &operator<<(std::ostream &out, PGeoLineSegment *ls) {
//   out << ls->_v1 << " - " << ls->_v2;
//   return out;
// }

// PGeoLineSegment::PGeoLineSegment(PDCELVertex *v1, PDCELVertex *v2) {
//   _v1 = v1;
//   _v2 = v2;
//   _he12 = nullptr;
//   _he21 = nullptr;
// }

// PGeoLineSegment::PGeoLineSegment(PDCELVertex *v1, SVector3 d) {
//   _v1 = v1;
//   _v2 = new PDCELVertex(v1->point() + d.point());
//   _he12 = nullptr;
//   _he21 = nullptr;
// }

// PGeoLineSegment::PGeoLineSegment(PGeoLineSegment *ls) {
//   _v1 = ls->v1();
//   _v2 = ls->v2();
//   _he12 = ls->he12();
//   _he21 = ls->he21();
// }

// std::string PGeoLineSegment::printString() {
//   std::string s = "";
//   s = _v1->printString() + " -> " + _v2->printString();
//   return s;
// }

// void PGeoLineSegment::print2() {
//   std::cout << _v1->point2() << " - " << _v2->point2();
// }

// PDCELVertex *PGeoLineSegment::vin() {
//   return _v1->point() < _v2->point() ? _v1 : _v2;
// }

// PDCELVertex *PGeoLineSegment::vout() {
//   return _v1->point() < _v2->point() ? _v2 : _v1;
// }

// SVector3 PGeoLineSegment::toVector() {
//   return SVector3(_v1->point(), _v2->point());
// }




// PDCELVertex *PGeoLineSegment::getParametricVertex1(double u) {

//   double x, y, z;

//   x = _v1->x() + u * (_v2->x() - _v1->x());
//   y = _v1->y() + u * (_v2->y() - _v1->y());
//   z = _v1->z() + u * (_v2->z() - _v1->z());

//   return new PDCELVertex(x, y, z);

// }




// PDCELVertex *PGeoLineSegment::getParametricVertex(double u) {
//   if (fabs(u) < TOLERANCE) {
//     return _v1;
//   } else if (fabs(u - 1) < TOLERANCE) {
//     return _v2;
//   } else {
//     double x, y, z;

//     x = _v1->x() + u * (_v2->x() - _v1->x());
//     y = _v1->y() + u * (_v2->y() - _v1->y());
//     z = _v1->z() + u * (_v2->z() - _v1->z());

//     return new PDCELVertex(x, y, z);
//   }
// }

// double PGeoLineSegment::getParametricLocation(PDCELVertex *v) {
//   double u;

//   SPoint3 p1 = _v1->point();
//   SPoint3 p2 = _v2->point();
//   SPoint3 p = v->point();

//   if (fabs(p1[0] - p2[0]) > TOLERANCE) {
//     u = (p[0] - p1[0]) / (p2[0] - p1[0]);
//   } else if (fabs(p1[1] - p2[1]) > TOLERANCE) {
//     u = (p[1] - p1[1]) / (p2[1] - p1[1]);
//   } else {
//     u = (p[2] - p1[2]) / (p2[2] - p1[2]);
//   }

//   return u;
// }

// PDCELHalfEdge *PGeoLineSegment::getHalfEdgeWithSource(PDCELVertex *source) {
//   if (source == _v1) {
//     return _he12;
//   } else if (source == _v2) {
//     return _he21;
//   }
//   return nullptr;
// }

// void PGeoLineSegment::setHalfEdge(PDCELHalfEdge *he) {
//   if (he->source() == _v1) {
//     _he12 = he;
//   } else if (he->source() == _v2) {
//     _he21 = he;
//   }
// }

// // ===================================================================
// //                                                       Class PGeoArc
// PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, int direction) {
//   // amethod = "pp";
//   _center = center;
//   _start = start;
//   _end = start;
//   _radius2 = calcDistanceSquared(_center, _start);
//   _angle = 360.0;
//   _direction = direction;
// }

// PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, PDCELVertex *end, int direction) {
//   // amethod = "ppp";
//   _center = center;
//   _start = start;
//   _end = end;
//   _direction = direction;
//   _radius2 = calcDistanceSquared(_center, _start);

//   SVector3 vs{_center->point(), _start->point()};
//   SVector3 ve{_center->point(), _end->point()};
//   _angle = rad2deg(acos(dot(vs, ve) / _radius2));
//   int turn = getTurningSide(vs, ve);
//   if (turn * direction < 0) {
//     _angle = 360.0 - _angle;
//   }
// }

// PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, double angle, int direction) {
//   // amethod = "ppa";
//   _center = center;
//   _start = start;
//   _angle = angle;
//   _direction = direction;
//   _radius2 = calcDistanceSquared(_center, _start);

//   SVector3 vs{_center->point(), _start->point()};
//   STensor3 mr;
//   mr = getRotationMatrix(_angle, direction, DEGREE);
//   SVector3 ve;
//   ve = mr * vs;
//   _end = new PDCELVertex(_center->point() + ve.point());
// }

// PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, PDCELVertex *end, double angle, int direction) {
//   _center = center;
//   _start = start;
//   _end = end;
//   _angle = angle;
//   _direction = direction;
// }
