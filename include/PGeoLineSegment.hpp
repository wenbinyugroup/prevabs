#pragma once

#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "gmsh_mod/SVector3.h"

#include <iostream>

class PDCELHalfEdge;
class PDCELVertex;

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
