#pragma once

// Forward declarations first — before any includes — to break circular
// dependencies in the include chain.
class Baseline;
class Message;
class PDCELVertex;
class PDCELHalfEdge;
class PDCELHalfEdgeLoop;
class PDCELFace;
class PGeoLineSegment;

#include "declarations.hpp"
#include "PBaseLine.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalVariables.hpp"

#include <list>
#include <set>
#include <vector>

/// Comparator for PDCELVertex* — orders by geometric position (lexicographic
/// x, y, z), matching the sort order required by the sweep-line algorithm.
/// The body of operator() is defined in PDCEL.cpp to avoid requiring the full
/// PDCELVertex definition in this header (circular include guard issue).
struct CompareVertexByPoint {
  bool operator()(PDCELVertex *a, PDCELVertex *b) const;
};

/** @ingroup geo
 * A doubly connect edge list class.
 */
class PDCEL {
private:
  std::list<PDCELVertex *> _vertices;
  std::list<PDCELHalfEdge *> _halfedges;
  std::list<PDCELFace *> _faces;

  std::list<PDCELHalfEdgeLoop *> _halfedge_loops;

  std::set<PDCELVertex *, CompareVertexByPoint> _vertex_tree;

  /// Segments created internally by addEdge(v1, v2) — owned and deleted by PDCEL.
  std::list<PGeoLineSegment *> _owned_segments;

  /// Half-edges removed from _halfedges but kept live so that ->twin() traversal
  /// through bounding-box edges continues to work.  Deleted in ~PDCEL().
  std::vector<PDCELHalfEdge *> _background_halfedges;

  // Helper functions
  void updateEdgeNeighbors(PDCELHalfEdge *);

  /// Find line segments intersecting the vertical sweep line passing the given vertex.
  /// Temporary segments created for half-edges without a pre-existing line segment
  /// are appended to temp_segs; the caller is responsible for deleting them.
  std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
      PDCELVertex *, std::vector<PGeoLineSegment *> &temp_segs);

  /// Find the half-edge of the nearest segment below v, or nullptr if none.
  /// All temporary segments allocated during the sweep are deleted before returning.
  PDCELHalfEdge *findHalfEdgeBelowVertex(PDCELVertex *);

  /// Return the first vertex in _vertex_tree within GEO_TOL of v, or nullptr.
  PDCELVertex *findCoincidentVertex(PDCELVertex *v) const;

public:
  PDCEL() = default;
  ~PDCEL();

  PDCEL(const PDCEL &) = delete;
  PDCEL &operator=(const PDCEL &) = delete;

  void initialize();

  void print_dcel();

  /// Check structural DCEL invariants and log a warning for each violation.
  /// Returns true if all invariants hold, false if any violation is found.
  /// Intended for use in debug builds; the cost is O(V + E + F).
  bool validate();

  const std::list<PDCELVertex *> &vertices() const { return _vertices; }
  const std::list<PDCELHalfEdge *> &halfedges() const { return _halfedges; }
  const std::list<PDCELFace *> &faces() const { return _faces; }

  const std::list<PDCELHalfEdgeLoop *> &halfedgeloops() const { return _halfedge_loops; }

  void fixGeometry(const BuilderConfig &);

  // =================================================================
  // VERTEX

  /// Add v to the DCEL.  If a geometrically coincident vertex (within GEO_TOL)
  /// already exists, v is NOT inserted and the existing vertex is returned
  /// instead.  Otherwise v is inserted and returned.
  /// The caller MUST use the return value as the canonical vertex — it may
  /// differ from v when a coincident vertex is found.  If v was heap-allocated
  /// and the return value differs from v, the caller is responsible for
  /// deleting v.
  PDCELVertex *addVertex(PDCELVertex *v);
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
  void removeHalfEdgeLoop(PDCELHalfEdgeLoop *);

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
