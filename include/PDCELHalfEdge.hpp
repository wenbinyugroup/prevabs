#pragma once

#include "declarations.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"

#include "geo_types.hpp"

class PDCELFace;
class PDCELHalfEdgeLoop;
class PDCELVertex;
class PGeoLineSegment;

/** @ingroup geo
 * A DCEL half edge class.
 */
class PDCELHalfEdge {
private:
  PDCELVertex *_source;
  PDCELHalfEdge *_twin, *_prev, *_next;
  PDCELHalfEdgeLoop *_loop;
  PDCELFace *_face = nullptr;
  PGeoLineSegment *_line_segment;
  int _sign;
  unsigned int _lineage_id = 0;

public:
  PDCELHalfEdge()
      : _source(nullptr), _twin(nullptr), _prev(nullptr), _next(nullptr),
        _loop(nullptr), _face(nullptr),
        _line_segment(nullptr) {}
  PDCELHalfEdge(PDCELVertex *source)
      : _source(source), _twin(nullptr), _prev(nullptr), _next(nullptr),
        _loop(nullptr), _face(nullptr),
        _line_segment(nullptr) {}
  PDCELHalfEdge(PDCELVertex *source, bool /*build*/)
      : _source(source), _twin(nullptr), _prev(nullptr), _next(nullptr),
        _loop(nullptr), _face(nullptr),
        _line_segment(nullptr) {}
  PDCELHalfEdge(PDCELVertex *source, int sign)
      : _source(source), _twin(nullptr), _prev(nullptr), _next(nullptr),
        _loop(nullptr), _face(nullptr), _sign(sign),
        _line_segment(nullptr) {}

  friend std::ostream &operator<<(std::ostream &, PDCELHalfEdge *);
  std::string printString();
  std::string printBrief();
  void print();
  void print2();

  PDCELVertex *source() { return _source; }
  PDCELVertex *target() { return _twin->_source; }
  PDCELHalfEdge *twin() { return _twin; }
  PDCELHalfEdge *prev() { return _prev; }
  PDCELHalfEdge *next() { return _next; }
  PDCELHalfEdgeLoop *loop() { return _loop; }
  PDCELFace *face() { return _face; }
  PGeoLineSegment *lineSegment() { return _line_segment; }

  bool isFinite();

  int sign() { return _sign; }
  unsigned int lineageId() const { return _lineage_id; }

  SVector3 toVector();
  PGeoLineSegment *toLineSegment();
  double angle();

  void resetLoop() { _loop = nullptr; }

  void setSource(PDCELVertex *source) { _source = source; }
  void setTwin(PDCELHalfEdge *twin) { _twin = twin; }
  void setPrev(PDCELHalfEdge *prev) { _prev = prev; }
  void setNext(PDCELHalfEdge *next) { _next = next; }
  void setLoop(PDCELHalfEdgeLoop *);
  void setIncidentFace(PDCELFace *face) { _face = face; }
  void setLineSegment(PGeoLineSegment *line_segment) { _line_segment = line_segment; }
  void setSign(int sign) { _sign = sign; }
  void setLineageId(unsigned int lineage_id) { _lineage_id = lineage_id; }

  void clearLineSegment() { _line_segment = nullptr; }

};
