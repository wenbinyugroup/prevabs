#include "PDCEL.hpp"

#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "DCELSweepLine.hpp"
#include "DCELValidator.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <typeinfo>

namespace {

static const int kRepeatedSplitWarningThreshold = 3;
static const double kNearDegenerateAngleWarningRad = 1e-3;

double smallestAngleDelta(double a1, double a2) {
  const double delta = std::fabs(a1 - a2);
  return std::min(delta, 2.0 * PI - delta);
}

void warnIfAnglesTooClose(
    PDCELVertex *vertex, PDCELHalfEdge *he1, PDCELHalfEdge *he2) {
  if (vertex == nullptr || he1 == nullptr || he2 == nullptr) {
    return;
  }

  const double delta = smallestAngleDelta(he1->angle(), he2->angle());
  if (delta < kNearDegenerateAngleWarningRad) {
    PLOG(warning) << "updateEdgeNeighbors: near-degenerate outgoing edge angles"
                  << " at vertex " << vertex->printString()
                  << " (delta=" << delta << " rad)"
                  << ", edge1=" << he1->printString()
                  << ", edge2=" << he2->printString();
  }
}

} // namespace

PDCEL::~PDCEL() {
  for (auto v : _vertices)
    delete v;
  for (auto e : _halfedges)
    delete e;
  for (auto f : _faces)
    delete f;
  for (auto l : _halfedge_loops)
    delete l;
  // _owned_segments and _background_halfedges are unique_ptr collections;
  // elements are freed automatically when the lists are destroyed.
}

void PDCEL::initialize() {
  // Create the infinity large bounding box
  PDCELVertex *v_tr = addVertex(new PDCELVertex(0, INF, INF, false));
  PDCELVertex *v_tl = addVertex(new PDCELVertex(0, -INF, INF, false));
  PDCELVertex *v_bl = addVertex(new PDCELVertex(0, -INF, -INF, false));
  PDCELVertex *v_br = addVertex(new PDCELVertex(0, INF, -INF, false));

  PGeoLineSegment *ls_top = new PGeoLineSegment(v_tr, v_tl);
  PGeoLineSegment *ls_left = new PGeoLineSegment(v_tl, v_bl);
  PGeoLineSegment *ls_bottom = new PGeoLineSegment(v_bl, v_br);
  PGeoLineSegment *ls_right = new PGeoLineSegment(v_br, v_tr);

  _owned_segments.push_back(std::unique_ptr<PGeoLineSegment>(ls_top));
  _owned_segments.push_back(std::unique_ptr<PGeoLineSegment>(ls_left));
  _owned_segments.push_back(std::unique_ptr<PGeoLineSegment>(ls_bottom));
  _owned_segments.push_back(std::unique_ptr<PGeoLineSegment>(ls_right));

  PDCELHalfEdge *he_top = addEdge(ls_top);
  PDCELHalfEdge *he_left = addEdge(ls_left);
  PDCELHalfEdge *he_bottom = addEdge(ls_bottom);
  PDCELHalfEdge *he_right = addEdge(ls_right);

  PDCELHalfEdgeLoop *hel = addHalfEdgeLoop(he_top);

  PDCELFace *f = new PDCELFace(he_top, false); // The unbounded face
  f->setId(allocateFaceId());

  hel->setDirection(1);
  hel->setFace(f);
  setLoopKept(hel, true);

  // Assign background face to its bounding half-edges so validate() passes.
  he_top->setIncidentFace(f);
  he_left->setIncidentFace(f);
  he_bottom->setIncidentFace(f);
  he_right->setIncidentFace(f);

  _background_halfedges.push_back(std::unique_ptr<PDCELHalfEdge>(he_top->twin()));
  _background_halfedges.push_back(std::unique_ptr<PDCELHalfEdge>(he_left->twin()));
  _background_halfedges.push_back(std::unique_ptr<PDCELHalfEdge>(he_bottom->twin()));
  _background_halfedges.push_back(std::unique_ptr<PDCELHalfEdge>(he_right->twin()));

  _halfedges.remove(he_top->twin());
  _halfedges.remove(he_left->twin());
  _halfedges.remove(he_bottom->twin());
  _halfedges.remove(he_right->twin());

  _faces.push_back(f);
}

void PDCEL::print_dcel() {
  if (config.debug_level < DebugLevel::geo) {
    return;
  }
  PLOG(debug) << "DCEL Summary";

  PLOG(debug) << _vertices.size() << " vertices:";
  for (auto v : _vertices) {
    PLOG(debug) << v->printString() << " - degree " << v->degree();
  }

  PLOG(debug) << _halfedges.size() << " half edges:";
  for (auto e : _halfedges) {
    PLOG(debug) << e->printString();
  }

  PLOG(debug) << _halfedge_loops.size() << " half edge loops:";
  for (auto l : _halfedge_loops) {
    l->log();
  }

  PLOG(debug) << _faces.size() << " faces:";
  for (auto f : _faces) {
    f->print();
  }
}

void PDCEL::dumpToFile(const std::string &filename) const {
  std::ofstream f(filename);
  if (!f.is_open()) {
    PLOG(warning) << "dumpToFile: could not open " + filename;
    return;
  }

  // Safe ID-to-label helpers — return "nullptr" if the pointer is null.
  auto vid  = [](PDCELVertex      *v) -> std::string {
    return v ? v->label() : "nullptr";
  };
  auto hid  = [](PDCELHalfEdge    *h) -> std::string {
    return h ? h->label() : "nullptr";
  };
  auto lid  = [](PDCELHalfEdgeLoop *l) -> std::string {
    return l ? l->label() : "nullptr";
  };
  auto fid  = [](PDCELFace        *fc) -> std::string {
    return fc ? fc->label() : "nullptr";
  };

  f << "=== DCEL CRASH DUMP ===\n\n";

  // Vertices
  f << "VERTICES (" << _vertices.size() << "):\n";
  for (PDCELVertex *v : _vertices) {
    if (!v) { f << "  <null vertex>\n"; continue; }
    f << "  " << v->label()
      << " (" << v->x() << ", " << v->y() << ", " << v->z() << ")"
      << " edge=" << hid(v->edge())
      << "\n";
  }
  f << "\n";

  auto writeHalfEdgeLine = [&](PDCELHalfEdge *he, const char *scope) {
    if (!he) {
      f << "  <null halfedge>\n";
      return;
    }
    PDCELFace *hf = he->face();
    std::string face_label = hf ? (hf->label() + "(" + hf->logName() + ")")
                                : "nullptr";
    f << "  " << he->label()
      << " scope=" << scope
      << " src=" << vid(he->source())
      << " twin=" << hid(he->twin())
      << " next=" << hid(he->next())
      << " prev=" << hid(he->prev())
      << " loop=" << lid(he->loop())
      << " face=" << face_label
      << "\n";
  };

  // Half-edges: include both active and background-owned edges so any
  // twin/next/prev label printed elsewhere in the dump can be resolved.
  const std::size_t total_halfedges =
      _halfedges.size() + _background_halfedges.size();
  f << "HALFEDGES (" << total_halfedges << "):\n";
  for (PDCELHalfEdge *he : _halfedges) {
    writeHalfEdgeLine(he, "active");
  }
  for (const auto &owned_he : _background_halfedges) {
    writeHalfEdgeLine(owned_he.get(), "background");
  }
  f << "\n";

  // Half-edge loops
  f << "LOOPS (" << _halfedge_loops.size() << "):\n";
  for (PDCELHalfEdgeLoop *hel : _halfedge_loops) {
    if (!hel) { f << "  <null loop>\n"; continue; }
    bool kept = false;
    auto it = _loop_keep.find(hel);
    if (it != _loop_keep.end()) kept = it->second;

    PDCELHalfEdgeLoop *adj = nullptr;
    auto jt = _loop_adjacent.find(hel);
    if (jt != _loop_adjacent.end()) adj = jt->second;

    f << "  " << hel->label()
      << " incident=" << hid(hel->incidentEdge())
      << " face=" << fid(hel->face())
      << " kept=" << (kept ? "true" : "false")
      << " adjacent=" << lid(adj)
      << "\n";
  }
  f << "\n";

  // Faces
  f << "FACES (" << _faces.size() << "):\n";
  for (PDCELFace *fc : _faces) {
    if (!fc) { f << "  <null face>\n"; continue; }
    f << "  " << fc->label()
      << " name=" << fc->logName()
      << " bounded=" << (fc->isBounded() ? "true" : "false")
      << " outer=" << hid(fc->outer());
    for (PDCELHalfEdge *ih : fc->inners()) {
      f << " inner=" << hid(ih);
    }
    f << "\n";
  }
  f << "\n";

  f.flush();
  PLOG(info) << "DCEL state dumped to: " + filename;
}

bool PDCEL::validate() {
  return validateDCEL(*this);
}

void PDCEL::fixGeometry(const BuilderConfig &bcfg) {
  fixDCELGeometry(*this, bcfg);
}

// ===================================================================
//
// Vertex
//
// ===================================================================

bool CompareVertexByPoint::operator()(PDCELVertex *a, PDCELVertex *b) const {
  return a->point() < b->point();
}

PDCELVertex *PDCEL::findCoincidentVertex(PDCELVertex *v) const {
  const SPoint3 p = v->point();
  // Lower-bound probe: first set entry with x >= p.x() - GEO_TOL.
  // Using -INF for y and z ensures we start at the very first vertex in that
  // x-band regardless of y/z ordering.
  PDCELVertex lo(p.x() - GEO_TOL, -INF, -INF);
  for (auto it = _vertex_tree.lower_bound(&lo);
       it != _vertex_tree.end() && (*it)->x() <= p.x() + GEO_TOL;
       ++it) {
    if ((*it)->point().distance(p) <= GEO_TOL)
      return *it;
  }
  return nullptr;
}

PDCELVertex *PDCEL::addVertex(PDCELVertex *v) {
  if (v->isRegistered())
    return v;  // already in this DCEL

  // Merge with any geometrically coincident vertex that is already present.
  if (PDCELVertex *existing = findCoincidentVertex(v))
    return existing;

  v->setId(allocateVertexId());
  _vertices.push_back(v);
  _vertex_tree.insert(v);
  v->setRegistered(true);
  return v;
}

void PDCEL::removeVertex(PDCELVertex *v) {
  if (v->edge() != nullptr) {
    PLOG(error) << "removeVertex: vertex " << v->name()
                << " still has incident edges; cannot remove";
    return;
  }
  if (v->isRegistered()) {
    _vertices.remove(v);
    _vertex_tree.erase(v);
    v->setRegistered(false);
  }
}

// ===================================================================
//
// Half Edge
//
// Exception-safety model for DCEL mutation operations
// ────────────────────────────────────────────────────
// The "safe minimum" goal is: complete all heap allocations before
// patching any existing DCEL pointers.  If an allocation throws after
// earlier allocations have succeeded, the already-allocated objects
// are leaked, but the pre-existing DCEL graph is left untouched.
//
// Functions that meet this guarantee:
//   splitEdge      — 4 new PDCELHalfEdge at top, then all pointer patches
//   addEdge(v1,v2) — 1 new PGeoLineSegment + 2 new PDCELHalfEdge, then patches
//   addEdge(ls)    — 2 new PDCELHalfEdge, then patches
//   addFace(hel)   — 1 new PDCELFace, then patches
//
// Functions with weaker guarantees:
//   addFace(vloop,f) — allocations and patches are interleaved across loop
//                      iterations; a throw in iteration N cannot undo the
//                      pointer patches from iterations 1..N-1.
//   splitFace        — sequential sub-calls (addEdge, addHalfEdgeLoop,
//                      addFace); each sub-call is internally safe, but a
//                      throw in a later step cannot roll back earlier steps.
//
// There is no full transaction/rollback mechanism.  Callers must ensure
// inputs are valid (vertices exist, face is bounded) to avoid partial
// mutations.
//
// ===================================================================

PDCELHalfEdge *PDCEL::findHalfEdgeInFace(PDCELVertex *v, PDCELFace *f) {
  if (v->edge() == nullptr) {
    return nullptr;
  }

  PDCELHalfEdge *he;

  he = v->edge();
  do {
    if (he->face() == f) {
      return he;
    }
    if (he->twin() == nullptr) break;
    he = he->twin()->next();
  } while (he != nullptr && he != v->edge());

  return nullptr;
}

PDCELVertex *PDCEL::splitEdge(PDCELHalfEdge *e12, PDCELVertex *v0) {
  //          e12
  //          -->
  //     e10      e02
  //     -->      -->
  // v1 ----- v0 ----- v2
  //     <--      <--
  //     e01      e20
  //         <--
  //         e21

  PDCELHalfEdge *e21 = e12->twin();

  PDCELVertex *v1 = e12->source();
  PDCELVertex *v2 = e21->source();

  // Resolve v0 to the canonical DCEL vertex before allocating any structures.
  // addVertex may return an existing coincident vertex instead of v0.
  PDCELVertex *v0_canon = addVertex(v0);
  if (v0_canon != v0) {
    delete v0;
    v0 = v0_canon;
  }

  // If v0 is already one of the edge's endpoints, splitting would produce a
  // zero-length (degenerate) edge and corrupt the DCEL. Skip the split.
  if (v0 == v1 || v0 == v2) {
    return v0;
  }

  PDCELHalfEdgeLoop *hel12 = e12->loop();
  PDCELHalfEdgeLoop *hel21 = e21->loop();
  PDCELFace *f12 = e12->face();
  PDCELFace *f21 = e21->face();
  const unsigned int lineage_id = e12->lineageId();

  if (lineage_id != 0) {
    const int split_count = ++_split_counts[lineage_id];
    if (split_count >= kRepeatedSplitWarningThreshold) {
      PLOG(warning) << "splitEdge: repeated splits on edge lineage #"
                    << lineage_id << " (" << split_count
                    << " splits in current phase), edge="
                    << e12->printString();
    }
  }

  PDCELHalfEdge *e10 = new PDCELHalfEdge(v1, 1);
  PDCELHalfEdge *e01 = new PDCELHalfEdge(v0, -1);

  PDCELHalfEdge *e02 = new PDCELHalfEdge(v0, 1);
  PDCELHalfEdge *e20 = new PDCELHalfEdge(v2, -1);

  e10->setId(allocateHalfEdgeId());
  e01->setId(allocateHalfEdgeId());
  e02->setId(allocateHalfEdgeId());
  e20->setId(allocateHalfEdgeId());

  e10->setLineageId(lineage_id);
  e01->setLineageId(lineage_id);
  e02->setLineageId(lineage_id);
  e20->setLineageId(lineage_id);

  e10->setTwin(e01);
  e01->setTwin(e10);
  e20->setTwin(e02);
  e02->setTwin(e20);

  e10->setNext(e02);
  e02->setPrev(e10);
  e20->setNext(e01);
  e01->setPrev(e20);

  e10->setPrev(e12->prev());
  e02->setNext(e12->next());
  e20->setPrev(e21->prev());
  e01->setNext(e21->next());

  e10->setLoop(hel12);
  e02->setLoop(hel12);
  e20->setLoop(hel21);
  e01->setLoop(hel21);

  e10->setIncidentFace(f12);
  e02->setIncidentFace(f12);
  e20->setIncidentFace(f21);
  e01->setIncidentFace(f21);

  v0->setIncidentEdge(e01);

  // Update prev and next (guard for edges not yet wired into a face/loop)
  if (e12->prev()) e12->prev()->setNext(e10);
  if (e12->next()) e12->next()->setPrev(e02);
  if (e21->prev()) e21->prev()->setNext(e20);
  if (e21->next()) e21->next()->setPrev(e01);

  // Update vertex incident half edge if necessary
  if (v1->edge() == e12) {
    v1->setIncidentEdge(e10);
  }

  if (v2->edge() == e21) {
    v2->setIncidentEdge(e20);
  }

  // Update loop incident half edge if necessary
  if (hel12 != nullptr && hel12->incidentEdge() == e12) {
    hel12->setIncidentEdge(e10);
  }

  if (hel21 != nullptr && hel21->incidentEdge() == e21) {
    hel21->setIncidentEdge(e20);
  }

  // Update face incident half edge if necessary
  if (f12 != nullptr && f12->outer() == e12) {
    f12->setOuterComponent(e10);
  }

  if (f21 != nullptr && f21->outer() == e21) {
    f21->setOuterComponent(e20);
  }

  _halfedges.push_back(e10);
  _halfedges.push_back(e02);
  _halfedges.push_back(e20);
  _halfedges.push_back(e01);

  _halfedges.remove(e12);
  _halfedges.remove(e21);
  delete e12;
  delete e21;

  // Invalidate cached loop direction: the incident edge may have changed, so
  // any previously computed _direction value is potentially stale.
  if (hel12 != nullptr) hel12->setDirection(0);
  if (hel21 != nullptr) hel21->setDirection(0);

  return v0;
}

PDCELHalfEdge *PDCEL::addEdge(PDCELVertex *v1, PDCELVertex *v2) {
  // addVertex returns the vertex actually used: either v1/v2 themselves (if
  // new) or an existing coincident vertex from the set.  Rebind so that the
  // edge connects the canonical vertices.
  v1 = addVertex(v1);
  v2 = addVertex(v2);

  PGeoLineSegment *ls = new PGeoLineSegment(v1, v2);
  _owned_segments.push_back(std::unique_ptr<PGeoLineSegment>(ls));

  PDCELHalfEdge *he12 = new PDCELHalfEdge(v1, 1);
  PDCELHalfEdge *he21 = new PDCELHalfEdge(v2, -1);
  const unsigned int lineage_id = allocateEdgeLineageId();
  he12->setId(allocateHalfEdgeId());
  he21->setId(allocateHalfEdgeId());

  he12->setLineSegment(ls);
  he21->setLineSegment(ls);
  he12->setLineageId(lineage_id);
  he21->setLineageId(lineage_id);

  ls->setHalfEdge(he12);
  ls->setHalfEdge(he21);

  he12->setTwin(he21);
  he21->setTwin(he12);

  updateEdgeNeighbors(he12);
  updateEdgeNeighbors(he21);

  _halfedges.push_back(he12);
  _halfedges.push_back(he21);

  return he12;
}

PDCELHalfEdge *PDCEL::addEdge(PGeoLineSegment *ls) {
  PDCELHalfEdge *he12 = new PDCELHalfEdge(ls->v1(), 1);
  PDCELHalfEdge *he21 = new PDCELHalfEdge(ls->v2(), -1);
  const unsigned int lineage_id = allocateEdgeLineageId();
  he12->setId(allocateHalfEdgeId());
  he21->setId(allocateHalfEdgeId());

  if (ls->v1()->edge() == nullptr) {
    ls->v1()->setIncidentEdge(he12);
  }

  if (ls->v2()->edge() == nullptr) {
    ls->v2()->setIncidentEdge(he21);
  }

  ls->setHalfEdge(he12);
  ls->setHalfEdge(he21);

  he12->setLineSegment(ls);
  he21->setLineSegment(ls);
  he12->setLineageId(lineage_id);
  he21->setLineageId(lineage_id);

  he12->setTwin(he21);
  he21->setTwin(he12);

  updateEdgeNeighbors(he12);
  updateEdgeNeighbors(he21);

  _halfedges.push_back(he12);
  _halfedges.push_back(he21);

  return he12;
}

void PDCEL::removeEdge(PDCELHalfEdge *he) {
  PDCELHalfEdge *he2 = he->twin();

  // Update face incident edges
  if (he->face()) {
    if (he->face()->outer() == he) {
      if (he->next()) {
        he->face()->setOuterComponent(he->next());
      } else if (he->prev()) {
        he->face()->setOuterComponent(he->prev());
      } else {
        he->face()->setOuterComponent(nullptr);
      }
    }

    for (size_t i = 0; i < he->face()->inners().size(); i++) {
      if (he->face()->inners()[i] == he) {
        if (he->next()) {
          he->face()->inners()[i] = he->next();
        } else if (he->prev()) {
          he->face()->inners()[i] = he->prev();
        } else {
          he->face()->inners()[i] = nullptr;
        }
        break;
      }
    }
  }

  if (he2->face()) {
    if (he2->face()->outer() == he2) {
      if (he2->next()) {
        he2->face()->setOuterComponent(he2->next());
      } else if (he2->prev()) {
        he2->face()->setOuterComponent(he2->prev());
      } else {
        he2->face()->setOuterComponent(nullptr);
      }
    }

    for (size_t i = 0; i < he2->face()->inners().size(); i++) {
      if (he2->face()->inners()[i] == he2) {
        if (he2->next()) {
          he2->face()->inners()[i] = he2->next();
        } else if (he2->prev()) {
          he2->face()->inners()[i] = he2->prev();
        } else {
          he2->face()->inners()[i] = nullptr;
        }
        break;
      }
    }
  }

  // Update loop incident edge
  if (he->loop()) {
    if (he->loop()->incidentEdge() == he) {
      if (he->next()) {
        he->loop()->setIncidentEdge(he->next());
      } else if (he->prev()) {
        he->loop()->setIncidentEdge(he->prev());
      } else {
        he->loop()->setIncidentEdge(nullptr);
      }
    }
  }

  if (he2->loop()) {
    if (he2->loop()->incidentEdge() == he2) {
      if (he2->next()) {
        he2->loop()->setIncidentEdge(he2->next());
      } else if (he2->prev()) {
        he2->loop()->setIncidentEdge(he2->prev());
      } else {
        he2->loop()->setIncidentEdge(nullptr);
      }
    }
  }

  // Update prev and next edges
  if (he->prev() && he->next()) {
    he->prev()->setNext(he->next());
    he->next()->setPrev(he->prev());
  }

  if (he2->prev() && he2->next()) {
    he2->prev()->setNext(he2->next());
    he2->next()->setPrev(he2->prev());
  }

  // Update vertex incident edge
  if (he->source()->edge() == he) {
    if (he->twin()->next()) {
      he->source()->setIncidentEdge(he->twin()->next());
    } else if (he->prev()) {
      he->source()->setIncidentEdge(he->prev()->twin());
    } else {
      he->source()->setIncidentEdge(nullptr);
    }
  }

  if (he2->source()->edge() == he2) {
    if (he2->twin()->next()) {
      he2->source()->setIncidentEdge(he2->twin()->next());
    } else if (he2->prev()) {
      he2->source()->setIncidentEdge(he2->prev()->twin());
    } else {
      he2->source()->setIncidentEdge(nullptr);
    }
  }

  // Merge source and target vertices
  PDCELVertex *vs = he->source();
  PDCELVertex *vt = he->target();
  PDCELHalfEdge *vs_replacement = nullptr;
  if (vt->edge() != nullptr && vt->edge() != he2) {
    vs_replacement = vt->edge();
  }
  PDCELHalfEdge *_he_tmp = vt->edge();
  if (_he_tmp != nullptr) {
    do {
      _he_tmp->setSource(vs);
      _he_tmp = _he_tmp->twin();
      _he_tmp = _he_tmp->next();
    } while (_he_tmp != nullptr && _he_tmp != vt->edge());
  }

  // All edges formerly incident on vt have been redirected to vs.
  // Clear vt's incident edge so removeVertex's precondition passes.
  if (vs->edge() == he || vs->edge() == he2) {
    vs->setIncidentEdge(vs_replacement);
  }
  vt->setIncidentEdge(nullptr);
  removeVertex(vt);

  _halfedges.remove(he);
  _halfedges.remove(he2);
  delete he;
  delete he2;
}

PDCELHalfEdge *PDCEL::findHalfEdgeBetween(PDCELVertex *v1, PDCELVertex *v2) {
  return const_cast<PDCELHalfEdge *>(
      static_cast<const PDCEL *>(this)->findHalfEdgeBetween(v1, v2));
}

PDCELHalfEdge *PDCEL::findHalfEdgeBetween(PDCELVertex *v1,
                                          PDCELVertex *v2) const {
  if (v1->edge() == nullptr) {
    return nullptr;
  }

  PDCELHalfEdge *he1 = v1->edge();
  PDCELHalfEdge *heit = he1;
  do {
    if (heit->target() == v2) {
      return heit;
    }
    if (heit->twin() == nullptr) break;
    heit = heit->twin()->next();
  } while (heit != nullptr && heit != he1);

  return nullptr;
}

void PDCEL::addEdgesFromCurve(const std::vector<PDCELVertex *> &vertices) {
  if (vertices.size() < 2) {
    PLOG_DEBUG_AT(join) << "addEdgesFromCurve: skipping because the curve"
                        << " has fewer than two vertices ("
                        << vertices.size() << ")";
    return;
  }
  for (std::size_t i = 0; i < vertices.size() - 1; ++i) {
    addEdge(vertices[i], vertices[i + 1]);
  }
}

// ===================================================================
//
// Half Edge Loop
//
// ===================================================================

int PDCEL::isOuterOrInnerBoundary(PDCELHalfEdge *he1, PDCELHalfEdge *he2) {
  SVector3 sv1 = he1->toVector();
  SVector3 sv2 = he2->toVector();
  SVector3 sv0 = crossprod(sv1, sv2);

  if (sv0.x() > 0) return 1;
  if (sv0.x() < 0) return -1;
  return 0;
}

PDCELHalfEdgeLoop *PDCEL::addHalfEdgeLoop(PDCELHalfEdge *he) {
  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();
  hel->setId(allocateLoopId());

  PDCELHalfEdge *hei = he;
  std::vector<PDCELHalfEdge *> visited;
  do {
    hei->setLoop(hel);
    hel->updateIncidentEdge(hei);
    visited.push_back(hei);
    hei = hei->next();
  } while (hei != nullptr && hei != he);

  if (hei == nullptr) {
    PLOG(warning) << "addHalfEdgeLoop: broken half-edge cycle detected; "
                     "loop discarded";
    for (auto h : visited) h->resetLoop();
    delete hel;
    return nullptr;
  }

  _halfedge_loops.push_back(hel);

  return hel;
}

void PDCEL::removeHalfEdgeLoop(PDCELHalfEdgeLoop *hel) {
  PDCELHalfEdge *he = hel->incidentEdge();
  if (he != nullptr) {
    walkLoopWithLimit(he, [](PDCELHalfEdge *h) { h->resetLoop(); });
  }

  _halfedge_loops.remove(hel);
  _loop_keep.erase(hel);
  _loop_adjacent.erase(hel);

  delete hel;
}


bool PDCEL::isLoopKept(PDCELHalfEdgeLoop *hel) const {
  auto it = _loop_keep.find(hel);
  return it != _loop_keep.end() && it->second;
}

void PDCEL::setLoopKept(PDCELHalfEdgeLoop *hel, bool kept) {
  _loop_keep[hel] = kept;
}

PDCELHalfEdgeLoop *PDCEL::adjacentLoop(PDCELHalfEdgeLoop *hel) const {
  auto it = _loop_adjacent.find(hel);
  return it != _loop_adjacent.end() ? it->second : nullptr;
}

void PDCEL::setAdjacentLoop(PDCELHalfEdgeLoop *hel, PDCELHalfEdgeLoop *adj) {
  _loop_adjacent[hel] = adj;
}


PDCELHalfEdgeLoop *PDCEL::findNearestLoop(PDCELHalfEdgeLoop *loop) {
  if (_halfedge_loops.empty()) return nullptr;

  PDCELHalfEdgeLoop *loop_near = _halfedge_loops.front();

  PDCELHalfEdge *he = ::findHalfEdgeBelowVertex(*this,
                                                loop->bottomLeftVertex());
  if (he != nullptr) {
    loop_near = he->loop();
  }

  return loop_near;
}

void PDCEL::removeTempLoops() {
  // Remove all half edge loops, excluding segments face boundaries
  for (auto he : _halfedges) {
    if ((he->loop() != nullptr) && !isLoopKept(he->loop())) {
      he->resetLoop();
    }
  }

  std::vector<PDCELHalfEdgeLoop *> loops_to_remove;
  for (auto hel : _halfedge_loops) {
    if (!isLoopKept(hel)) {
      loops_to_remove.push_back(hel);
    }
  }

  for (auto hel : loops_to_remove) {
    _halfedge_loops.remove(hel);
    _loop_keep.erase(hel);
    _loop_adjacent.erase(hel);
    delete hel;
  }
}

void PDCEL::createTempLoops() {
  // Create new half edge loops, excluding segments face boundaries
  for (auto he : _halfedges) {
    if (he->loop() == nullptr) {
      PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();
      hel->setId(allocateLoopId());
      PDCELHalfEdge *hei = he;
      std::vector<PDCELHalfEdge *> visited;
      do {
        hei->setLoop(hel);
        visited.push_back(hei);
        hei = hei->next();
      } while (hei != nullptr && hei != he);

      if (hei == nullptr) {
        PLOG(warning) << "createTempLoops: broken half-edge cycle detected; "
                         "loop discarded";
        for (auto h : visited) h->resetLoop();
        delete hel;
      } else {
        _halfedge_loops.push_back(hel);
      }
    }
  }
}

void PDCEL::linkHalfEdgeLoops() {
  for (auto loop : _halfedge_loops) {
    if (loop->direction() < 0) {
      // Each inner loop should have a path linking to an outer loop
      PDCELHalfEdgeLoop *hel = findNearestLoop(loop);
      if (hel != nullptr) {
        setAdjacentLoop(loop, hel);
      }
    }
  }
}

PDCELHalfEdgeLoop *PDCEL::findEnclosingLoop(PDCELVertex *v) {
  if (_halfedge_loops.empty()) return nullptr;

  PDCELHalfEdgeLoop *loop_near = _halfedge_loops.front();

  PDCELHalfEdge *he = ::findHalfEdgeBelowVertex(*this, v);
  if (he != nullptr) {
    loop_near = he->loop();

    while (adjacentLoop(loop_near) != nullptr) {
      // Need to return the outer loop
      loop_near = adjacentLoop(loop_near);
    }
  }

  return loop_near;
}

void PDCEL::findCurvesIntersection(PDCELHalfEdgeLoop *hel,
                                   PGeoLineSegment *ls) {
  PDCELHalfEdge *hei = hel->incidentEdge(), *he1 = nullptr, *he2 = nullptr;
  PGeoLineSegment *lsi;
  bool not_parallel;
  double u_ls, u_lsi;
  PDCELVertex *v_tmp, *v1 = nullptr, *v2 = nullptr;
  std::list<PDCELVertex *> vlist1, vlist2;

  int _iter = 0;
  do {
    if (++_iter > kDCELLoopHardCap) {
      throw std::runtime_error(
          "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
          " iterations in findCurvesIntersection at " +
          hel->incidentEdge()->printString());
    }
    lsi = hei->toLineSegment();

    not_parallel = calcLineIntersection2D(lsi, ls, u_lsi, u_ls, TOLERANCE);
    if (!not_parallel) {
      if (!isCollinear(lsi, ls)) {
        hei = hei->next();
        continue;
      }

      if (vlist1.empty()) {
        vlist1.push_back(lsi->v1());
        vlist1.push_back(lsi->v2());
        he1 = hei;
      } else {
        vlist2.push_back(lsi->v1());
        vlist2.push_back(lsi->v2());
        he2 = hei;
      }
    } else {
      if (u_lsi >= 0 && u_lsi < 1) {
        if (u_lsi < TOLERANCE) {
          v_tmp = lsi->v1();
        } else if (1 - u_lsi < TOLERANCE) {
          v_tmp = lsi->v2();
        } else {
          v_tmp = lsi->getParametricVertex(u_lsi);
        }

        if (!isInContainer(vlist1, v_tmp) && !isInContainer(vlist2, v_tmp)) {
          if (vlist1.empty()) {
            vlist1.push_back(v_tmp);
            he1 = hei;
          } else {
            vlist2.push_back(v_tmp);
            he2 = hei;
          }
        }
      } else {
        hei = hei->next();
        continue;
      }
    }

    hei = hei->next();
  } while (hei != hel->incidentEdge());

  // Find the closest two vertices, v1 from vlist1 and v2 from vlist2
  if (vlist1.size() == 1) {
    v1 = vlist1.front();
  } else if (vlist1.size() > 1) {
    double d2{INF}, d2_tmp;
    if (vlist2.empty()) {
      // Only one intersection side found; take the first candidate.
      v1 = vlist1.front();
    } else {
      for (auto v : vlist1) {
        d2_tmp = calcDistanceSquared(v, vlist2.front());
        if (d2_tmp < d2) {
          d2 = d2_tmp;
          v1 = v;
        }
      }
    }
  }

  if (vlist2.size() == 1) {
    v2 = vlist2.front();
  } else if (vlist2.size() > 1) {
    double d2{INF}, d2_tmp;
    for (auto v : vlist2) {
      d2_tmp = calcDistanceSquared(v, v1);
      if (d2_tmp < d2) {
        d2 = d2_tmp;
        v2 = v;
      }
    }
  }

  // Guard: both intersection vertices must be found before proceeding.
  if (v1 == nullptr || v2 == nullptr) {
    PLOG(warning) << "findCurvesIntersection: fewer than two intersections "
                     "found; skipping edge split and segment update";
    return;
  }

  // Split half edges if necessary
  if (v1->degree() == 0) {
    v1 = splitEdge(he1, v1);
  }

  if (v2->degree() == 0) {
    v2 = splitEdge(he2, v2);
  }

  // Update the line segment, keeping the same orientation
  SVector3 vec_old, vec_new;
  vec_old = ls->toVector();
  vec_new = SVector3(v1->point(), v2->point());

  if (dot(vec_old, vec_new) > 0) {
    // Same orientation
    ls->setV1(v1);
    ls->setV2(v2);
  } else {
    ls->setV1(v2);
    ls->setV2(v1);
  }
}

// ===================================================================
//
// Face
//
// ===================================================================

PDCELFace *PDCEL::addFace(PDCELHalfEdgeLoop *hel) {
  PDCELFace *fnew = new PDCELFace();
  fnew->setId(allocateFaceId());

  fnew->setOuterComponent(hel->incidentEdge());
  walkLoopWithLimit(hel->incidentEdge(), [fnew](PDCELHalfEdge *hei) {
    hei->setIncidentFace(fnew);
  });

  _faces.push_back(fnew);

  return fnew;
}

PDCELFace *PDCEL::addFace(const std::list<PDCELVertex *> &vloop, PDCELFace *f) {
  if (vloop.size() < 2) {
    PLOG_DEBUG_AT(join) << "addFace(vloop): skipping because the vertex loop"
                        << " has fewer than two vertices (" << vloop.size()
                        << ")";
    return nullptr;
  }

  if (f == nullptr) {
    // insert into the background unbounded face
    f = _faces.front();
  }

  // ── Phase 1: resolve canonical vertices ──────────────────────────────────
  // addVertex never corrupts topology, so vertex insertions are safe here.
  std::vector<PDCELVertex *> cverts;
  cverts.reserve(vloop.size());
  for (auto vraw : vloop) {
    cverts.push_back(addVertex(vraw));
  }

  // ── Phase 2: pre-allocate new half-edge pairs ─────────────────────────────
  // All new PDCELHalfEdge allocations happen before any prev/next/face
  // mutations.  If any allocation throws, no topology has been patched.
  struct EdgeSlot {
    PDCELVertex  *v1, *v2;
    PDCELHalfEdge *he12, *he21;
    bool is_new;
  };
  std::vector<EdgeSlot> slots;
  slots.reserve(cverts.size() - 1);
  for (std::size_t i = 0; i + 1 < cverts.size(); ++i) {
    PDCELVertex  *v1  = cverts[i];
    PDCELVertex  *v2  = cverts[i + 1];
    PDCELHalfEdge *he12 = findHalfEdgeBetween(v1, v2);
    bool is_new = (he12 == nullptr);
    PDCELHalfEdge *he21;
    if (is_new) {
      he12 = new PDCELHalfEdge(v1, 1);
      he21 = new PDCELHalfEdge(v2, -1);
      he12->setId(allocateHalfEdgeId());
      he21->setId(allocateHalfEdgeId());
      he12->setTwin(he21);
      he21->setTwin(he12);
    } else {
      he21 = he12->twin();
    }
    slots.push_back({v1, v2, he12, he21, is_new});
  }

  // ── Phase 3: topology mutations ───────────────────────────────────────────
  // All allocations succeeded; safe to patch prev/next/face/incidentEdge.
  PDCELFace *fnew = new PDCELFace();
  fnew->setId(allocateFaceId());
  std::list<PDCELFace *> other_faces;

  PDCELHalfEdge *head = nullptr, *he12prev = nullptr, *he21next = nullptr;
  for (auto &s : slots) {
    PDCELHalfEdge *he12 = s.he12, *he21 = s.he21;
    if (s.is_new) {
      if (s.v1->edge() == nullptr) s.v1->setIncidentEdge(he12);
      if (s.v2->edge() == nullptr) s.v2->setIncidentEdge(he21);
      he21->setIncidentFace(f);
      if (he12prev != nullptr) {
        he12->setPrev(he12prev);
        he12prev->setNext(he12);
        he21->setNext(he21next);
        he21next->setPrev(he21);
      }
      _halfedges.push_back(he12);
      _halfedges.push_back(he21);
    } else {
      // Record all neighboring faces that are not the background f
      if (he21->face() != f) {
        other_faces.push_back(he21->face());
      }
      // Update the incident edge of any inner hole of f
      for (std::size_t i = 0; i < f->inners().size(); ++i) {
        if (f->inners()[i] == he12) {
          f->inners()[i] = he12->next();
        }
      }
      if (he12prev != nullptr) {
        he12->setPrev(he12prev);
        he12prev->setNext(he12);
        if (he21->face() == f) {
          he21->setNext(he21next);
          he21next->setPrev(he21);
        }
      }
    }

    he12->setIncidentFace(fnew);
    if (fnew->outer() == nullptr) {
      fnew->setOuterComponent(he12);
    }
    he12prev = he12;
    he21next = he21;
    if (head == nullptr) head = he12;
  }

  head->setPrev(he12prev);
  he12prev->setNext(head);
  if (head->twin()->face() == f) {
    head->twin()->setNext(he21next);
    he21next->setPrev(head->twin());
  }

  _faces.push_back(fnew);

  // For all other faces, update the corresponding prev and next
  other_faces.unique();
  for (auto of : other_faces) {
    PDCELHalfEdge *out_he = of->outer(), *out_he_next;
    int _iter = 0;
    do {
      if (++_iter > kDCELLoopHardCap) {
        throw std::runtime_error(
            "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
            " iterations in addFace (other_faces) at " +
            of->outer()->printString());
      }
      out_he_next = out_he->next();
      if ((out_he->twin()->face() == fnew) &&
          (out_he_next->twin()->face() == f)) {
        out_he_next->twin()->setNext(out_he->twin()->prev()->twin());
        out_he->twin()->prev()->twin()->setPrev(out_he_next->twin());
      } else if ((out_he->twin()->face() == f) &&
                 (out_he_next->twin()->face() == fnew)) {
        out_he->twin()->setPrev(out_he_next->twin()->next()->twin());
        out_he_next->twin()->next()->twin()->setNext(out_he->twin());
      }
      out_he = out_he->next();
    } while (out_he != of->outer());
  }

  return fnew;
}

std::list<PDCELFace *> PDCEL::splitFace(PDCELFace *f, PDCELVertex *v1,
                                        PDCELVertex *v2) {
  // Precondition: f must be a bounded face with a fully wired outer boundary.
  if (f->outer() == nullptr) {
    PLOG(error) << "splitFace: face has no outer boundary";
    return {};
  }
  if (f->outer()->loop() == nullptr) {
    PLOG(error) << "splitFace: face outer boundary has no loop";
    return {};
  }

  PDCELHalfEdge *he12, *he21;
  PDCELHalfEdgeLoop *hel = f->outer()->loop();

  he12 = addEdge(v1, v2);
  he21 = he12->twin();

  PDCELHalfEdgeLoop *hel12 = addHalfEdgeLoop(he12);
  PDCELHalfEdgeLoop *hel21 = addHalfEdgeLoop(he21);

  PDCELFace *f12 = addFace(hel12);
  PDCELFace *f21 = addFace(hel21);

  _faces.remove(f);
  delete f;

  _halfedge_loops.remove(hel);
  delete hel;

  std::list<PDCELFace *> new_faces;
  new_faces.push_back(f12);
  new_faces.push_back(f21);

  return new_faces;
}

std::list<PDCELFace *> PDCEL::splitFace(PDCELFace *f, PGeoLineSegment *ls) {
  // Precondition: f must be a bounded face with a fully wired outer boundary.
  if (f->outer() == nullptr) {
    PLOG(error) << "splitFace: face has no outer boundary";
    return {};
  }
  if (f->outer()->loop() == nullptr) {
    PLOG(error) << "splitFace: face outer boundary has no loop";
    return {};
  }

  // Find the two intersections
  findCurvesIntersection(f->outer()->loop(), ls);

  return splitFace(f, ls->v1(), ls->v2());
}

// ===================================================================
//
// Private helper functions
//
// ===================================================================

void PDCEL::updateEdgeNeighbors(PDCELHalfEdge *he) {
  PDCELVertex *v = he->source();
  PDCELHalfEdge *het = he->twin();

  if (v->degree() == 0) {
    v->setIncidentEdge(he);
  } else if (v->degree() == 1) {
    warnIfAnglesTooClose(v, he, v->edge());
    he->setPrev(v->edge()->twin());
    v->edge()->twin()->setNext(he);
    het->setNext(v->edge());
    v->edge()->setPrev(het);
  } else {
    // Two or more edges already at v.
    // Precondition: all half-edges in the cycle around v are fully wired —
    // prev() and twin() are non-null and form a closed ring.
    std::list<PDCELHalfEdge *> cycle_list, cycle_list_2;
    PDCELHalfEdge *hei;
    int list_num = 1;

    hei = v->edge();
    cycle_list.push_back(hei);
    if (hei->prev() == nullptr || hei->prev()->twin() == nullptr) {
      PLOG(error) << "updateEdgeNeighbors: broken half-edge cycle at vertex "
                  << v->name();
      return;
    }
    hei = hei->prev()->twin();
    int _iter = 0;
    do {
      if (++_iter > kDCELLoopHardCap) {
        throw std::runtime_error(
            "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
            " iterations in updateEdgeNeighbors at vertex " + v->name());
      }
      if (hei->angle() < cycle_list.back()->angle()) {
        list_num = 2;
      }
      if (list_num == 1) {
        cycle_list.push_back(hei);
      } else if (list_num == 2) {
        cycle_list_2.push_back(hei);
      }
      if (hei->prev() == nullptr || hei->prev()->twin() == nullptr) {
        PLOG(error) << "updateEdgeNeighbors: broken half-edge cycle at vertex "
                    << v->name();
        return;
      }
      hei = hei->prev()->twin();
    } while (hei != v->edge());

    if (cycle_list_2.size() > 0) {
      cycle_list.splice(cycle_list.begin(), cycle_list_2);
      // Now the list stores leaving edges sorted from (-pi, pi]
    }

    // Insert the new edge according to the angle
    std::list<PDCELHalfEdge *>::iterator it;
    for (it = cycle_list.begin(); it != cycle_list.end(); ++it) {
      if (he->angle() < (*it)->angle())
        break;
    }
    cycle_list.insert(it, he);

    // Update prev and next
    PDCELHalfEdge *he12left;
    if (it == cycle_list.end()) {
      he12left = cycle_list.front();
    } else {
      he12left = *it;
    }

    warnIfAnglesTooClose(v, he, he12left);
    if (he12left->twin() != nullptr && he12left->twin()->next() != nullptr) {
      warnIfAnglesTooClose(v, he, he12left->twin()->next());
    }

    he->setPrev(he12left->twin());
    het->setNext(he12left->twin()->next());
    he12left->twin()->next()->setPrev(het);
    he12left->twin()->setNext(he);
  }
}

