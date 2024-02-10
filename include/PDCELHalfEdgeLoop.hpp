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
  PDCELHalfEdgeLoop *_adjacent_loop;
  PDCELFace *_face;
  PDCELVertex *_bottom_left_vertex;
  int _direction;
  bool _keep;

public:
  PDCELHalfEdgeLoop()
      : _incident_edge(nullptr), _adjacent_loop(nullptr),
        _face(nullptr), _bottom_left_vertex(nullptr), _direction(0), _keep(false) {};

  void log();
  void print();

  int direction();
  PDCELHalfEdge *incidentEdge() { return _incident_edge; }
  PDCELHalfEdgeLoop *adjacentLoop() { return _adjacent_loop; }
  PDCELFace *face() { return _face; }
  PDCELVertex *bottomLeftVertex() { return _bottom_left_vertex; }
  bool keep() { return _keep; }
  // int isOuterOrInnerBoundary();

  // void setDirection(int direction) { _direction = direction; }
  void setIncidentEdge(PDCELHalfEdge *he) { _incident_edge = he; }
  void setAdjacentLoop(PDCELHalfEdgeLoop *hel) { _adjacent_loop = hel; }
  // void setFace(PDCELFace *f) { _face = f; }
  void setFace(PDCELFace *f);
  void setBottomLeftVertex(PDCELVertex *v) { _bottom_left_vertex = v; }
  void setDirection(int direction) { _direction = direction; }
  void setKeep(bool keep) { _keep = keep; }

  void updateVertexEdge(PDCELHalfEdge *);
};
