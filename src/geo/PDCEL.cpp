#include "PDCEL.hpp"

#include "PBST.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <iterator>
#include <limits>
#include <list>
#include <vector>

#include <typeinfo>

PDCEL::~PDCEL() {
  for (auto v : _vertices)
    delete v;
  for (auto e : _halfedges)
    delete e;
  for (auto f : _faces)
    delete f;
}









void PDCEL::initialize() {
  // Create the infinity large bounding box
  PDCELVertex *v_tr = new PDCELVertex(0, INF, INF, false);
  PDCELVertex *v_tl = new PDCELVertex(0, -INF, INF, false);
  PDCELVertex *v_bl = new PDCELVertex(0, -INF, -INF, false);
  PDCELVertex *v_br = new PDCELVertex(0, INF, -INF, false);

  PGeoLineSegment *ls_top = new PGeoLineSegment(v_tr, v_tl);
  PGeoLineSegment *ls_left = new PGeoLineSegment(v_tl, v_bl);
  PGeoLineSegment *ls_bottom = new PGeoLineSegment(v_bl, v_br);
  PGeoLineSegment *ls_right = new PGeoLineSegment(v_br, v_tr);

  PDCELHalfEdge *he_top = addEdge(ls_top);
  PDCELHalfEdge *he_left = addEdge(ls_left);
  PDCELHalfEdge *he_bottom = addEdge(ls_bottom);
  PDCELHalfEdge *he_right = addEdge(ls_right);

  PDCELHalfEdgeLoop *hel = addHalfEdgeLoop(he_top);

  PDCELFace *f = new PDCELFace(he_top, false); // The unbounded face
  f->setName("background");

  _vertex_tree = new PAVLTreeVertex();

  hel->setDirection(1);
  hel->setFace(f);
  hel->setKeep(true);

  _vertices.push_back(v_tr);
  _vertices.push_back(v_tl);
  _vertices.push_back(v_bl);
  _vertices.push_back(v_br);

  _halfedges.remove(he_top->twin());
  _halfedges.remove(he_left->twin());
  _halfedges.remove(he_bottom->twin());
  _halfedges.remove(he_right->twin());

  _faces.push_back(f);

  // _vertex_tree->insert(v_tr);
  // _vertex_tree->insert(v_tl);
  // _vertex_tree->insert(v_bl);
  // _vertex_tree->insert(v_br);

  // print_dcel();
}









void PDCEL::print_dcel() {
  std::cout << std::endl;
  // std::cout << "DCEL Summary" << std::endl;
  PLOG(debug) << "DCEL Summary";
  std::cout << std::endl;


  std::list<PDCELVertex *>::iterator vit;
  // std::cout << _vertices.size() << " vertices:" << std::endl;
  PLOG(debug) << _vertices.size() << " vertices:";
  for (vit = _vertices.begin(); vit != _vertices.end(); ++vit) {
    // std::cout << (*vit) << " - degree " << (*vit)->degree() << std::endl;
    PLOG(debug) << (*vit)->printString() << " - degree " << (*vit)->degree();
  }
  std::cout << std::endl;


  std::list<PDCELHalfEdge *>::iterator eit;
  // std::cout << _halfedges.size() << " half edges:" << std::endl;
  PLOG(debug) << _halfedges.size() << " half edges:";
  for (eit = _halfedges.begin(); eit != _halfedges.end(); ++eit) {
    // std::cout << (*eit)->source() << " -> " << (*eit)->target() << std::endl;
    // (*eit)->print2();
    PLOG(debug) << (*eit)->printString();
  }
  std::cout << std::endl;


  std::list<PDCELHalfEdgeLoop *>::iterator lit;
  // std::cout << _halfedge_loops.size() << " half edge loops:" << std::endl;
  PLOG(debug) << _halfedge_loops.size() << " half edge loops:";
  for (lit = _halfedge_loops.begin(); lit != _halfedge_loops.end(); ++lit) {
    // (*lit)->print();
    (*lit)->log();
  }
  std::cout << std::endl;


  std::list<PDCELFace *>::iterator fit;
  std::cout << _faces.size() << " faces:" << std::endl;
  for (fit = _faces.begin(); fit != _faces.end(); ++fit) {
    (*fit)->print();
  }
  std::cout << std::endl;
}









void PDCEL::fixGeometry(Message *pmessage) {
  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("fixing geometry");
  // Remove very small edges (very close points)
  // double tol{1e-6};

  // Find all small edges
  std::vector<PDCELHalfEdge *> small_edges;
  for (auto he : _halfedges) {
    bool found = false;
    for (auto she : small_edges) {
      if (he->twin() == she) {
        found = true;
        break;
      }
    }
    if (found) continue;

    // Calculate edge length
    double sqlen = calcDistanceSquared(he->source(), he->target());
    if (sqlen <= config.geo_tol*config.geo_tol) {
      small_edges.push_back(he);
    }
  }

  // Remove small edges
  for (auto he : small_edges) {
    removeEdge(he);
  }

  // std::cout << "after removing small edges\n";
  // for (auto he : _halfedges) {
  //   double sqlen = calcDistanceSquared(he->source(), he->target());
  //   if (sqlen <= tol*tol) {
  //     std::cout << he << std::endl;
  //   }
  // }
  pmessage->decreaseIndent();
}









// ===================================================================
//
// Vertex
//
// ===================================================================

void PDCEL::addVertex(PDCELVertex *v) {
  if (v->dcel() == nullptr) {
    // std::cout << "adding new vertex " << v << std::endl;
    _vertices.push_back(v);
    _vertex_tree->insert(v);
    v->setDCEL(this);
  }
}









void PDCEL::removeVertex(PDCELVertex *v) {
  if (v->dcel() != nullptr) {
    _vertices.remove(v);
    _vertex_tree->remove(v);
    v->setDCEL(nullptr);
  }
}









// ===================================================================
//
// Half Edge
//
// ===================================================================

PDCELHalfEdge *PDCEL::findHalfEdge(PDCELVertex *v, PDCELFace *f) {
  PDCELHalfEdge *he;

  he = v->edge();
  do {
    if (he->face() == f) {
      return he;
    }
    he = he->twin()->next();
  } while (he != v->edge() && he != nullptr);

  return nullptr;
}









void PDCEL::splitEdge(PDCELHalfEdge *e12, PDCELVertex *v0) {
  //          e12
  //          -->
  //     e10      e02
  //     -->      -->
  // v1 ----- v0 ----- v2
  //     <--      <--
  //     e01      e20
  //         <--
  //         e21

  // std::cout << "[debug] splitEdge:" << std::endl;
  // std::cout << "        half edge e12: " << e12 << std::endl;
  // std::cout << "        vertex v0: " << v0 << std::endl;

  PDCELHalfEdge *e21 = e12->twin();

  PDCELVertex *v1 = e12->source();
  PDCELVertex *v2 = e21->source();
  PDCELHalfEdgeLoop *hel12 = e12->loop();
  PDCELHalfEdgeLoop *hel21 = e21->loop();
  PDCELFace *f12 = e12->face();
  PDCELFace *f21 = e21->face();

  // std::cout << "        create new edges" << std::endl;
  PDCELHalfEdge *e10 = new PDCELHalfEdge(v1, 1);
  PDCELHalfEdge *e01 = new PDCELHalfEdge(v0, -1);

  PDCELHalfEdge *e02 = new PDCELHalfEdge(v0, 1);
  PDCELHalfEdge *e20 = new PDCELHalfEdge(v2, -1);

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

  // Update prev and next
  // std::cout << "        update prevs and nexts" << std::endl;
  e12->prev()->setNext(e10);
  e12->next()->setPrev(e02);
  e21->prev()->setNext(e20);
  e21->next()->setPrev(e01);

  // Update vertex incident half edge if necessary
  // std::cout << "        update vertex incident half edge" << std::endl;
  if (v1->edge() == e12) {
    v1->setIncidentEdge(e10);
  }

  if (v2->edge() == e21) {
    v2->setIncidentEdge(e20);
  }

  // Update loop incident half edge if necessary
  // std::cout << "        update loop incident half edge" << std::endl;
  if (hel12 != nullptr && hel12->incidentEdge() == e12) {
    hel12->setIncidentEdge(e10);
  }

  if (hel21 != nullptr && hel21->incidentEdge() == e21) {
    hel21->setIncidentEdge(e20);
  }

  // Update face incident half edge if necessary
  // std::cout << "        update face incident half edge" << std::endl;
  // std::cout << "        half edge e12:" << std::endl;
  // e12->print2();
  // if (f12 != nullptr) {
  //   std::cout << "        half edge f12->outer():" << std::endl;
  //   f12->outer()->print2();
  // }
  if (f12 != nullptr && f12->outer() == e12) {
    f12->setOuterComponent(e10);
  }

  // std::cout << "        half edge e21:" << std::endl;
  // e21->print2();
  // if (f21 != nullptr) {
  //   std::cout << "        half edge f21->outer():" << std::endl;
  //   f21->outer()->print2();
  // }
  if (f21 != nullptr && f21->outer() == e21) {
    f21->setOuterComponent(e20);
  }

  // Add the new vertex
  // std::cout << "        add new vertex" << std::endl;
  addVertex(v0);
  // _vertices.push_back(v0);
  // _vertex_tree->insert(v0);

  // Add the new half edges
  // std::cout << "        add new half edges" << std::endl;
  _halfedges.push_back(e10);
  _halfedges.push_back(e02);
  _halfedges.push_back(e20);
  _halfedges.push_back(e01);

  // std::cout << "        remove old half edges" << std::endl;
  _halfedges.remove(e12);
  _halfedges.remove(e21);

  // std::cout << "        delete old half edges" << std::endl;
  // delete e12;
  // delete e21;

  // std::cout << "        half edges of vertex v0: " << v0 << std::endl;
  // v0->printAllLeavingHalfEdges();

  // std::cout << "        half edges of vertex v1: " << v1 << std::endl;
  // v1->printAllLeavingHalfEdges();

  // std::cout << "        half edges of vertex v2: " << v2 << std::endl;
  // v2->printAllLeavingHalfEdges();
}









PDCELHalfEdge *PDCEL::addEdge(PDCELVertex *v1, PDCELVertex *v2) {
  // std::cout << "[debug] addEdge: " << v1 << ", " << v2 << std::endl;

  addVertex(v1);
  addVertex(v2);

  PGeoLineSegment *ls = new PGeoLineSegment(v1, v2);

  PDCELHalfEdge *he12 = new PDCELHalfEdge(v1, 1);
  PDCELHalfEdge *he21 = new PDCELHalfEdge(v2, -1);

  he12->setLineSegment(ls);
  he21->setLineSegment(ls);

  ls->setHalfEdge(he12);
  ls->setHalfEdge(he21);

  he12->setTwin(he21);
  he21->setTwin(he12);

  // std::cout << "v1 degree = " << v1->degree() << std::endl;
  updateEdgeNeighbors(he12);

  // std::cout << "v2 degree = " << v2->degree() << std::endl;
  updateEdgeNeighbors(he21);

  _halfedges.push_back(he12);
  _halfedges.push_back(he21);

  return he12;
}









PDCELHalfEdge *PDCEL::addEdge(PGeoLineSegment *ls) {
  PDCELHalfEdge *he12 = new PDCELHalfEdge(ls->v1(), 1);
  PDCELHalfEdge *he21 = new PDCELHalfEdge(ls->v2(), -1);

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

  // std::cout << "he1: " << he << std::endl;
  // std::cout << "he2: " << he2 << std::endl;

  // Update face incident edges
  // std::cout << "update face incident edge\n";
  if (he->face()) {
    // std::cout << "he1 outer\n";
    // he->face()->print();
    // std::cout << he->face()->outer() << std::endl;
    if (he->face()->outer() == he) {
      if (he->next()) {
        he->face()->setOuterComponent(he->next());
      } else if (he->prev()) {
        he->face()->setOuterComponent(he->prev());
      } else {
        he->face()->setOuterComponent(nullptr);
      }
    }

    // std::cout << "he1 inner\n";
    for (int i=0; i<he->face()->inners().size(); i++) {
      // std::cout << he->face()->inners()[i] << std::endl;
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
    // std::cout << "he2 outer\n";
    // std::cout << he2->face()->name() << std::endl;
    // he2->face()->print();
    // std::cout << he2->face()->outer() << std::endl;
    if (he2->face()->outer() == he2) {
      if (he2->next()) {
        he2->face()->setOuterComponent(he2->next());
      } else if (he2->prev()) {
        he2->face()->setOuterComponent(he2->prev());
      } else {
        he2->face()->setOuterComponent(nullptr);
      }
    }

    // std::cout << "he2 inner\n";
    for (int i=0; i<he2->face()->inners().size(); i++) {
      // std::cout << he2->face()->inners()[i] << std::endl;
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
  // std::cout << "update loop incident edge\n";
  if (he->loop()) {
    // std::cout << he->loop()->incidentEdge() << std::endl;
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
    // std::cout << he2->loop()->incidentEdge() << std::endl;
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
  // std::cout << "update prev and next edge\n";
  if (he->prev() && he->next()) {
    he->prev()->setNext(he->next());
    he->next()->setPrev(he->prev());
  }
  // std::cout << he->prev()->next() << std::endl;
  // std::cout << he->next()->prev() << std::endl;

  if (he2->prev() && he2->next()) {
    he2->prev()->setNext(he2->next());
    he2->next()->setPrev(he2->prev());
  }
  // std::cout << he2->prev()->next() << std::endl;
  // std::cout << he2->next()->prev() << std::endl;

  // Update vertex incident edge
  // std::cout << "update vertex incident edge\n";
  // std::cout << he->source()->edge() << std::endl;
  if (he->source()->edge() == he) {
    if (he->twin()->next()) {
      he->source()->setIncidentEdge(he->twin()->next());
    } else if (he->prev()) {
      he->source()->setIncidentEdge(he->prev()->twin());
    }
  }

  // std::cout << he2->source()->edge() << std::endl;
  if (he2->source()->edge() == he2) {
    if (he2->twin()->next()) {
      he2->source()->setIncidentEdge(he2->twin()->next());
    } else if (he2->prev()) {
      he2->source()->setIncidentEdge(he2->prev()->twin());
    }
  }

  // Merge source and target vertices
  // std::cout << "merge source and target vertices\n";
  PDCELVertex *vs = he->source();
  PDCELVertex *vt = he->target();
  PDCELHalfEdge *_he_tmp = vt->edge();
  do {
    // Outbound edge
    _he_tmp->setSource(vs);

    // Inbound edge
    _he_tmp = _he_tmp->twin();


    _he_tmp = _he_tmp->next();
  } while (_he_tmp != vt->edge());

  // Remove target vertex
  removeVertex(vt);

  _halfedges.remove(he);
  _halfedges.remove(he2);
}









PDCELHalfEdge *PDCEL::findHalfEdge(PDCELVertex *v1, PDCELVertex *v2) {
  // std::cout << "[debug] findHalfEdge: " << v1 << ", " << v2 << std::endl;
  // std::cout << "        half edge v1->edge(): ";
  // if (v1->edge() != nullptr) {
  //   std::cout << v1->edge() << std::endl;
  // } else {
  //   std::cout << "nullptr" << std::endl;
  // }

  if (v1->edge() == nullptr) {
    return nullptr;
  }

  PDCELHalfEdge *he1 = v1->edge();
  PDCELHalfEdge *heit = he1;
  do {
    // std::cout << "        vertex heit->target(): " << heit->target() << std::endl;
    if (heit->target() == v2) {
      return heit;
    }
    heit = heit->twin()->next();
  } while (heit != he1 && heit != nullptr);

  return nullptr;
}









void PDCEL::addEdgesFromCurve(Baseline *bl) {
  // std::cout << "[debug] adding edges from a curve" << std::endl;
  PDCELHalfEdge *he;
  for (int i = 0; i < bl->vertices().size() - 1; ++i) {
    he = addEdge(bl->vertices()[i], bl->vertices()[i + 1]);
    // std::cout << "        new half edge he: " << he << std::endl;
  }
}









// ===================================================================
//
// Half Edge Loop
//
// ===================================================================

int PDCEL::isOuterOrInnerBoundary(PDCELHalfEdge *he1, PDCELHalfEdge *he2) {
  SVector3 sv1, sv2, sv0;
  sv1 = he1->toVector();
  sv2 = he2->toVector();

  sv0 = crossprod(sv1, sv2);

  return sv0.x() > 0 ? 1 : -1;
}









PDCELHalfEdgeLoop *PDCEL::addHalfEdgeLoop(PDCELHalfEdge *he) {
  // std::cout << "[debug] addHalfEdgeLoop:" << std::endl;
  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();

  PDCELHalfEdge *hei = he;
  do {
    // std::cout << "        half edge hei: " << hei << std::endl;
    hei->setLoop(hel);
    hel->updateVertexEdge(hei);
    hei = hei->next();
  } while (hei != he);

  _halfedge_loops.push_back(hel);

  return hel;
}









PDCELHalfEdgeLoop *
PDCEL::addHalfEdgeLoop(const std::list<PDCELVertex *> &vloop) {
  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();

  PDCELVertex *v1, *v2;
  PDCELHalfEdge *he12, *he21;
  PDCELHalfEdge *he12prev = nullptr, *he21next = nullptr;

  std::list<PDCELVertex *>::const_iterator vit;
  for (vit = vloop.begin(); vit != vloop.end(); ++vit) {
    addVertex(*vit);

    // if ((*vit)->edge() == nullptr) {
    //   std::cout << "adding new vertex " << (*vit) << std::endl;
    //   _vertices.push_back(*vit);
    //   _vertex_tree->insert(*vit);
    // }

    if (vit != vloop.begin()) {
      v1 = *std::prev(vit);
      v2 = *vit;

      he12 = findHalfEdge(v1, v2);
      if (he12 == nullptr) {
        // New half edges
        // std::cout << "creating new half edge:" << std::endl;
        // std::cout << v1 << " -> " << v2 << std::endl;
        he12 = addEdge(v1, v2);
      }
      he12->setLoop(hel);

      // hel->updateVertexEdge(he12);
      // std::cout << "loop bottom left vertex: " << hel->bottomLeftVertex()
      //           << std::endl;
    }
  }

  _halfedge_loops.push_back(hel);

  return hel;
}









void PDCEL::removeHalfEdgeLoop(PDCELHalfEdgeLoop *hel) {
  PDCELHalfEdge *he = hel->incidentEdge();
  do {
    he->resetLoop();
    he = he->next();
  } while (he != hel->incidentEdge());

  _halfedge_loops.remove(hel);

  delete hel;
}









void PDCEL::clearHalfEdgeLoops() {
  for (auto he : _halfedges) {
    he->resetLoop();
  }
  _halfedge_loops.clear();
}









PDCELHalfEdgeLoop *PDCEL::findNearestLoop(PDCELHalfEdgeLoop *loop) {
  PDCELHalfEdgeLoop *loop_near = _halfedge_loops.front();

  PGeoLineSegment *ls_below =
      findLineSegmentBelowVertex(loop->bottomLeftVertex());
  if (ls_below != nullptr) {
    PDCELHalfEdge *he = findHalfEdge(ls_below->vin(), ls_below->vout());
    loop_near = he->loop();
  }

  return loop_near;
}









void PDCEL::removeTempLoops() {
  // Remove all half edge loops, excluding segments face boundaries
  // std::cout << "[debug] removing temporary half edge loops" << std::endl;

  for (auto he : _halfedges) {
    if ((he->loop() != nullptr) && !(he->loop()->keep())) {
      he->resetLoop();
    }
  }

  std::vector<PDCELHalfEdgeLoop *> loops_to_remove;
  for (auto hel : _halfedge_loops) {
    if (!hel->keep()) {
      loops_to_remove.push_back(hel);
    }
  }

  for (auto hel : loops_to_remove) {
    _halfedge_loops.remove(hel);
  }
}









void PDCEL::createTempLoops() {
  // Create new half edge loops, excluding segments face boundaries
  // std::cout << "[debug] creating temporary half edge loops" << std::endl;

  for (auto he : _halfedges) {
    if (he->loop() == nullptr) {
      PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();
      PDCELHalfEdge *hei = he;
      do {
        hei->setLoop(hel);
        hei = hei->next();
      } while (hei != he);
      _halfedge_loops.push_back(hel);
    }
  }
}









void PDCEL::linkHalfEdgeLoops() {
  PDCELHalfEdgeLoop *hel;
  std::list<PDCELHalfEdgeLoop *>::iterator lit;
  for (lit = _halfedge_loops.begin(); lit != _halfedge_loops.end(); ++lit) {
    if ((*lit)->direction() < 0) {
      // Each inner loop should have a path linking to an outer loop
      hel = findNearestLoop((*lit));
      (*lit)->setAdjacentLoop(hel);
    }
  }
}









PDCELHalfEdgeLoop *PDCEL::findEnclosingLoop(PDCELVertex *v) {
  // std::cout << "\n[debug] findEnclosingLoop: " << v << std::endl;
  PDCELHalfEdgeLoop *loop_near = _halfedge_loops.front();

  PGeoLineSegment *ls_below = findLineSegmentBelowVertex(v);
  if (ls_below != nullptr) {
    // std::cout << "        line segment ls_below: " << ls_below << std::endl;
    // std::cout << "        vertex ls_below->vin(): " << ls_below->vin() << std::endl;
    // std::cout << "        vertex ls_below->vout(): " << ls_below->vout() << std::endl;
    PDCELHalfEdge *he = findHalfEdge(ls_below->vin(), ls_below->vout());
    // std::cout << "        half edge he: " << he << std::endl;
    
    loop_near = he->loop();
    // std::cout << "        half edge loop loop_near:" << std::endl;
    // loop_near->print();

    while (loop_near->adjacentLoop() != nullptr) {
      // Need to return the outer loop
      loop_near = loop_near->adjacentLoop();
    }
  }

  // loop_near->print();

  return loop_near;
}









void PDCEL::findCurvesIntersection(PDCELHalfEdgeLoop *hel,
                                   PGeoLineSegment *ls) {
  // std::cout << "[debug] findCurvesIntersection(loop, line segment)"
  //           << std::endl;

  PDCELHalfEdge *hei = hel->incidentEdge(), *he1, *he2;
  PGeoLineSegment *lsi;
  bool not_parallel, split1, split2;
  double u_ls, u_lsi;
  PDCELVertex *v_tmp, *v1, *v2;
  std::list<PDCELVertex *> vlist1, vlist2;

  // std::cout << "        line segment ls:" << ls << std::endl;
  do {
    lsi = hei->toLineSegment();
    // std::cout << "        line segment lsi: " << lsi << std::endl;

    not_parallel = calcLineIntersection2D(lsi, ls, u_lsi, u_ls);
    // std::cout << "        not_parallel = " << (not_parallel ? "true" : "false") << std::endl;
    // std::cout << "        u_lsi = " << u_lsi << ", u_ls = " << u_ls <<
    // std::endl;
    if (!not_parallel) {
      // std::cout << "        isCollinear(lsi, ls) = " << (isCollinear(lsi, ls) ? "true" : "false") << std::endl;
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
        // std::cout << "        u_lsi = " << std::endl;
        if (u_lsi < TOLERANCE) {
          v_tmp = lsi->v1();
        } else if (1 - u_lsi < TOLERANCE) {
          v_tmp = lsi->v2();
        } else {
          v_tmp = lsi->getParametricVertex(u_lsi);
        }

        // std::cout << "        vertex v_tmp: " << v_tmp << std::endl;

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
    // std::cout << std::endl;
  } while (hei != hel->incidentEdge());

  // std::cout << "        vertex list vlist1:" << std::endl;
  // for (auto v : vlist1) {
  //   std::cout << "        " << v << std::endl;
  // }

  // std::cout << "        vertex list vlist2:" << std::endl;
  // for (auto v : vlist2) {
  //   std::cout << "        " << v << std::endl;
  // }

  // Find the closest two vertices, v1 from vlist1 and v2 from vlist2
  if (vlist1.size() == 1) {
    v1 = vlist1.front();
  } else if (vlist1.size() > 1) {
    double d2{INF}, d2_tmp;
    for (auto v : vlist1) {
      d2_tmp = calcDistanceSquared(v, vlist2.front());
      if (d2_tmp < d2) {
        d2 = d2_tmp;
        v1 = v;
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

  // std::cout << "        vertex v1: " << v1 << std::endl;
  // std::cout << "        vertex v2: " << v2 << std::endl;

  // Split half edges if necessary
  if (v1->degree() == 0) {
    // New vertex
    splitEdge(he1, v1);
  }

  if (v2->degree() == 0) {
    splitEdge(he2, v2);
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
  // std::cout << "[debug] addFace" << std::endl;

  PDCELFace *fnew = new PDCELFace();

  fnew->setOuterComponent(hel->incidentEdge());
  PDCELHalfEdge *hei = hel->incidentEdge();
  do {
    hei->setIncidentFace(fnew);
    hei = hei->next();
  } while (hei != hel->incidentEdge());

  _faces.push_back(fnew);

  // PDCELHalfEdge *he = fnew->outer();
  // do {
  //   std::cout << "        " << he << std::endl;
  //   he = he->next();
  // } while (he != fnew->outer());

  return fnew;
}









PDCELFace *PDCEL::addFace(const std::list<PDCELVertex *> &vloop, PDCELFace *f) {
  if (f == nullptr) {
    // insert into the background unbounded face
    f = _faces.front();
  }

  PDCELFace *fnew = new PDCELFace();
  std::list<PDCELFace *> other_faces;

  // Add vertices and create half edges
  PDCELVertex *v1, *v2;
  PDCELHalfEdge *he12, *he21;
  PDCELHalfEdge *head = nullptr, *he12prev = nullptr, *he21next = nullptr;
  std::list<PDCELVertex *>::const_iterator it;
  for (it = vloop.begin(); it != vloop.end(); ++it) {
    // std::cout << "vertex: " << (*it) << std::endl;
    addVertex(*it);

    if (it != vloop.begin()) {
      // Create half edges from the second vertex
      v1 = *std::prev(it);
      v2 = *it;

      he12 = findHalfEdge(v1, v2);
      // std::cout << "he12: " << he12 << std::endl;
      if (he12 == nullptr) {
        // New half edges
        he12 = new PDCELHalfEdge(v1, 1);
        he21 = new PDCELHalfEdge(v2, -1);

        he12->setTwin(he21);
        he21->setTwin(he12);

        if (v1->edge() == nullptr) {
          v1->setIncidentEdge(he12);
        }
        if (v2->edge() == nullptr) {
          v2->setIncidentEdge(he21);
        }

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
        // Half edges already exist
        // Record all neighboring faces that are not the background f
        he21 = he12->twin();
        if (he21->face() != f) {
          other_faces.push_back(he21->face());
        }

        // Update the incident edge of the inner hole of the face f
        for (int i = 0; i < f->inners().size(); ++i) {
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

      if (head == nullptr) {
        head = he12;
      }
    }
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
  std::list<PDCELFace *>::iterator fit;
  for (fit = other_faces.begin(); fit != other_faces.end(); ++fit) {
    PDCELHalfEdge *out_he = (*fit)->outer(), *out_he_next;
    do {
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
    } while (out_he != (*fit)->outer());
  }

  return fnew;
}









std::list<PDCELFace *> PDCEL::splitFace(PDCELFace *f, PDCELVertex *v1,
                                        PDCELVertex *v2) {
  // std::cout << "[debug] splitFace:" << std::endl;
  // f->print();

  // std::cout << "        vertex v1 = " << v1 << std::endl;
  // std::cout << "        vertex v2 = " << v2 << std::endl;

  PDCELHalfEdge *he12, *he21;
  PDCELHalfEdgeLoop *hel = f->outer()->loop();

  // std::cout << "        creating new half edges" << std::endl;
  he12 = addEdge(v1, v2);
  he21 = he12->twin();

  // Create new half edge loops
  // std::cout << "        creating new half edge loops" << std::endl;
  PDCELHalfEdgeLoop *hel12 = addHalfEdgeLoop(he12);
  PDCELHalfEdgeLoop *hel21 = addHalfEdgeLoop(he21);

  // Create new faces
  // std::cout << "        creating new faces" << std::endl;
  PDCELFace *f12 = addFace(hel12);
  PDCELFace *f21 = addFace(hel21);

  _faces.remove(f);
  // delete f;

  _halfedge_loops.remove(hel);
  // delete hel;

  std::list<PDCELFace *> new_faces;
  new_faces.push_back(f12);
  new_faces.push_back(f21);

  // std::cout << "        new face f12:" << std::endl;
  // f12->print();

  // std::cout << "        new face f21:" << std::endl;
  // f21->print();

  return new_faces;
}









std::list<PDCELFace *> PDCEL::splitFace(PDCELFace *f, PGeoLineSegment *ls) {
  // std::cout << "[debug] splitFace(face, line segment)" << std::endl;

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
  // std::cout << "[debug] updateEdgeNeighbors: " << he << std::endl;

  PDCELVertex *v = he->source();
  PDCELHalfEdge *het = he->twin();

  // std::cout << "        v degree = " << v->degree() << std::endl;
  if (v->degree() == 0) {
    v->setIncidentEdge(he);
  } else if (v->degree() == 1) {
    he->setPrev(v->edge()->twin());
    v->edge()->twin()->setNext(he);
    het->setNext(v->edge());
    v->edge()->setPrev(het);
  } else {
    // Two or more edges
    // Find the closet one on the left to the new edge
    std::list<PDCELHalfEdge *> cycle_list, cycle_list_2;
    PDCELHalfEdge *hei;
    int list_num = 1;

    hei = v->edge();
    cycle_list.push_back(hei);
    hei = hei->prev()->twin();
    do {
      if (hei->angle() < cycle_list.back()->angle()) {
        list_num = 2;
      }
      if (list_num == 1) {
        cycle_list.push_back(hei);
      } else if (list_num == 2) {
        cycle_list_2.push_back(hei);
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

    he->setPrev(he12left->twin());
    het->setNext(he12left->twin()->next());
    he12left->twin()->next()->setPrev(het);
    he12left->twin()->setNext(he);
  }
}









std::list<PGeoLineSegment *>
PDCEL::findLineSegmentsAtSweepLine(PDCELVertex *v) {
  // std::cout << "[debug] findLineSegmentsAtSweepLine: " << v << std::endl;

  std::list<PGeoLineSegment *> ls_list;

  std::list<PBSTNodeVertex *> node_list;
  node_list = _vertex_tree->toListInOrder();

  PBSTNodeVertex *node;
  PDCELVertex *vi;
  PDCELHalfEdge *he;

  PGeoLineSegment *ls_tmp;

  // Vertical sweep line passing through each vertex from left to right
  while (!node_list.empty()) {
    node = node_list.front();
    // std::cout << "node: " << node << std::endl;

    vi = node->vertex();
    // std::cout << "all leaving half edges:\n";
    // vi->printAllLeavingHalfEdges(-1);
    if (vi == v) {
      break;
    }

    // For each line segment having this vertex,
    // if this vertex is on the left (or bottom), add this line segment to the
    // list otherwise (on the right or top), remove this line segment from the
    // list
    he = vi->edge();
    do {
      // std::cout << "he: " << he << std::endl;
      if (vi->point() < he->target()->point()) {
        if (he->lineSegment() == nullptr) {
          ls_tmp = he->toLineSegment();
          he->setLineSegment(ls_tmp);
          he->twin()->setLineSegment(ls_tmp);
        }
        ls_list.push_back(he->lineSegment());
      }
      else {
        ls_list.remove(he->lineSegment());
        he->clearLineSegment();
        he->twin()->clearLineSegment();
      }
      // std::cout << "he->twin(): " << he->twin() << std::endl;
      // std::cout << "he->twin()->next(): " << he->twin()->next() << std::endl;
      he = he->twin()->next();
    } while (he != nullptr && he != vi->edge());

    node_list.pop_front();
  }

  // std::cout << "[debug] function findLineSegmentsAtSweepLine done" << std::endl;

  return ls_list;
}









PGeoLineSegment *PDCEL::findLineSegmentBelowVertex(PDCELVertex *v) {
  // std::cout << "[debug] findLineSegmentBelowVertex: " << v << std::endl;

  PGeoLineSegment *ls_below = nullptr, *ls_tmp;

  std::list<PGeoLineSegment *> ls_list;
  ls_list = findLineSegmentsAtSweepLine(v);

  PDCELVertex *vt = new PDCELVertex(v->x(), v->y(), v->z() + 1);
  PDCELVertex *vintersect, *vbelow;

  ls_tmp = new PGeoLineSegment(v, vt);

  double u1 = INF, u2, u1_tmp;
  bool is_intersect;

  std::list<PGeoLineSegment *>::iterator lsit;
  for (lsit = ls_list.begin(); lsit != ls_list.end(); ++lsit) {
    // std::cout << "        line segment *lsit: " << (*lsit) << std::endl;
    // is_intersect = calcLineIntersection2D(v, vt, (*lsit)->v1(),
    // (*lsit)->v2(), u1_tmp, u2);
    is_intersect = calcLineIntersection2D(ls_tmp, (*lsit), u1_tmp, u2);

    if (is_intersect) {
      // not parallel
      if (u1_tmp < 0) {
        // the intersection point is below the vertex
        // vintersect = (*lsit)->getParametricVertex(u2);
        // if (ls_below == nullptr || vbelow->point() < vintersect->point()) {
        if (ls_below == nullptr || fabs(u1_tmp) < fabs(u1)) {
          u1 = u1_tmp;
          ls_below = (*lsit);
          vbelow = (*lsit)->getParametricVertex(u2);
        }
      }
    } else {
      // parallel, i.e. the line segment is also vertical
      if (!isCollinear(ls_tmp, (*lsit))) {
        continue;
      }

      if ((*lsit)->vout()->point() < v->point()) {
        if (ls_below == nullptr || calcDistanceSquared(v, (*lsit)->vout()) <
                                       calcDistanceSquared(v, vbelow)) {
          ls_below = (*lsit);
          vbelow = (*lsit)->vout();
          u1 = ls_tmp->getParametricLocation(vbelow);
        }
      }
    }

    // std::cout << "        vertex vbelow: " << vbelow << std::endl;
  }

  // std::cout << "[debug] function findLineSegmentBelowVertex done" << std::endl;

  return ls_below;
}
