#include "PGeoLine.hpp"
#include "PDCELVertex.hpp"
#include "geo.hpp"

#include <iostream>
#include <iterator>
#include <list>
#include <vector>

// std::ostream &operator<<(std::ostream &out, PGeoLine *line) {
//   std::list<PDCELVertex *>::iterator it;
//   for (it = line->_vertices.begin(); it != line->_vertices.end(); ++it) {
//     out << *it << " -> ";
//   }
//   if (line->isClosed()) {
//     out << "loop";
//   } else {
//     out << "open";
//   }
//   return out;
// }

// void PGeoLine::setVertices(std::list<PDCELVertex *> vertices) {
//   _vertices = vertices;
// }

// void PGeoLine::addVertex(PDCELVertex *v) {
//   _vertices.push_back(v);
// }

// void PGeoLine::closeLine() {
//   _vertices.push_back(_vertices.front());
// }




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
  return s;
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




PDCELVertex *PGeoLineSegment::getParametricVertex1(double u) {

  double x, y, z;

  x = _v1->x() + u * (_v2->x() - _v1->x());
  y = _v1->y() + u * (_v2->y() - _v1->y());
  z = _v1->z() + u * (_v2->z() - _v1->z());

  return new PDCELVertex(x, y, z);

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
  return nullptr;
}

void PGeoLineSegment::setHalfEdge(PDCELHalfEdge *he) {
  if (he->source() == _v1) {
    _he12 = he;
  } else if (he->source() == _v2) {
    _he21 = he;
  }
}

// ===================================================================
//                                                       Class PGeoArc
PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, int direction) {
  // amethod = "pp";
  _center = center;
  _start = start;
  _end = start;
  _radius2 = calcDistanceSquared(_center, _start);
  _angle = 360.0;
  _direction = direction;
}

PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, PDCELVertex *end, int direction) {
  // amethod = "ppp";
  _center = center;
  _start = start;
  _end = end;
  _direction = direction;
  _radius2 = calcDistanceSquared(_center, _start);

  SVector3 vs{_center->point(), _start->point()};
  SVector3 ve{_center->point(), _end->point()};
  _angle = rad2deg(acos(dot(vs, ve) / _radius2));
  int turn = getTurningSide(vs, ve);
  if (turn * direction < 0) {
    _angle = 360.0 - _angle;
  }
}

PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, double angle, int direction) {
  // amethod = "ppa";
  _center = center;
  _start = start;
  _angle = angle;
  _direction = direction;
  _radius2 = calcDistanceSquared(_center, _start);

  SVector3 vs{_center->point(), _start->point()};
  STensor3 mr;
  mr = getRotationMatrix(_angle, direction, DEGREE);
  SVector3 ve;
  ve = mr * vs;
  _end = new PDCELVertex(_center->point() + ve.point());
}

PGeoArc::PGeoArc(PDCELVertex *center, PDCELVertex *start, PDCELVertex *end, double angle, int direction) {
  _center = center;
  _start = start;
  _end = end;
  _angle = angle;
  _direction = direction;
}




