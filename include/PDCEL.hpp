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
struct BuilderConfig;

#include <list>
#include <vector>

std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
    const class PDCEL &dcel, PDCELVertex *v,
    std::vector<PGeoLineSegment *> &temp_segs);
PDCELHalfEdge *findHalfEdgeBelowVertex(const class PDCEL &dcel,
                                       PDCELVertex *v);

#include "declarations.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalVariables.hpp"

#include "PDCELUtils.hpp"

#include <memory>
#include <set>
#include <unordered_map>

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

  /// Segments created internally by addEdge(v1, v2) — owned by PDCEL.
  std::list<std::unique_ptr<PGeoLineSegment>> _owned_segments;

  /// Half-edges removed from _halfedges but kept live so that ->twin() traversal
  /// through bounding-box edges continues to work.  Owned by PDCEL.
  std::vector<std::unique_ptr<PDCELHalfEdge>> _background_halfedges;

  /// Sweep-line keep flag for each half-edge loop (default: false).
  std::unordered_map<PDCELHalfEdgeLoop *, bool> _loop_keep;
  /// Sweep-line adjacent-loop link: inner loop → its enclosing outer loop.
  std::unordered_map<PDCELHalfEdgeLoop *, PDCELHalfEdgeLoop *> _loop_adjacent;

  // Helper functions
  void updateEdgeNeighbors(PDCELHalfEdge *);

  /// Return the first vertex in _vertex_tree within GEO_TOL of v, or nullptr.
  PDCELVertex *findCoincidentVertex(PDCELVertex *v) const;

  friend std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
      const PDCEL &dcel, PDCELVertex *v,
      std::vector<PGeoLineSegment *> &temp_segs);
  friend PDCELHalfEdge *findHalfEdgeBelowVertex(const PDCEL &dcel,
                                                PDCELVertex *v);

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

  /// Find the half-edge with source vertex v whose incident face is f.
  PDCELHalfEdge *findHalfEdgeInFace(PDCELVertex *v, PDCELFace *f);

  /// Split an existing edge at v.
  /// v is resolved to the canonical DCEL vertex (an existing coincident vertex
  /// may be reused instead of v; if so, v is deleted).
  /// Returns the canonical split vertex.
  PDCELVertex *splitEdge(PDCELHalfEdge *e, PDCELVertex *v);

  /*!
    \return The half edge from v1 to v2.
   */
  PDCELHalfEdge *addEdge(PDCELVertex *v1, PDCELVertex *v2);
  PDCELHalfEdge *addEdge(PGeoLineSegment *ls);
  /// Remove an edge and collapse its two endpoints into the source vertex.
  /// The target vertex is deleted and all half-edges formerly incident on it
  /// are redirected to the source vertex. Any external pointer to the target
  /// vertex becomes dangling after this call.
  void removeEdge(PDCELHalfEdge *);

  /// Return the half-edge from v1 to v2, or nullptr if none exists.
  PDCELHalfEdge *findHalfEdgeBetween(PDCELVertex *v1, PDCELVertex *v2);
  PDCELHalfEdge *findHalfEdgeBetween(PDCELVertex *v1,
                                     PDCELVertex *v2) const;

  void addEdgesFromCurve(const std::vector<PDCELVertex *> &vertices);

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

  /// Sweep-line keep flag: true if this loop belongs to a finalized face boundary.
  bool isLoopKept(PDCELHalfEdgeLoop *hel) const;
  void setLoopKept(PDCELHalfEdgeLoop *hel, bool kept);
  /// Sweep-line adjacent-loop link: the enclosing outer loop for an inner loop,
  /// or nullptr if none has been set.
  PDCELHalfEdgeLoop *adjacentLoop(PDCELHalfEdgeLoop *hel) const;
  void setAdjacentLoop(PDCELHalfEdgeLoop *hel, PDCELHalfEdgeLoop *adj);

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

  /// Split a bounded face by new edges connecting two vertices.
  /// The original face f is deleted; any external pointer to f becomes
  /// dangling after this call. Use only the returned faces going forward.
  /*!
    \param f the bounded face (deleted on return)
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
