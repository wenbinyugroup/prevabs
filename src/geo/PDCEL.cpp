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









/**
 * Initialize the DCEL by adding the infinite bounding box and the
 * unbounded face.
 */
void PDCEL::initialize() {
  // Create the infinite bounding box
  PDCELVertex *v_top_right = new PDCELVertex(0, INF, INF, false);
  PDCELVertex *v_top_left = new PDCELVertex(0, -INF, INF, false);
  PDCELVertex *v_bottom_left = new PDCELVertex(0, -INF, -INF, false);
  PDCELVertex *v_bottom_right = new PDCELVertex(0, INF, -INF, false);

  PGeoLineSegment *ls_top = new PGeoLineSegment(v_top_right, v_top_left);
  PGeoLineSegment *ls_left = new PGeoLineSegment(v_top_left, v_bottom_left);
  PGeoLineSegment *ls_bottom = new PGeoLineSegment(v_bottom_left, v_bottom_right);
  PGeoLineSegment *ls_right = new PGeoLineSegment(v_bottom_right, v_top_right);

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

  _vertices.push_back(v_top_right);
  _vertices.push_back(v_top_left);
  _vertices.push_back(v_bottom_left);
  _vertices.push_back(v_bottom_right);

  // Remove the half edges which are not part of the unbounded face
  _halfedges.remove(he_top->twin());
  _halfedges.remove(he_left->twin());
  _halfedges.remove(he_bottom->twin());
  _halfedges.remove(he_right->twin());

  _faces.push_back(f);
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









/**
 * @brief Fixes the geometry of the PDCEL by removing very small edges.
 *
 * @param pmessage the message object to print the output
 */
void PDCEL::fixGeometry(Message *pmessage) {
  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("fixing geometry by removing small edges");

  // Find all small edges
  std::vector<PDCELHalfEdge *> small_edges;
  for (auto he : _halfedges) {
    // Check for null pointer reference
    if (he == nullptr) continue;

    // Check if the edge is in the list of small edges already
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
    // Check for null pointer reference
    if (he == nullptr) continue;

    try {
      removeEdge(he);
    } catch (const std::exception& e) {
      PLOG(error) << "Exception occurred while removing edge: " << e.what();
    }
  }

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

/**
 * Find the half edge with source vertex v having incident face f
 * @param[in] v the vertex
 * @param[in] f the face
 * @return the half edge with source vertex v having incident face f, or nullptr if no such half edge exists
 */
PDCELHalfEdge *PDCEL::findHalfEdge(PDCELVertex *v, PDCELFace *f) {
  PLOG(debug) << "findHalfEdge: vertex " << v << ", face " << f;
  PDCELHalfEdge *he = v->edge();

  // Check for null pointer reference
  if (he == nullptr) {
    PLOG(debug) << "findHalfEdge: vertex has no incident edge";
    return nullptr;
  }

  do {
    // Check for null pointer reference
    if (he->face() == nullptr) {
      continue;
    }

    if (he->face() == f) {
      PLOG(debug) << "findHalfEdge: found half edge " << he;
      return he;
    }

    // Check for null pointer reference
    if (he->twin() == nullptr) {
      PLOG(debug) << "findHalfEdge: half edge twin is null";
      return nullptr;
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









/**
 * @brief Add a new edge to the DCEL.
 *
 * This function first adds the two vertices to the DCEL if they are not
 * already in the DCEL. Then it creates two half edges, one from v1 to v2
 * and one from v2 to v1, and a PGeoLineSegment to represent the line
 * segment between the two vertices.  The half edges are then added to the
 * DCEL and the edge neighbors of the two vertices are updated.
 *
 * @param v1 The source vertex of the edge.
 * @param v2 The target vertex of the edge.
 * @return The half edge from v1 to v2.
 */
PDCELHalfEdge *PDCEL::addEdge(PDCELVertex *v1, PDCELVertex *v2) {
  // std::cout << "[debug] addEdge: " << v1 << ", " << v2 << std::endl;
  PLOG(debug) << "adding edge: " << v1 << " -> " << v2;

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









/**
 * @brief Add a new edge to the DCEL, given a PGeoLineSegment.
 *
 * This function creates two half edges, one from v1 to v2 and one from v2
 * to v1, and sets the incident edge of the two vertices if they do not
 * already have an incident edge. Then it sets the twin half edges of each
 * other, and the line segment of each half edge.  Finally, it updates the
 * edge neighbors of the two vertices and adds the two half edges to the
 * DCEL.
 *
 * @param ls The PGeoLineSegment to add to the DCEL.
 * @return The half edge from v1 to v2.
 */
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









/**
 * @brief Remove a half edge from the DCEL.
 *
 * This function removes a half edge from the DCEL and updates the
 * incident edges of the two vertices, the face and loop that the
 * half edge belongs to.  It also updates the prev and next edges of
 * the half edge, and removes the half edge from the DCEL.
 *
 * @param[in] he The half edge to remove from the DCEL.
 */
void PDCEL::removeEdge(PDCELHalfEdge *he) {
  PLOG(debug) << "removing edge: " << he;

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









/**
 * Find the half edge with source vertex v1 and target vertex v2.
 * The method traverses the half edges incident on v1 and checks if
 * the target of the half edge is v2.
 * @param[in] v1 the source vertex
 * @param[in] v2 the target vertex
 * @return the half edge from v1 to v2, or nullptr if no such half edge exists
 */
PDCELHalfEdge *PDCEL::findHalfEdge(PDCELVertex *v1, PDCELVertex *v2) {

  PLOG(debug) << "looking for half edge: v1 " << v1 << " -> v2 " << v2;

  PDCELHalfEdge *he1 = v1->edge();

  if (he1 == nullptr) {
    PLOG(debug) << "  v1 has no incident edge";
    return nullptr;
  }

  PDCELHalfEdge *heit = he1;
  do {
    if (heit->target() == v2) {
      PLOG(debug) << "found half edge: " << heit;
      return heit;
    }
    heit = heit->twin()->next();
  } while (heit != he1 && heit != nullptr);

  PLOG(debug) << "half edge not found";

  return nullptr;
}









/**
 * Add the edges from a Baseline to the DCEL.
 *
 * This function goes through all the vertices in the Baseline and adds
 * an edge between each pair of adjacent vertices. The method addEdge is
 * used to add the edge, which will also update the edge neighbors of
 * the vertices.
 *
 * @param bl the Baseline to add edges from
 */
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

/**
 * Determine if a boundary is an outer or inner boundary.
 * @param[in] he1 one half edge of the boundary
 * @param[in] he2 the other half edge of the boundary
 * @return 1 if the boundary is an outer boundary and -1 if it is an inner boundary
 */
// int PDCEL::isOuterOrInnerBoundary(PDCELHalfEdge *he1, PDCELHalfEdge *he2) {
//   SVector3 sv1, sv2, sv0;
//   sv1 = he1->toVector();
//   sv2 = he2->toVector();

//   sv0 = crossprod(sv1, sv2);

//   return sv0.x() > 0 ? 1 : -1;
// }









/**
 * Add a half edge loop to the data structure.
 * @param[in] he a half edge of the loop
 * @return the newly created half edge loop
 */
PDCELHalfEdgeLoop *PDCEL::addHalfEdgeLoop(PDCELHalfEdge *he) {
  PLOG(debug) << "adding half edge loop with half edge: " << he;

  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();

  PDCELHalfEdge *hei = he;
  do {
    PLOG(debug) << "  handling half edge: " << hei;
    hei->setLoop(hel);
    // hel->updateVertexEdge(hei);
    hei = hei->next();
  } while (hei != he);

  _halfedge_loops.push_back(hel);

  return hel;
}









/**
 * Add a new half edge loop, specified by a list of vertices.
 * The method adds new half edges and vertices as needed.
 * The method also updates the bottom left vertex of the loop.
 * @param[in] vloop the list of vertices to add
 * @return the newly added half edge loop
 */
PDCELHalfEdgeLoop *PDCEL::addHalfEdgeLoop(const std::list<PDCELVertex *> &vloop) {
  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();

  PDCELVertex *v1, *v2;
  PDCELHalfEdge *he12;
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









/**
 * Remove a half edge loop from the DCEL.
 * The method removes the loop and all its half edges from the DCEL.
 * The method also resets the loop pointer of each half edge.
 * @param[in] hel the half edge loop to remove
 */
void PDCEL::removeHalfEdgeLoop(PDCELHalfEdgeLoop *hel) {
  PLOG(debug) << "removing half edge loop";
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









/**
 * Find the half edge loop nearest to a given half edge loop.
 * @param[in] loop the loop to find the nearest loop for
 * @return the nearest loop
 */
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









/**
 * Link inner half edge loops to their nearest outer half edge loop.
 * For each inner loop, find the nearest outer loop by traversing the half edge loop chain.
 * Then set the adjacent loop pointer of the inner loop to the nearest outer loop.
 * This step is necessary for the construction of the final mesh, in which each inner loop
 * should have a path linking to an outer loop.
 */
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









/**
 * Find the enclosing loop of a given vertex.
 * The algorithm works by first finding a line segment below the vertex, then
 * traversing the half edge loop chain until it reaches an outer loop.
 * @param[in] v the vertex to find the enclosing loop for
 * @return the enclosing loop
 */
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









/**
 * Find the intersection points of a given line segment with all line segments of a half edge loop.
 * The method iterates through all half edges of the half edge loop and checks if they intersect with the given line segment.
 * If they do, the method adds the intersection points to two lists, vlist1 and vlist2.
 * If the line segments are parallel, the method checks if they are collinear, and if so, adds the vertices to the lists.
 * Finally, the method finds the closest two vertices, v1 from vlist1 and v2 from vlist2, and splits the half edges if necessary.
 * @param[in] hel the half edge loop to find the intersection points for
 * @param[in] ls the line segment to find the intersection points for
 */
void PDCEL::findCurvesIntersection(PDCELHalfEdgeLoop *hel,
                                   PGeoLineSegment *ls) {
  PLOG(debug) << "finding curves intersection";

  // Iterate through all half edges of the half edge loop
  PDCELHalfEdge *hei = hel->incidentEdge(), *he1, *he2;
  PGeoLineSegment *lsi;

  // Store the vertices of the intersection points
  std::list<PDCELVertex *> vlist1, vlist2;

  // Iterate through all line segments of the half edge loop
  do {
    lsi = hei->toLineSegment();

    PLOG(debug) << "  checking line segments: (lsi) " << lsi
      << " and (ls) " << ls;

    // Check if the line segment intersects with the line segment ls
    // double ipx, ipy;
    double u_lsi, u_ls;
    PDCELVertex *v_intersect;
    // bool not_parallel = calcLineIntersection2D(lsi, ls, u_lsi, u_ls, ABS_TOL);
    bool not_parallel = calc_line_intersection_2d(
      lsi->v1(), lsi->v2(), ls->v1(), ls->v2(),
      v_intersect, u_lsi, u_ls, 1, 1, 1, 1
    );

    if (!not_parallel) {
      // The line segments are parallel, so check if they are collinear
      PLOG(debug) << "  line segments are parallel";

      if (!isCollinear(lsi, ls)) {
        // Not collinear, so skip this line segment
        PLOG(debug) << "  line segments are not collinear";
        hei = hei->next();
        continue;
      }

      // The line segments are collinear, so add the vertices to the lists
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
      // The line segments intersect, so add the intersection point to the lists
      PDCELVertex *v_tmp = nullptr;

      // Calculate the intersection point
      if (is_close(u_lsi, 0)) {
        v_tmp = lsi->v1();
      } else if (is_close(u_lsi, 1)) {
        v_tmp = lsi->v2();
      } else if (u_lsi > 0 && u_lsi < 1) {
        v_tmp = lsi->getParametricVertex(u_lsi);
      } else {
        // The intersection point is outside the line segment, so skip it
        hei = hei->next();
        continue;
      }

      if (v_tmp != nullptr) {
        // Check if the intersection point is already in the lists
        if (!isInContainer(vlist1, v_tmp) && !isInContainer(vlist2, v_tmp)) {
          if (vlist1.empty()) {
            vlist1.push_back(v_tmp);
            he1 = hei;
          } else {
            vlist2.push_back(v_tmp);
            he2 = hei;
          }
        }
      } 
    }

    hei = hei->next();
  } while (hei != hel->incidentEdge());

  // Find the closest two vertices, v1 from vlist1 and v2 from vlist2
  PDCELVertex *v1, *v2;
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
  PLOG(debug) << "splitFace";

  // Find the two intersections
  findCurvesIntersection(f->outer()->loop(), ls);

  return splitFace(f, ls->v1(), ls->v2());
}









/// @brief Update the inner loops of a face
/// @param f The face to update
void PDCEL::update_face_inner_loops(PDCELFace *f) {
  // Check for null face
  if (f == nullptr) {
    PLOG(error) << "Cannot update inner loops of null face";
    return;
  }

  PDCELHalfEdgeLoop *hel_out = f->outer();
  // Check if the face has an outer loop
  if (hel_out == nullptr) {
    PLOG(error) << "Face has no outer loop";
    return;
  }

  // Link half edge loops to establish adjacency relationships
  linkHalfEdgeLoops();
  
  // Iterate through all half edge loops
  for (auto heli : halfedgeloops()) {
    // Skip null loops and loops that should be kept
    if (heli == nullptr || heli->keep()) {
      continue;
    }
    
    // Follow the adjacency chain to find the outermost containing loop
    PDCELHalfEdgeLoop *helj = heli;
    while (helj != nullptr && helj->adjacentLoop() != nullptr) {
      helj = helj->adjacentLoop();
    }
    
    // If the outermost loop is the outer loop of our face
    if (helj == hel_out) {
      // Check if the loop has a valid incident edge
      if (heli->incidentEdge() == nullptr) {
        PLOG(warning) << "Inner loop has no incident edge, skipping";
        continue;
      }
      
      // Set the face of the inner loop and add it as an inner component
      heli->setFace(f);
      f->addInnerComponent(heli->incidentEdge());
    }
  }
}









// ===================================================================
//
// Private helper functions
//
// ===================================================================

/**
 * @brief Update the neighbors of an edge.
 *
 * This function updates the neighbors of an edge by checking the degree of the
 * source vertex and updating the previous and next half edges accordingly.
 *
 * @param he The half edge to update.
 */
void PDCEL::updateEdgeNeighbors(PDCELHalfEdge *he) {
  PLOG(debug) << "updateEdgeNeighbors";

  PDCELVertex *v = he->source();
  PDCELHalfEdge *het = he->twin();


  if (v->degree() == 0) {
    // If the source vertex has degree 0, set the incident edge to be the given
    // half edge.
    v->setIncidentEdge(he);
  } else if (v->degree() == 1) {
    // If the source vertex has degree 1, set the previous and next half edges
    // to form a cycle.
    he->setPrev(v->edge()->twin());
    v->edge()->twin()->setNext(he);
    het->setNext(v->edge());
    v->edge()->setPrev(het);
  } else {
    // Two or more edges
    // Find the closest one on the left to the new edge
    std::list<PDCELHalfEdge *> cycle_list, cycle_list_2;
    PDCELHalfEdge *hei;
    int list_num = 1;

    hei = v->edge();
    cycle_list.push_back(hei);
    hei = hei->prev()->twin();
    do {
      // If the angle of the current half edge is less than the angle of the
      // last half edge in the list, then the list number is 2.
      if (hei->angle() < cycle_list.back()->angle()) {
        list_num = 2;
      }
      // Add the current half edge to the list.
      if (list_num == 1) {
        cycle_list.push_back(hei);
      } else if (list_num == 2) {
        cycle_list_2.push_back(hei);
      }
      // Move to the previous half edge.
      hei = hei->prev()->twin();
    } while (hei != v->edge());

    // If the second list is not empty, then the first list is the list of
    // half edges that are on the left of the new edge and the second list is
    // the list of half edges that are on the right of the new edge.
    // Splice the second list into the first list.
    if (cycle_list_2.size() > 0) {
      cycle_list.splice(cycle_list.begin(), cycle_list_2);
      // Now the list stores leaving edges sorted from (-pi, pi]
    }

    // Insert the new edge according to the angle
    std::list<PDCELHalfEdge *>::iterator it;
    for (it = cycle_list.begin(); it != cycle_list.end(); ++it) {
      // If the angle of the new edge is less than the angle of the current
      // half edge, then insert the new edge before the current half edge.
      if (he->angle() < (*it)->angle())
        break;
    }
    cycle_list.insert(it, he);

    // Update prev and next
    PDCELHalfEdge *he12left;
    if (it == cycle_list.end()) {
      // If the new edge is inserted at the end of the list, then the left
      // neighbor of the new edge is the first half edge in the list.
      he12left = cycle_list.front();
    } else {
      // Otherwise, the left neighbor of the new edge is the current half edge.
      he12left = *it;
    }

    he->setPrev(he12left->twin());
    het->setNext(he12left->twin()->next());
    he12left->twin()->next()->setPrev(het);
    he12left->twin()->setNext(he);
  }
}









/**
 * @brief Find all line segments at the sweep line at a given vertex.
 *
 * Find all line segments at the sweep line at a given vertex. The sweep line
 * is a vertical line passing through the given vertex. The function returns a
 * list of line segments that intersect with the sweep line.
 *
 * @param v The vertex that the sweep line passes through.
 * @return A list of line segments that intersect with the sweep line.
 */
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









/**
 * Find the line segment below a given vertex.
 * The method returns the line segment below the given vertex by sweeping a
 * vertical line through the vertex from the top to the bottom.
 * @param[in] v The vertex to find the line segment below
 * @return The line segment below the given vertex
 */
PGeoLineSegment *PDCEL::findLineSegmentBelowVertex(PDCELVertex *v) {
  // std::cout << "[debug] findLineSegmentBelowVertex: " << v << std::endl;

  PGeoLineSegment *ls_below = nullptr, *ls_tmp;

  std::list<PGeoLineSegment *> ls_list;
  ls_list = findLineSegmentsAtSweepLine(v);

  PDCELVertex *vt = new PDCELVertex(v->x(), v->y(), v->z() + 1);
  PDCELVertex *vbelow;

  ls_tmp = new PGeoLineSegment(v, vt);

  double u1 = INF, u2, u1_tmp;
  bool is_intersect;

  std::list<PGeoLineSegment *>::iterator lsit;
  for (lsit = ls_list.begin(); lsit != ls_list.end(); ++lsit) {
    // std::cout << "        line segment *lsit: " << (*lsit) << std::endl;
    // is_intersect = calcLineIntersection2D(v, vt, (*lsit)->v1(),
    // (*lsit)->v2(), u1_tmp, u2);
    // is_intersect = calcLineIntersection2D(ls_tmp, (*lsit), u1_tmp, u2, TOLERANCE);
    PDCELVertex *v_intersect;
    is_intersect = calc_line_intersection_2d(
      ls_tmp->v1(), ls_tmp->v2(),
      (*lsit)->v1(), (*lsit)->v2(),
      v_intersect, u1, u2, 1, 1, 1, 1
    );

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
