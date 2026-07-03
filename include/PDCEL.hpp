#pragma once

// Forward declarations first — before any includes — to break circular
// dependencies in the include chain.
class Baseline;
class Message;
class PGeoLineSegment;
struct BuilderConfig;

namespace dcel {
class PDCELVertex;
class PDCELHalfEdge;
class PDCELHalfEdgeLoop;
class PDCELFace;
class PDCEL;
}  // namespace dcel

#include <list>
#include <string>
#include <vector>

namespace dcel {
std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
    const PDCEL &dcel, PDCELVertex *v,
    std::vector<PGeoLineSegment *> &temp_segs);
PDCELHalfEdge *findHalfEdgeBelowVertex(const PDCEL &dcel,
                                       PDCELVertex *v);
}  // namespace dcel

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
namespace dcel {

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
  std::unordered_map<unsigned int, int> _split_counts;
  unsigned int _next_vertex_id = 1;
  unsigned int _next_halfedge_id = 1;
  unsigned int _next_loop_id = 1;
  unsigned int _next_face_id = 1;
  unsigned int _next_edge_lineage_id = 1;

  // Helper functions
  void updateEdgeNeighbors(PDCELHalfEdge *);
  unsigned int allocateVertexId() { return _next_vertex_id++; }
  unsigned int allocateHalfEdgeId() { return _next_halfedge_id++; }
  unsigned int allocateLoopId() { return _next_loop_id++; }
  unsigned int allocateFaceId() { return _next_face_id++; }
  unsigned int allocateEdgeLineageId() { return _next_edge_lineage_id++; }

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

  /// Serialize all DCEL entities to a plain-text file for post-mortem analysis.
  /// Safe to call even when the DCEL is in a partially broken state: only
  /// stored IDs are printed; no loop walks or deep pointer chains are followed.
  void dumpToFile(const std::string &filename) const;

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
  void resetSplitLineageCounters() { _split_counts.clear(); }

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

  /// Split a bounded face by a simple polyline whose endpoints lie on the
  /// face outer boundary. The original face is deleted on success.
  /// Returns an empty list when the split is not viable (e.g. the path
  /// endpoints are not on the outer boundary). Such geometric rejections are
  /// a recoverable status the caller is expected to handle, so they are logged
  /// at debug level only — the caller chooses whether the empty result is an
  /// error or a clean fall-back.
  std::list<PDCELFace *> splitFaceByPolyline(
      PDCELFace *f, const std::vector<PDCELVertex *> &path);
  
  /// Split a bounded face (convex) by a line or line segment
  std::list<PDCELFace *> splitFace(PDCELFace *f, PGeoLineSegment *ls);

  /// Bridge the outer boundary of a bounded face to one of its inner (hole)
  /// boundaries by inserting a single edge v_outer -> v_inner. The two boundary
  /// cycles merge into one, so the face becomes simply connected (an annulus
  /// turns into a slit disk) and the bridged inner loop is dropped from the
  /// hole list. The face is reused (not deleted) and returned. Returns nullptr
  /// without mutating the DCEL when the preconditions are not met: f bounded
  /// with an outer loop, v_outer on that outer loop, v_inner on a distinct
  /// inner loop of f.
  ///
  /// This is the closed-annulus analogue of the first radial cut: the very
  /// first staircase connector of a closed layer band lands one endpoint on the
  /// band's outer curve and the other on its inner curve, so it must bridge the
  /// two loops rather than split one. Subsequent connectors then split the
  /// resulting disk with the ordinary splitFaceByPolyline.
  PDCELFace *bridgeFaceLoops(PDCELFace *f, PDCELVertex *v_outer,
                             PDCELVertex *v_inner);

  /// Split a bounded face by a closed curve lying strictly inside it (carve a
  /// concentric ring). `ring` is a closed polygon (front == back) that does not
  /// touch the face boundary. The face is divided into two: the original face
  /// `f` is reused as the OUTER region (its outer loop kept, the ring added as a
  /// new hole) and a new INNER face is returned bounded by the ring; any of
  /// f's pre-existing holes that fall inside the ring are moved to the inner
  /// face. Returns {f_outer, f_inner} (f_outer == f). Returns an empty list
  /// without mutating the DCEL when the ring is degenerate or not a simple
  /// closed curve.
  ///
  /// Reuses the same outer/inner classification as everywhere else: the ring's
  /// two half-edge loops are labelled by PDCELHalfEdgeLoop::direction(), so the
  /// caller need not pre-decide which side is the hole.
  std::list<PDCELFace *> splitFaceByClosedCurve(
      PDCELFace *f, const std::vector<PDCELVertex *> &ring);
};

}  // namespace dcel
