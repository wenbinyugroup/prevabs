#pragma once

#include "declarations.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"

class PDCELHalfEdge;
class PDCELFace;
class PDCELVertex;

/** @ingroup geo
 * A DCEL half edge loop class.
 */
class PDCELHalfEdgeLoop {
private:
  // int _direction; // 1: outer boundary; -1: inner boundary
  PDCELHalfEdge *_incident_edge;
  PDCELFace *_face;
  int _direction;

public:
  PDCELHalfEdgeLoop()
      : _incident_edge(nullptr), _face(nullptr), _direction(0) {};

  void log();
  void print();

  int direction();
  PDCELHalfEdge *incidentEdge() { return _incident_edge; }
  PDCELFace *face() { return _face; }
  /// The bottom-left vertex of this loop (source of the incident edge).
  PDCELVertex *bottomLeftVertex();
  // int isOuterOrInnerBoundary();

  // void setDirection(int direction) { _direction = direction; }
  void setIncidentEdge(PDCELHalfEdge *he) { _incident_edge = he; }
  // void setFace(PDCELFace *f) { _face = f; }
  void setFace(PDCELFace *f);
  void setDirection(int direction) { _direction = direction; }

  /// Update _incident_edge to track the half-edge with the bottom-left source.
  void updateIncidentEdge(PDCELHalfEdge *);
};
