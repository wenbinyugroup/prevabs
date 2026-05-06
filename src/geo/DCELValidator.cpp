#include "DCELValidator.hpp"

#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "geo.hpp"
#include "plog.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

bool validateDCEL(const PDCEL &dcel) {
  bool ok = true;
  const std::size_t limit = dcel.halfedges().size() + 1;

  for (PDCELHalfEdge *he : dcel.halfedges()) {
    if (he->twin() == nullptr) {
      PLOG(warning) << "validate: half-edge " << he << " has null twin";
      ok = false;
      continue;
    }

    if (he->twin()->twin() != he) {
      PLOG(warning) << "validate: half-edge " << he << " twin->twin != he";
      ok = false;
    }

    if (he->next() != nullptr && he->next()->prev() != he) {
      PLOG(warning) << "validate: half-edge " << he << " next->prev != he";
      ok = false;
    }

    if (he->prev() != nullptr && he->prev()->next() != he) {
      PLOG(warning) << "validate: half-edge " << he << " prev->next != he";
      ok = false;
    }

    if (he->next() != nullptr && he->next()->source() != he->target()) {
      PLOG(warning) << "validate: half-edge " << he
                    << " next->source != he->target";
      ok = false;
    }

    if (he->source() == he->target()) {
      PLOG(warning) << "validate: half-edge " << he
                    << " is a self-loop (source == target)";
      ok = false;
    }

    if (he->face() == nullptr) {
      PLOG(warning) << "validate: half-edge " << he
                    << " has null incident face (orphaned)";
      ok = false;
    }
  }

  {
    std::unordered_map<PDCELVertex *, std::vector<PDCELVertex *> > seen;
    for (PDCELHalfEdge *he : dcel.halfedges()) {
      PDCELVertex *src = he->source();
      PDCELVertex *tgt = he->target();
      std::vector<PDCELVertex *> &nbrs = seen[src];
      if (std::find(nbrs.begin(), nbrs.end(), tgt) != nbrs.end()) {
        PLOG(warning) << "validate: multi-edge detected from vertex "
                      << src << " to vertex " << tgt;
        ok = false;
      } else {
        nbrs.push_back(tgt);
      }
    }
  }

  for (PDCELVertex *v : dcel.vertices()) {
    if (v->edge() == nullptr) {
      PLOG(warning) << "validate: vertex " << v
                    << " is isolated (no incident edge)";
      ok = false;
    }
  }

  for (PDCELFace *f : dcel.faces()) {
    if (f->outer() == nullptr) {
      continue;
    }

    PDCELHalfEdge *start = f->outer();
    PDCELHalfEdge *hei = start;
    std::size_t count = 0;
    do {
      if (hei == nullptr) {
        PLOG(warning) << "validate: face outer loop broken (null next)";
        ok = false;
        break;
      }
      if (hei->face() != f) {
        PLOG(warning) << "validate: face outer loop contains half-edge "
                         "with wrong incident face";
        ok = false;
      }
      hei = hei->next();
      if (++count > limit) {
        PLOG(warning) << "validate: face outer loop does not close";
        ok = false;
        break;
      }
    } while (hei != start);
  }

  return ok;
}

void fixDCELGeometry(PDCEL &dcel, const BuilderConfig &bcfg) {
    PLOG(info) << "fixing geometry";

  std::unordered_set<PDCELHalfEdge *> small_edges;
  for (PDCELHalfEdge *he : dcel.halfedges()) {
    if (small_edges.count(he->twin())) {
      continue;
    }

    double sqlen = calcDistanceSquared(he->source(), he->target());
    if (sqlen <= bcfg.geo_tol * bcfg.geo_tol) {
      small_edges.insert(he);
    }
  }

  for (std::unordered_set<PDCELHalfEdge *>::const_iterator it =
           small_edges.begin(); it != small_edges.end(); ++it) {
    dcel.removeEdge(*it);
  }

  std::vector<PDCELVertex *> orphan_vertices;
  for (PDCELVertex *vertex : dcel.vertices()) {
    if (vertex->edge() == nullptr) {
      orphan_vertices.push_back(vertex);
    }
  }

  for (PDCELVertex *vertex : orphan_vertices) {
    dcel.removeVertex(vertex);
  }

  PLOG(info) << "fixed geometry: removed "
             << small_edges.size() << " degenerate edges, "
             << orphan_vertices.size() << " orphan verts; final dcel: "
             << dcel.vertices().size() << " verts / "
             << (dcel.halfedges().size() / 2) << " edges / "
             << dcel.faces().size() << " faces";
}
