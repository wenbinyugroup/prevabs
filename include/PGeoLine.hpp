#pragma once

#include "declarations.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"

#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <iostream>

// class PGeoLine {
// private:
//   std::list<PDCELVertex *> _vertices;

// public:
//   PGeoLine() {}

//   friend std::ostream &operator<<(std::ostream &, PGeoLine *);

//   std::list<PDCELVertex *> &vertices() { return _vertices; }
//   bool isClosed() { return (_vertices.front() == _vertices.back()); }

//   void setVertices(std::list<PDCELVertex *>);
//   void addVertex(PDCELVertex *);
//   void closeLine();
// };









class PGeoLineSegment {
private:
  PDCELVertex *_v1;
  PDCELVertex *_v2;
  PDCELHalfEdge *_he12;
  PDCELHalfEdge *_he21;

public:
  PGeoLineSegment()
      : _v1(nullptr), _v2(nullptr), _he12(nullptr), _he21(nullptr) {}
  PGeoLineSegment(PDCELVertex *, PDCELVertex *);
  PGeoLineSegment(PDCELVertex *, SVector3);
  PGeoLineSegment(PGeoLineSegment *);

  friend std::ostream &operator<<(std::ostream &, PGeoLineSegment *);

  std::string printString();
  void print2();

  PDCELVertex *v1() { return _v1; }
  PDCELVertex *v2() { return _v2; }
  PDCELHalfEdge *he12() { return _he12; }
  PDCELHalfEdge *he21() { return _he21; }
  PDCELVertex *vin(); // The left bottom vertex
  PDCELVertex *vout(); // The right top vertex

  void setV1(PDCELVertex *v) { _v1 = v; }
  void setV2(PDCELVertex *v) { _v2 = v; }

  SVector3 toVector();

  PDCELVertex *getParametricVertex1(double);
  PDCELVertex *getParametricVertex(double);
  double getParametricLocation(PDCELVertex *);

  PDCELHalfEdge *getHalfEdgeWithSource(PDCELVertex *);

  void setHalfEdge(PDCELHalfEdge *);
};









class PGeoArc {
private:
  PDCELVertex *_center, *_start, *_end;
  double _radius2; // radius square
  double _angle;   // angle in degree
  int _direction;
  // std::string amethod; // method of constructing the arc

public:
  PGeoArc() {}
  PGeoArc(PDCELVertex *, PDCELVertex *, int); // Circle
  PGeoArc(PDCELVertex *, PDCELVertex *, PDCELVertex *, int);
  PGeoArc(PDCELVertex *, PDCELVertex *, double, int);
  PGeoArc(PDCELVertex *, PDCELVertex *, PDCELVertex *, double, int);

  PDCELVertex *center() const { return _center; }
  PDCELVertex *start() const { return _start; }
  PDCELVertex *end() const { return _end; }
  double radiusSquare() const { return _radius2; }
  double angle() const { return _angle; }
  int direction() const { return _direction; }
  // std::string getMethod() const { return amethod; }
};
