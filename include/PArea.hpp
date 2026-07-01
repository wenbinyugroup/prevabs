#pragma once

// Forward declarations first — before any includes — to break circular
// dependencies in the include chain.
class Message;
class PDCELVertex;
class PDCELHalfEdge;
class PDCELFace;
class PGeoLineSegment;
class Segment;

#include "declarations.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PSegment.hpp"
#include "utilities.hpp"
#include "globalVariables.hpp"
#include "geo_types.hpp"

#include <list>
#include <vector>

/** @ingroup cs
 * A cross-sectional area class.
 */
class PArea {
private:
  Segment *_segment;
  PDCELHalfEdge *_base, *_opposite;
  std::list<PDCELFace *> _faces;
  // Through-thickness vector for the `_mat_orient_e2 == "layup"` selector.
  // Frame for the `baseline` selector is computed per-face in
  // applyFrameFromBaseCurve, not on the area, so no `_y1` is kept here.
  SVector3 _y2{0, 1, 0}, _y3;
  SVector3 _prev_bound, _next_bound;

  // excluding vertices on the base curve and offset curve
  std::vector<PDCELVertex *> _prev_bound_vertices, _next_bound_vertices;
  PDCELFace *_face;
  PGeoLineSegment *_line_segment_base;

public:
  PArea();
  PArea(Segment *);
  ~PArea();

  void print();

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
  void setLocaly2(SVector3 v) { _y2 = v; };
  void setLocaly3(SVector3 v) { _y3 = v; };
  void setPrevBound(SVector3 &);
  void setNextBound(SVector3 &);
  void setPrevBoundVertices(std::vector<PDCELVertex *>);
  void setNextBoundVertices(std::vector<PDCELVertex *>);
  void addPrevBoundVertex(PDCELVertex *);
  void addNextBoundVertex(PDCELVertex *);
  void setFace(PDCELFace *f) { _face = f; }
  void setLineSegmentBase(PGeoLineSegment *ls) { _line_segment_base = ls; }

  void buildLayers(const BuilderConfig &);

  // After buildLayers() has populated _faces, override each face's local
  // y1/y2 (and theta1) using a nearest-segment query on the segment's base
  // curve. Only fires when the segment's mat-orient selector for that axis
  // is "baseline"; other selectors keep the area-level fallback set above.
  // This is the Phase B integration point of
  // plan-20260514-decouple-local-frame-from-map.md.
  void applyFrameFromBaseCurve(const BuilderConfig &);
};
