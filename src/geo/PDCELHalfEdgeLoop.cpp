#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <iostream>
#include <fstream>


void PDCELHalfEdgeLoop::log() {

  PLOG(debug) << "direction: " << ((direction() == 1) ? "outer" : "inner");

  PLOG(debug) << "keep: " << (_keep ? "yes" : "no");

  PLOG(debug) << "face: " << (_face ? _face->name() : "nullptr");


  PDCELHalfEdge *he = _incident_edge;

  PLOG(debug) << "half edges:";
  do {
    // he->print2();
    PLOG(debug) << he->printString();
    he = he->next();
  } while (he != _incident_edge);


  PLOG(debug) << "adjacent loop:" << (_adjacent_loop ? _adjacent_loop->incidentEdge()->printBrief() : "nullptr");

}









void PDCELHalfEdgeLoop::print() {
  std::cout << "direction: ";
  if (direction() == 1) {
    std::cout << "outer" << std::endl;
  } else if (direction() == -1) {
    std::cout << "inner" << std::endl;
  }

  std::cout << "keep: ";
  if (_keep) {
    std::cout << "yes" << std::endl;
  } else {
    std::cout << "no" << std::endl;
  }

  std::cout << "face: ";
  if (_face != nullptr) {
    std::cout << _face->name() << std::endl;
  } else {
    std::cout << "nullptr" << std::endl;
  }

  PDCELHalfEdge *he = _incident_edge;
  std::cout << "half edges:" << std::endl;
  do {
    he->print2();
    he = he->next();
  } while (he != _incident_edge);
  // std::cout << std::endl;

  std::cout << "adjacent loop:";
  if (_adjacent_loop == nullptr) {
    std::cout << " nullptr" << std::endl;
  }
  else {
    _adjacent_loop->incidentEdge()->print();
  }

  std::cout << std::endl;
}









int PDCELHalfEdgeLoop::direction() {
  if (_direction == 0) {
    SVector3 sv1, sv2, sv0;
    sv1 = _incident_edge->prev()->toVector();
    sv2 = _incident_edge->toVector();

    sv0 = crossprod(sv1, sv2);
    _direction = sv0.x() > 0 ? 1 : -1;

    // return sv0.x() > 0 ? 1 : -1;
  }
  return _direction;
}









void PDCELHalfEdgeLoop::setFace(PDCELFace *f) {
  _face = f;
  _incident_edge->setIncidentFace(f);
}









void PDCELHalfEdgeLoop::updateVertexEdge(PDCELHalfEdge *he) {
  PLOG(debug) << "updateVertexEdge: " << he;

  if (_incident_edge == nullptr) {
    PLOG(debug) << "  setting incident edge for the first time";
    _incident_edge = he;
    _bottom_left_vertex = he->source();
  } else if (he->source()->point() < _bottom_left_vertex->point()) {
    PLOG(debug) << "  setting incident edge to a smaller one";
    _incident_edge = he;
    _bottom_left_vertex = he->source();
  }

  PLOG(debug) << "done";
}

void PDCELHalfEdgeLoop::write_to_file(std::ofstream& file) {
  file << "direction: ";
  if (direction() == 1) {
    file << "outer" << std::endl;
  } else if (direction() == -1) {
    file << "inner" << std::endl;
  }

  file << "keep: ";
  if (_keep) {
    file << "yes" << std::endl;
  } else {
    file << "no" << std::endl;
  }

  file << "face: ";
  if (_face != nullptr) {
    file << _face->name() << std::endl;
  } else {
    file << "nullptr" << std::endl;
  }

  PDCELHalfEdge *he = _incident_edge;
  file << "half edges:" << std::endl;
  do {
    he->write_to_file(file);
    he = he->next();
  } while (he != _incident_edge);

  file << "adjacent loop:";
  if (_adjacent_loop == nullptr) {
    file << " nullptr" << std::endl;
  }
  else {
    _adjacent_loop->incidentEdge()->write_to_file(file);
  }

  file << std::endl;
}
