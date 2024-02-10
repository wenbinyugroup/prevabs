#pragma once

#include "declarations.hpp"
#include "PBaseLine.hpp"
#include "PBST.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"

#include <list>
#include <vector>

class Baseline;
class PDCELVertex;
class PDCELHalfEdge;
class PDCELHalfEdgeLoop;
class PDCELFace;
class PAVLTreeVertex;
class PGeoLineSegment;


/** @ingroup geo
 * A doubly connect edge list class.
 */
class PDCEL {
private:
  std::list<PDCELVertex *> _vertices;
  std::list<PDCELHalfEdge *> _halfedges;
  std::list<PDCELFace *> _faces;

  std::list<PDCELHalfEdgeLoop *> _halfedge_loops;

  PAVLTreeVertex *_vertex_tree;

  // Helper functions
  void updateEdgeNeighbors(PDCELHalfEdge *);

  /// Find line segments intersecting the verticle sweep line passing the given vertex
  std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(PDCELVertex *);
  PGeoLineSegment *findLineSegmentBelowVertex(PDCELVertex *);

public:
  PDCEL() = default;
  ~PDCEL();

  void initialize();

  void print_dcel();

  std::list<PDCELVertex *> &vertices() { return _vertices; }
  std::list<PDCELHalfEdge *> &halfedges() { return _halfedges; }
  std::list<PDCELFace *> &faces() { return _faces; }

  std::list<PDCELHalfEdgeLoop *> &halfedgeloops() { return _halfedge_loops; }

  PAVLTreeVertex *vertextree() { return _vertex_tree; }

  void fixGeometry(Message *);

  // =================================================================
  // VERTEX

  void addVertex(PDCELVertex *v);
  void removeVertex(PDCELVertex *v);

  // =================================================================
  // HALF EDGE

  /// Find the half edge with source vertex v having incident face f
  PDCELHalfEdge *findHalfEdge(PDCELVertex *v, PDCELFace *f);

  /// Add a new vertex on an existing edge
  /*!
    \param v a new vertex.
    \param e the existing edge.
  */
  void splitEdge(PDCELHalfEdge *e, PDCELVertex *v);

  /*!
    \return The half edge from v1 to v2.
   */
  PDCELHalfEdge *addEdge(PDCELVertex *v1, PDCELVertex *v2);
  PDCELHalfEdge *addEdge(PGeoLineSegment *ls);
  void removeEdge(PDCELHalfEdge *);

  /// Check if two vertices are already connected by halfedges
  PDCELHalfEdge *findHalfEdge(PDCELVertex *v1, PDCELVertex *v2);

  void addEdgesFromCurve(Baseline *);

  // =================================================================
  // HALF EDGE LOOP

  /// Check if the half edge cycle containing he1 and he2 is the outer
  /// or inner component of a face. This function only designed for
  /// the two half edges that pass through the leftmost vertex.
  /*!
    \param he1 the half edge goes into the vertex
    \param he2 the half edge leaves the vertex
    \return integer 1 for outer boundary, -1 for inner boundary
  */
  int isOuterOrInnerBoundary(PDCELHalfEdge *he1, PDCELHalfEdge *he2);

  PDCELHalfEdgeLoop *addHalfEdgeLoop(PDCELHalfEdge *he);
  PDCELHalfEdgeLoop *addHalfEdgeLoop(const std::list<PDCELVertex *> &vloop);
  void removeHalfEdgeLoop(PDCELHalfEdgeLoop *);
  void clearHalfEdgeLoops();

  void removeTempLoops();
  void createTempLoops();
  PDCELHalfEdgeLoop *findNearestLoop(PDCELHalfEdgeLoop *);
  void linkHalfEdgeLoops();

  PDCELHalfEdgeLoop *findEnclosingLoop(PDCELVertex *);

  /// Find intersection points between a half edge loop and a straight line. 
  /// Both loop and line will be updated using the intersecting vertices
  void findCurvesIntersection(PDCELHalfEdgeLoop *, PGeoLineSegment *);

  // =================================================================
  // FACE

  PDCELFace *addFace(PDCELHalfEdgeLoop *hel);
  /// Add a new face given a loop of vertices in an existing face
  /*!
    \param vloop a list of vertices enclosing a face. the first and the last
    elements are the same. \param f the face where the new face is inserted.
  */
  PDCELFace *addFace(const std::list<PDCELVertex *> &vloop,
                     PDCELFace *f = nullptr);

  /// Split a bounded face by new edges connecting two vertices
  /*!
    \param f the bounded face
    \param v1 the first vertex
    \param v2 the second vertex
    \return A list of two new faces. The first one is the incident face of the
    half edge v1->v2.
  */
  std::list<PDCELFace *> splitFace(PDCELFace *f, PDCELVertex *v1,
                                   PDCELVertex *v2);
  
  /// Split a bounded face (convex) by a line or line segment
  std::list<PDCELFace *> splitFace(PDCELFace *f, PGeoLineSegment *ls);
};
