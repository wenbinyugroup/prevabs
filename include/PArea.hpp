#pragma once

#include "declarations.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PModel.hpp"
#include "PSegment.hpp"
#include "utilities.hpp"
#include "gmsh/SVector3.h"

#include <list>
#include <vector>

class PDCELVertex;
class PDCELHalfEdge;
class PDCELFace;
class PGeoLineSegment;
class Segment;
class PModel;


/** @ingroup cs
 * A cross-sectional area class.
 */
class PArea {
private:
  PModel *_pmodel;
  Segment *_segment;
  PDCELHalfEdge *_base, *_opposite;
  std::list<PDCELFace *> _faces;
  SVector3 _y2, _y3;
  SVector3 _prev_bound, _next_bound;

  // excluding vertices on the base curve and offset curve
  std::vector<PDCELVertex *> _prev_bound_vertices, _next_bound_vertices;
  PDCELFace *_face;
  PGeoLineSegment *_line_segment_base;

public:
  PArea();
  PArea(PModel *, Segment *);

  void print();

  PModel *pmodel() { return _pmodel; }
  Segment *segment() { return _segment; }
  std::list<PDCELFace *> &faces() { return _faces; }
  SVector3 &prevBound() { return _prev_bound; }
  SVector3 &nextBound() { return _next_bound; }
  std::vector<PDCELVertex *> &prevBoundVertices() {
    return _prev_bound_vertices;
  }
  std::vector<PDCELVertex *> &nextBoundVertices() {
    return _next_bound_vertices;
  }
  PDCELFace *face() { return _face; }
  PGeoLineSegment *lineSegmentBase() { return _line_segment_base; }

  SVector3 localy2() { return _y2; }
  SVector3 localy3();

  void setSegment(Segment *);
  void addFace(PDCELFace *);
  void setLocaly2(SVector3);
  void setLocaly3(SVector3);
  void setPrevBound(SVector3 &);
  void setNextBound(SVector3 &);
  void setPrevBoundVertices(std::vector<PDCELVertex *>);
  void setNextBoundVertices(std::vector<PDCELVertex *>);
  void addPrevBoundVertex(PDCELVertex *);
  void addNextBoundVertex(PDCELVertex *);
  void setFace(PDCELFace *f) { _face = f; }
  void setLineSegmentBase(PGeoLineSegment *ls) { _line_segment_base = ls; }

  void buildLayers(Message *);
};
