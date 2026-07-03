#pragma once

#include "declarations.hpp"
#include "dcel/PDCELVertex.hpp"

#include "geo_types.hpp"

#include <cmath>
#include <iostream>

// class PGeoLine {
// private:
//   std::list<dcel::PDCELVertex *> _vertices;

// public:
//   PGeoLine() {}

//   friend std::ostream &operator<<(std::ostream &, PGeoLine *);

//   std::list<dcel::PDCELVertex *> &vertices() { return _vertices; }
//   bool isClosed() { return (_vertices.front() == _vertices.back()); }

//   void setVertices(std::list<dcel::PDCELVertex *>);
//   void addVertex(dcel::PDCELVertex *);
//   void closeLine();
// };

class PGeoLineSegment {
private:
  dcel::PDCELVertex *_v1;
  dcel::PDCELVertex *_v2;

public:
  PGeoLineSegment()
      : _v1(nullptr), _v2(nullptr) {}
  PGeoLineSegment(dcel::PDCELVertex *, dcel::PDCELVertex *);
  PGeoLineSegment(dcel::PDCELVertex *, SVector3);
  PGeoLineSegment(PGeoLineSegment *);

  friend std::ostream &operator<<(std::ostream &, PGeoLineSegment *);

  std::string printString();
  void print2();

  dcel::PDCELVertex *v1() { return _v1; }
  dcel::PDCELVertex *v2() { return _v2; }
  dcel::PDCELVertex *vin(); // The left bottom vertex
  dcel::PDCELVertex *vout(); // The right top vertex

  void setV1(dcel::PDCELVertex *v) { _v1 = v; }
  void setV2(dcel::PDCELVertex *v) { _v2 = v; }

  SVector3 toVector();

  dcel::PDCELVertex *getParametricVertex1(double);
  dcel::PDCELVertex *getParametricVertex(double);
  double getParametricLocation(dcel::PDCELVertex *);
};

class PGeoArc {
private:
  dcel::PDCELVertex *_center, *_start, *_end;
  double _radius2; // radius square
  double _angle;   // angle in degree
  int _direction;
  // std::string amethod; // method of constructing the arc

public:
  PGeoArc() {}
  PGeoArc(dcel::PDCELVertex *, dcel::PDCELVertex *, int); // Circle
  PGeoArc(dcel::PDCELVertex *, dcel::PDCELVertex *, dcel::PDCELVertex *, int);
  PGeoArc(dcel::PDCELVertex *, dcel::PDCELVertex *, double, int);
  PGeoArc(dcel::PDCELVertex *, dcel::PDCELVertex *, dcel::PDCELVertex *, double, int);

  dcel::PDCELVertex *center() const { return _center; }
  dcel::PDCELVertex *start() const { return _start; }
  dcel::PDCELVertex *end() const { return _end; }
  double radiusSquare() const { return _radius2; }
  double angle() const { return _angle; }
  int direction() const { return _direction; }
  // std::string getMethod() const { return amethod; }
};
