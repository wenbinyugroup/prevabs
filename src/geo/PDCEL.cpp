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

  _vertex_tree = new PAVLTreeVertex();

  // Create the infinite bounding box
  PDCELVertex *v_top_right = new PDCELVertex(0, INF, INF, false);
  PDCELVertex *v_top_left = new PDCELVertex(0, -INF, INF, false);
  PDCELVertex *v_bottom_left = new PDCELVertex(0, -INF, -INF, false);
  PDCELVertex *v_bottom_right = new PDCELVertex(0, INF, -INF, false);

  addVertex(v_top_right);
  addVertex(v_top_left);
  addVertex(v_bottom_left);
  addVertex(v_bottom_right);

  // PGeoLineSegment *ls_top = new PGeoLineSegment(v_top_right, v_top_left);
  // PGeoLineSegment *ls_left = new PGeoLineSegment(v_top_left, v_bottom_left);
  // PGeoLineSegment *ls_bottom = new PGeoLineSegment(v_bottom_left, v_bottom_right);
  // PGeoLineSegment *ls_right = new PGeoLineSegment(v_bottom_right, v_top_right);

  PDCELHalfEdge *he_top = addEdge(v_top_right, v_top_left);
  PDCELHalfEdge *he_left = addEdge(v_top_left, v_bottom_left);
  PDCELHalfEdge *he_bottom = addEdge(v_bottom_left, v_bottom_right);
  PDCELHalfEdge *he_right = addEdge(v_bottom_right, v_top_right);

  PDCELHalfEdgeLoop *hel = addHalfEdgeLoop(he_top);

  PDCELFace *f = new PDCELFace(he_top, false); // The unbounded face
  f->setName("background");

  hel->setDirection(1);
  hel->setFace(f);
  hel->setKeep(true);

  // _vertices.push_back(v_top_right);
  // _vertices.push_back(v_top_left);
  // _vertices.push_back(v_bottom_left);
  // _vertices.push_back(v_bottom_right);
  // v_top_right->setDCEL(this);
  // v_top_left->setDCEL(this);
  // v_bottom_left->setDCEL(this);
  // v_bottom_right->setDCEL(this);

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




void PDCEL::write_dcel_to_file(const std::string &filename) {
  PLOG(debug) << "writing DCEL to file: " << filename;

  std::ofstream file(filename);
  if (!file.is_open()) {
    PLOG(error) << "Failed to open file: " << filename;
    return;
  }

  file << "DCEL Summary" << std::endl;
  file << std::endl;

  // Write vertices
  PLOG(debug) << "writing vertices";
  file << _vertices.size() << " vertices:" << std::endl;
  for (auto v : _vertices) {
    file << v << " - degree " << v->degree() << std::endl;
  }
  file << std::endl;

  // Write half edges
  PLOG(debug) << "writing half edges";
  file << _halfedges.size() << " half edges:" << std::endl;
  for (auto he : _halfedges) {
    he->write_to_file(file);
  }
  file << std::endl;

  // Write half edge loops
  PLOG(debug) << "writing half edge loops";
  file << _halfedge_loops.size() << " half edge loops:" << std::endl;
  for (auto hel : _halfedge_loops) {
    hel->write_to_file(file);
  }
  file << std::endl;

  // Write faces
  PLOG(debug) << "writing faces";
  file << _faces.size() << " faces:" << std::endl;
  for (auto f : _faces) {
    f->write_to_file(file);
  }

  file.close();

  PLOG(debug) << "done";
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
  PLOG(trace) << "start";

  PLOG(trace) << "  v = " << v;

  if (v->dcel() == nullptr) {
    PLOG(trace) << "  new vertex";
    _vertices.push_back(v);
    PLOG(trace) << "  vertex added to _vertices";
    PLOG(trace) << "  _vertices.size() = " << _vertices.size();
    _vertex_tree->insert(v);
    PLOG(trace) << "  vertex inserted into _vertex_tree";
    v->setDCEL(this);
  } else {
    PLOG(trace) << "  vertex already in DCEL";
  }

  PLOG(trace) << "done";
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
  if (f12 != nullptr) {
    if (f12->outer() == e12) {
      // The original half edge is the outer incident half edge of the face
      f12->setOuterComponent(e10);
    } else {
      // Check inner half edges
      for (int i=0; i<f12->inners().size(); i++) {
        if (f12->inners()[i] == e12) {
          f12->inners()[i] = e10;
          break;
        }
      }
    }
  }

  if (f21 != nullptr) {
    if (f21->outer() == e21) {
      // The original half edge is the outer incident half edge of the face
      f21->setOuterComponent(e20);
    } else {
      // Check inner half edges
      for (int i=0; i<f21->inners().size(); i++) {
        if (f21->inners()[i] == e21) {
          f21->inners()[i] = e20;
          break;
        }
      }
    }
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


  _halfedges.remove(e12);
  _halfedges.remove(e21);

}









/**
 * @brief Add a new edge to the DCEL.
 *
 * This function adds a new edge between two vertices in the DCEL. It handles:
 * 1. Input validation
 * 2. Vertex addition if not already present
 * 3. Creation of twin half-edges
 * 4. Creation and linking of the line segment
 * 5. Updating edge neighbors
 * 6. Adding the new edges to the DCEL
 *
 * @param v1 The source vertex of the edge
 * @param v2 The target vertex of the edge
 * @return The half edge from v1 to v2, or nullptr if operation fails
 * @throws std::invalid_argument if input vertices are invalid
 * @throws std::runtime_error if edge creation fails
 */
PDCELHalfEdge *PDCEL::addEdge(PDCELVertex *v1, PDCELVertex *v2) {
  PLOG(trace) << "Adding edge between vertices: " << v1 << " -> " << v2;

  // Input validation
  if (!v1 || !v2) {
    PLOG(error) << "Invalid vertex input: v1=" << v1 << ", v2=" << v2;
    throw std::invalid_argument("Invalid vertex input");
  }

  // Add vertices if not already in DCEL
  addVertex(v1);
  addVertex(v2);

  // Create line segment
  PGeoLineSegment *ls = new PGeoLineSegment(v1, v2);

  // Create twin half-edges
  PDCELHalfEdge *he12 = new PDCELHalfEdge(v1, 1);
  PDCELHalfEdge *he21 = new PDCELHalfEdge(v2, -1);

  // Link half-edges with line segment
  he12->setLineSegment(ls);
  he21->setLineSegment(ls);
  ls->setHalfEdge(he12);
  ls->setHalfEdge(he21);
  he12->setTwin(he21);
  he21->setTwin(he12);

  // Update edge neighbors
  updateEdgeNeighbors(he12);
  updateEdgeNeighbors(he21);

  // Add edges to DCEL
  _halfedges.push_back(he12);
  _halfedges.push_back(he21);

  v1->log_all_leaving_half_edges(1);
  v2->log_all_leaving_half_edges(1);

  PLOG(trace) << "Successfully added edge";
  return he12;
}


PDCELHalfEdge *PDCEL::find_or_add_edge(PDCELVertex *v1, PDCELVertex *v2) {
  PDCELHalfEdge *he = findHalfEdge(v1, v2);
  if (he == nullptr) {
    he =  addEdge(v1, v2);
  }
  return he;
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
  // updateEdgeNeighbors(he21);

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
  PLOG(debug) << "removing half edge: " << he;

  PDCELHalfEdge *he2 = he->twin();

  // Update face incident edges
  PLOG(debug) << "  check face incident edge";
  if (he->face()) {
    PLOG(debug) << "    the half edge belongs to a face";
    if (he->face()->outer() == he) {
      PLOG(debug) << "      the half edge is the outer component of the face";
      if (he->next()) {
        PLOG(debug) << "        the half edge has a next, set the outer component to the next";
        he->face()->setOuterComponent(he->next());
      } else if (he->prev()) {
        PLOG(debug) << "        the half edge has a prev, set the outer component to the prev";
        he->face()->setOuterComponent(he->prev());
      } else {
        PLOG(debug) << "        the half edge has no next or prev, set the outer component to nullptr";
        he->face()->setOuterComponent(nullptr);
      }
    }

    PLOG(debug) << "    check inner half edges";
    for (int i=0; i<he->face()->inners().size(); i++) {
      PLOG(debug) << "      checking inner half edge: " << he->face()->inners()[i];
      if (he->face()->inners()[i] == he) {
        PLOG(debug) << "        the half edge is found in the inner half edges";
        if (he->next()) {
          PLOG(debug) << "          the half edge has a next, set the inner half edge to the next";
          he->face()->inners()[i] = he->next();
        } else if (he->prev()) {
          PLOG(debug) << "          the half edge has a prev, set the inner half edge to the prev";
          he->face()->inners()[i] = he->prev();
        } else {
          PLOG(debug) << "          the half edge has no next or prev, set the inner half edge to nullptr";
          he->face()->inners()[i] = nullptr;
        }
        break;
      }
    }
  }

  PLOG(debug) << "  check twin half edge";
  if (he2->face()) {
    PLOG(debug) << "    the twin half edge belongs to a face";
    if (he2->face()->outer() == he2) {
      PLOG(debug) << "      the twin half edge is the outer component of the face";
      if (he2->next()) {
        PLOG(debug) << "        the twin half edge has a next, set the outer component to the next";
        he2->face()->setOuterComponent(he2->next());
      } else if (he2->prev()) {
        PLOG(debug) << "        the twin half edge has a prev, set the outer component to the prev";
        he2->face()->setOuterComponent(he2->prev());
      } else {
        PLOG(debug) << "        the twin half edge has no next or prev, set the outer component to nullptr";
        he2->face()->setOuterComponent(nullptr);
      }
    }

    PLOG(debug) << "    check inner half edges";
    for (int i=0; i<he2->face()->inners().size(); i++) {
      PLOG(debug) << "      checking inner half edge: " << he2->face()->inners()[i];
      if (he2->face()->inners()[i] == he2) {
        PLOG(debug) << "        the twin half edge is found in the inner half edges";
        if (he2->next()) {
          PLOG(debug) << "          the twin half edge has a next, set the inner half edge to the next";
          he2->face()->inners()[i] = he2->next();
        } else if (he2->prev()) {
          PLOG(debug) << "          the twin half edge has a prev, set the inner half edge to the prev";
          he2->face()->inners()[i] = he2->prev();
        } else {
          PLOG(debug) << "          the twin half edge has no next or prev, set the inner half edge to nullptr";
          he2->face()->inners()[i] = nullptr;
        }
        break;
      }
    }
  }

  PLOG(debug) << "  check loop incident edge";
  if (he->loop()) {
    PLOG(debug) << "    the half edge belongs to a loop";
    if (he->loop()->incidentEdge() == he) {
      PLOG(debug) << "      the half edge is the incident edge of the loop";
      if (he->next()) {
        PLOG(debug) << "        the half edge has a next, set the incident edge to the next";
        he->loop()->setIncidentEdge(he->next());
      } else if (he->prev()) {
        PLOG(debug) << "        the half edge has a prev, set the incident edge to the prev";
        he->loop()->setIncidentEdge(he->prev());
      } else {
        PLOG(debug) << "        the half edge has no next or prev, set the incident edge to nullptr";
        he->loop()->setIncidentEdge(nullptr);
      }
    }
  }

  PLOG(debug) << "  check twin loop incident edge";
  if (he2->loop()) {
    PLOG(debug) << "    the twin half edge belongs to a loop";
    if (he2->loop()->incidentEdge() == he2) {
      PLOG(debug) << "      the twin half edge is the incident edge of the loop";
      if (he2->next()) {
        PLOG(debug) << "        the twin half edge has a next, set the incident edge to the next";
        he2->loop()->setIncidentEdge(he2->next());
      } else if (he2->prev()) {
        PLOG(debug) << "        the twin half edge has a prev, set the incident edge to the prev";
        he2->loop()->setIncidentEdge(he2->prev());
      } else {
        PLOG(debug) << "        the twin half edge has no next or prev, set the incident edge to nullptr";
        he2->loop()->setIncidentEdge(nullptr);
      }
    }
  }

  PLOG(debug) << "  check prev and next edges";
  if (he->prev()) {
    PLOG(debug) << "    the half edge has a prev: " << he->prev();
    if (he->prev()->twin() != he2->next()) {
      PLOG(debug) << "      set the prev's next to the twin's next";
      he->prev()->setNext(he2->next());
      PLOG(debug) << "      set the twin's next's prev to the prev";
      he2->next()->setPrev(he->prev());
    } else {
      PLOG(debug) << "      the prev's twin is the twin's next";
      PLOG(debug) << "      set the prev's next to nullptr";
      he->prev()->setNext(nullptr);
      PLOG(debug) << "      set the twin's next's prev to nullptr";
      he2->next()->setPrev(nullptr);
    }
  }

  if (he->next()) {
    PLOG(debug) << "    the half edge has a next: " << he->next();
    if (he->next()->twin() != he2->prev()) {
      PLOG(debug) << "      set the next's prev to the twin's prev";
      he->next()->setPrev(he2->prev());
      PLOG(debug) << "      set the twin's prev's next to the next";
      he2->prev()->setNext(he->next());
    } else {
      PLOG(debug) << "      the next's twin is the prev's twin";
      PLOG(debug) << "      set the next's prev to nullptr";
      he->next()->setPrev(nullptr);
      PLOG(debug) << "      set the twin's prev's next to nullptr";
      he2->prev()->setNext(nullptr);
    }
  }


  PLOG(debug) << "  check source vertex: " << he->source();
  if (he->source()->edge() == he) {
    PLOG(debug) << "    the half edge is the incident edge of the source vertex";
    if (he->twin()->next()) {
      PLOG(debug) << "      the twin half edge has a next, set the incident edge to the next";
      he->source()->setIncidentEdge(he->twin()->next());
    } else if (he->prev()) {
      PLOG(debug) << "      the twin half edge has a prev, set the incident edge to the prev";
      he->source()->setIncidentEdge(he->prev()->twin());
    } else {
      PLOG(debug) << "      the twin half edge has no next or prev, set the incident edge to nullptr";
      he->source()->setIncidentEdge(nullptr);

      PLOG(debug) << "      the source vertex is isolated, remove it";
      removeVertex(he->source());
    }
  }

  PLOG(debug) << "  check twin's source vertex: " << he2->source();
  if (he2->source()->edge() == he2) {
    PLOG(debug) << "    the twin half edge is the incident edge of its source vertex";
    if (he2->twin()->next()) {
      PLOG(debug) << "      the twin half edge has a next, set the incident edge to the next";
      he2->source()->setIncidentEdge(he2->twin()->next());
    } else if (he2->prev()) {
      PLOG(debug) << "      the twin half edge has a prev, set the incident edge to the prev";
      he2->source()->setIncidentEdge(he2->prev()->twin());
    } else {
      PLOG(debug) << "      the twin half edge has no next or prev, set the incident edge to nullptr";
      he2->source()->setIncidentEdge(nullptr);

      PLOG(debug) << "      the source vertex is isolated, remove it";
      removeVertex(he2->source());
    }
  }

  // // Merge source and target vertices
  // // std::cout << "merge source and target vertices\n";
  // PDCELVertex *vs = he->source();
  // PDCELVertex *vt = he->target();
  // PDCELHalfEdge *_he_tmp = vt->edge();
  // do {
  //   // Outbound edge
  //   _he_tmp->setSource(vs);

  //   // Inbound edge
  //   _he_tmp = _he_tmp->twin();


  //   _he_tmp = _he_tmp->next();
  // } while (_he_tmp != vt->edge());

  // // Remove target vertex
  // if (vt->edge() == nullptr) {
  //   removeVertex(vt);
  // }

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

  PLOG(trace) << "looking for half edge: v1 " << v1 << " -> v2 " << v2;

  PDCELHalfEdge *he1 = v1->edge();

  if (he1 == nullptr) {
    PLOG(trace) << "  v1 has no incident edge";
    return nullptr;
  }

  PDCELHalfEdge *heit = he1;
  do {
    if (heit->target() == v2) {
      PLOG(trace) << "found half edge: " << heit;
      return heit;
    }
    heit = heit->twin()->next();
  } while (heit != he1 && heit != nullptr);

  PLOG(trace) << "half edge not found";

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
 * 
 * 1. Create a new half edge loop.
 * 2. Go through the chain of half edges and set the half edge loop of each half edge.
 * 3. Add the half edge loop to the list of half edge loops.
 * 
 * @param[in] he a half edge of the loop
 * @return the newly created half edge loop
 */
PDCELHalfEdgeLoop *PDCEL::addHalfEdgeLoop(PDCELHalfEdge *he) {
  PLOG(trace) << "adding half edge loop with half edge: " << he;

  PDCELHalfEdgeLoop *hel = new PDCELHalfEdgeLoop();

  PDCELHalfEdge *hei = he;
  do {
    PLOG(trace) << "  handling half edge: " << hei;
    hei->setLoop(hel);

    hei = hei->next();

    if (hei == nullptr) {
      PLOG(warning) << "  half edge is nullptr";
      break;
    }

  } while (hei != he);

  _halfedge_loops.push_back(hel);

  PLOG(trace) << "done";

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
  PLOG(trace) << "removing half edge loop";
  PDCELHalfEdge *he = hel->incidentEdge();
  do {
    he->resetLoop();
    he = he->next();
  } while (he != hel->incidentEdge());

  _halfedge_loops.remove(hel);

  delete hel;
}




/**
 * Remove a half edge loop from the DCEL.
 * The method removes the loop and all its half edges from the DCEL.
 * The method also resets the loop pointer of each half edge.
 * @param[in] hel the half edge loop to remove
 */
void PDCEL::remove_half_edge_loop(PDCELHalfEdgeLoop *hel) {
  PLOG(trace) << "removing half edge loop";

  // Reset the loop pointer of each half edge
  PDCELHalfEdge *he = hel->incidentEdge();
  do {
    if (he->loop() == hel) {
      he->resetLoop();
    }
    he = he->next();
  } while (he != hel->incidentEdge());

  // Remove the pointer from the face
  if (hel->face() != nullptr) {
    // Check the outer loop edge
    if (hel->incidentEdge() == hel->face()->outer()) {
      hel->face()->setOuterComponent(nullptr);
    }

    // Check the inner loops
    auto it = std::find(hel->face()->inners().begin(),
                        hel->face()->inners().end(),
                        hel->incidentEdge());
    if (it != hel->face()->inners().end()) {
      // Remove the edge from the vector of inner loops
      hel->face()->inners().erase(it);
    }
  }

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
 * Link an inner half edge loop to its nearest outer half edge loop.
 * @param[in] hel the inner half edge loop to link
 */
void PDCEL::link_inner_half_edge_loop(PDCELHalfEdgeLoop *hel) {
  PLOG(debug) << "linking inner half edge loop";

  PDCELHalfEdgeLoop *adj_hel = findNearestLoop(hel);
  PLOG(debug) << "adjacent loop: ";
  adj_hel->log();
  hel->setAdjacentLoop(adj_hel);

  PLOG(debug) << "done.";
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
  for (auto hel : _halfedge_loops) {
    if (hel->direction() < 0 && hel->adjacentLoop() == nullptr) {
      link_inner_half_edge_loop(hel);
    }
  }
  // std::list<PDCELHalfEdgeLoop *>::iterator lit;
  // for (lit = _halfedge_loops.begin(); lit != _halfedge_loops.end(); ++lit) {
  //   if ((*lit)->direction() < 0) {
  //     // Each inner loop should have a path linking to an outer loop
  //     hel = findNearestLoop((*lit));
  //     (*lit)->setAdjacentLoop(hel);
  //   }
  // }
}









/**
 * Find the enclosing loop of a given vertex.
 * The algorithm works by first finding a line segment below the vertex, then
 * traversing the half edge loop chain until it reaches an outer loop.
 * @param[in] v the vertex to find the enclosing loop for
 * @return the enclosing loop
 */
PDCELHalfEdgeLoop *PDCEL::findEnclosingLoop(PDCELVertex *v) {
  PLOG(debug) << "finding enclosing loop for vertex: " << v;

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
  } else {
    PLOG(debug) << "no finite line segments found, returning background infinite loop";
    loop_near = _halfedge_loops.front();
  }

  loop_near->log();

  PLOG(debug) << "done.";

  return loop_near;
}




bool PDCEL::is_same(PDCELHalfEdgeLoop *a, PDCELHalfEdgeLoop *b) const {

  if (a->direction() != b->direction()) {
    PLOG(debug) << "direction mismatch";
    return false;
  }

  if (a->bottomLeftVertex() != b->bottomLeftVertex()) {
    PLOG(debug) << "bottom left vertex mismatch";
    return false;
  }

  if (a->incidentEdge() != b->incidentEdge()) {
    PLOG(debug) << "incident edge mismatch";
    return false;
  }

  // if (_adjacent_loop != other->adjacentLoop()) {
  //   return false;
  // }

  // if (_keep != other->keep()) {
  //   return false;
  // }

  // if (_face != other->face()) {
  //   return false;
  // }

  return true;
}




/**
 * Update the half edge loop of all half edges from a chain starting from a given half edge.
 * 
 * 1. Go through the chain of half edges and get the first half edge loop.
 * 2. If no half edge loop is found, create a new one.
 * 3. Go through the chain again and set the half edge loop of each half edge.
 * 
 * @param[in] he the starting half edge
 * @return the half edge loop
 */
PDCELHalfEdgeLoop *PDCEL::update_half_edge_loop(PDCELHalfEdge *he) {
  PLOG(debug) << "updating half edge loop" << he;

  PDCELHalfEdge *hei = he;
  PDCELHalfEdgeLoop *hel = nullptr;
  std::list<PDCELHalfEdgeLoop *> hels_to_remove;
  do {
    if (hei->loop() != nullptr) {
      if (hel == nullptr) {
        hel = hei->loop();
      }

      // If the half edge loop is not the same as the one we are updating, add it to the list of half edge loops to remove
      if (!is_same(hei->loop(), hel)) {
        // Check if the loop is already in the list of half edge loops to remove
        if (std::find(hels_to_remove.begin(), hels_to_remove.end(), hei->loop()) == hels_to_remove.end()) {
          PLOG(debug) << "adding half edge loop to remove: ";
          hei->loop()->log();
          hels_to_remove.push_back(hei->loop());
        }
      }
    }
    hei = hei->next();
  } while (hei != he);


  if (hel != nullptr) {
    // Check if the half edge loop's incident edge is in the current chain of half edges
    bool found = false;
    PDCELHalfEdge *_hei = he;
    do {
      if (hel->incidentEdge() == _hei) {
        found = true;
        break;
      }
      _hei = _hei->next();
    } while (_hei != he);

    if (!found) {
      // hels_to_remove.push_back(hel);
      hel = nullptr;
    }
  }


  if (hel != nullptr) {
    PLOG(debug) << "half edge loop found: ";
    // hel->log();
    PLOG(debug) << "  direction: " << hel->direction();
    PLOG(debug) << "  bottom left vertex: " << hel->bottomLeftVertex();
    PLOG(debug) << "  incident edge: " << hel->incidentEdge();
  } else {
    PLOG(debug) << "no half edge loop found, creating a new one";
    hel = new PDCELHalfEdgeLoop();
    _halfedge_loops.push_back(hel);
  }

  hei = he;
  do {
    hei->setLoop(hel);
    hei = hei->next();
  } while (hei != he);


  // Remove the half edge loops that are not the same as the one we are updating
  for (auto hel : hels_to_remove) {
    // _halfedge_loops.remove(hel);
    remove_half_edge_loop(hel);
  }

  PLOG(debug) << "half edge loops after update:";
  for (auto hel : _halfedge_loops) {
    hel->log();
  }

  PLOG(debug) << "done.";

  return hel;
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

  hel->setFace(fnew);

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
  PLOG(debug) << "updating face inner loops";

  // Check for null face
  if (f == nullptr) {
    PLOG(error) << "Cannot update inner loops of null face";
    return;
  }

  PDCELHalfEdgeLoop *hel_out = f->outer()->loop();
  // Check if the face has an outer loop
  if (hel_out == nullptr) {
    PLOG(error) << "Face has no outer loop";
    return;
  }

  // Link half edge loops to establish adjacency relationships
  // linkHalfEdgeLoops();

  // Iterate through all half edge loops
  for (auto heli : halfedgeloops()) {
    // Skip null loops and loops that should be kept
    if (heli == nullptr || heli->keep() || heli->direction() > 0) {
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

      PLOG(debug) << "  heli: ";
      heli->log();

      // Set the face of the inner loop and add it as an inner component
      heli->setFace(f);
      f->addInnerComponent(heli->incidentEdge());
    }
  }

  PLOG(debug) << "done.";
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
  PLOG(trace) << "Updating neighbors of edge " << he;

  if (!he || !he->twin() || !he->source()) {
    PLOG(error) << "Invalid half edge or missing required components";
    return;
  }

  PDCELHalfEdge *het = he->twin();
  PDCELVertex *v = he->source();
  
  // Handle degree 0 case - vertex has no incident edges
  if (v->degree() == 0) {
    PLOG(trace) << "Vertex has no incident edges";
    v->setIncidentEdge(he);
    return;
  }

  // Handle degree 1 case - vertex has one incident edge
  if (v->degree() == 1) {
    PLOG(trace) << "Vertex has one incident edge";
    PDCELHalfEdge *existing_edge = v->edge();
    if (!existing_edge) {
      PLOG(error) << "Vertex with degree 1 has no incident edge";
      return;
    }
    
    // Form a cycle with the existing edge
    he->setPrev(existing_edge->twin());
    existing_edge->twin()->setNext(he);
    het->setNext(existing_edge);
    existing_edge->setPrev(het);
    return;
  }

  PLOG(trace) << "Vertex has degree >= 2";

  // Handle degree >= 2 case - need to find correct position in sorted cycle
  std::vector<PDCELHalfEdge *> sorted_edges;
  PDCELHalfEdge *current = v->edge();
  
  if (!current) {
    PLOG(error) << "Vertex with degree >= 2 has no incident edge";
    return;
  }

  // Collect all edges in sorted order based on angle
  do {
    sorted_edges.push_back(current);
    current = current->prev()->twin();
  } while (current != v->edge());

  PLOG(trace) << "Unsorted edges:";
  for (auto he : sorted_edges) {
    PLOG(trace) << "  " << he << " " << he->angle();
  }

  // Sort edges by angle
  std::sort(sorted_edges.begin(), sorted_edges.end(),
    [](PDCELHalfEdge* a, PDCELHalfEdge* b) {
      return a->angle() < b->angle();
    });

  PLOG(trace) << "Sorted edges:";
  for (auto he : sorted_edges) {
    PLOG(trace) << "  " << he << " " << he->angle();
  }

  // Find insertion point for new edge
  auto it = std::lower_bound(sorted_edges.begin(), sorted_edges.end(), he,
    [](PDCELHalfEdge* a, PDCELHalfEdge* b) {
      return a->angle() < b->angle();
    });

  // Update prev/next pointers
  PDCELHalfEdge *prev_edge = (it == sorted_edges.begin()) ? sorted_edges.back() : *(it - 1);
  PDCELHalfEdge *next_edge = (it == sorted_edges.end()) ? sorted_edges.front() : *it;

  PLOG(trace) << "Setting prev/next pointers";
  PLOG(trace) << "  prev_edge: " << prev_edge;
  PLOG(trace) << "  next_edge: " << next_edge;

  // Insert new edge
  sorted_edges.insert(it, he);

  PLOG(trace) << "Updated sorted edges:";
  for (auto he : sorted_edges) {
    PLOG(trace) << "  " << he << " " << he->angle();
  }

  he->setPrev(next_edge->twin());
  next_edge->twin()->setNext(he);

  het->setNext(prev_edge);
  prev_edge->setPrev(het);

  PLOG(trace) << "done";
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
  PLOG(debug) << "finding line segments at sweep line at vertex " << v;

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
    PLOG(debug) << "vi: " << vi;

    if (vi == v) {
      break;
    }

    // If the vertex is one of the four infinite corners, skip it
    if (vi->y() == INF || vi->y() == -INF || vi->z() == INF || vi->z() == -INF) {
      PLOG(debug) << "vertex is one of the four infinite corners, skipping";
      node_list.pop_front();
      continue;
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

  PLOG(debug) << "done.";

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
  PLOG(debug) << "finding line segment below vertex " << v;

  PGeoLineSegment *ls_below = nullptr, *ls_tmp;

  std::list<PGeoLineSegment *> ls_list = findLineSegmentsAtSweepLine(v);

  // If the list is empty, return the bottom infinite line segment
  if (ls_list.empty()) {
    // ls_below = new PGeoLineSegment(_vertices[2], _vertices[3]);
    PLOG(debug) << "no finite line segments found.";
    return ls_below;
  }

  PLOG(debug) << "ls_list:";
  for (auto ls : ls_list) {
    PLOG(debug) << "  " << ls;
  }

  PDCELVertex *vt = new PDCELVertex(v->x(), v->y(), v->z() + 1);
  PDCELVertex *vbelow = nullptr;

  ls_tmp = new PGeoLineSegment(v, vt);

  double u1 = INF, u2, u1_tmp;
  bool is_intersect;

  std::list<PGeoLineSegment *>::iterator lsit;
  // for (lsit = ls_list.begin(); lsit != ls_list.end(); ++lsit) {
  for (auto ls : ls_list) {
    PLOG(debug) << "ls: " << ls;
    // is_intersect = calcLineIntersection2D(v, vt, (*lsit)->v1(),
    // (*lsit)->v2(), u1_tmp, u2);
    // is_intersect = calcLineIntersection2D(ls_tmp, (*lsit), u1_tmp, u2, TOLERANCE);
    PDCELVertex *v_intersect;
    is_intersect = calc_line_intersection_2d(
      ls_tmp->v1(), ls_tmp->v2(),
      ls->v1(), ls->v2(),
      v_intersect, u1_tmp, u2, 1, 1, 1, 1
    );

    PLOG(debug) << "is_intersect: " << is_intersect
                << ", u1_tmp: " << u1_tmp
                << ", u2: " << u2;

    if (is_intersect) {
      PLOG(debug) << "intersect";
      // not parallel
      if (u1_tmp < 0) {
        // the intersection point is below the vertex
        // vintersect = (*lsit)->getParametricVertex(u2);
        // if (ls_below == nullptr || vbelow->point() < vintersect->point()) {
        if (ls_below == nullptr || fabs(u1_tmp) < fabs(u1)) {
          u1 = u1_tmp;
          ls_below = ls;
          vbelow = ls->getParametricVertex(u2);
        }
      }
    } else {
      PLOG(debug) << "not intersect";
      // parallel, i.e. the line segment is also vertical
      if (!isCollinear(ls_tmp, ls)) {
        continue;
      }

      if (ls->vout()->point() < v->point()) {
        if (ls_below == nullptr || calcDistanceSquared(v, ls->vout()) <
            calcDistanceSquared(v, vbelow)) {
          ls_below = ls;
          vbelow = ls->vout();
          u1 = ls_tmp->getParametricLocation(vbelow);
        }
      }
    }

    PLOG(debug) << "vbelow: " << (vbelow ? vbelow->to_string() : "nullptr");
  }

  PLOG(debug) << "ls_below: " << (ls_below ? ls_below->printString() : "nullptr");

  PLOG(debug) << "done.";

  return ls_below;
}
