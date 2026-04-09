#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <iostream>


void PDCELHalfEdgeLoop::log() {
  std::stringstream ss;

  // ss << "direction: ";
  // if (direction() == 1) {
  //   ss << "outer" << std::endl;
  // } else if (direction() == -1) {
  //   ss << "inner" << std::endl;
  // }
  PLOG(debug) << "direction: " << ((direction() == 1) ? "outer" : "inner");


  // ss.str("");
  // ss << "face: ";
  // if (_face != nullptr) {
  //   ss << _face->name() << std::endl;
  // } else {
  //   ss << "nullptr" << std::endl;
  // }
  PLOG(debug) << "face: " << (_face ? _face->name() : "nullptr");

  PDCELHalfEdge *he = _incident_edge;
  // std::cout << "half edges:" << std::endl;
  PLOG(debug) << "half edges:";
  do {
    // he->print2();
    PLOG(debug) << he->printString();
    he = he->next();
  } while (he != _incident_edge);
}









void PDCELHalfEdgeLoop::print() {
  std::cout << "direction: ";
  if (direction() == 1) {
    std::cout << "outer" << std::endl;
  } else if (direction() == -1) {
    std::cout << "inner" << std::endl;
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









PDCELVertex *PDCELHalfEdgeLoop::bottomLeftVertex() {
  return _incident_edge ? _incident_edge->source() : nullptr;
}

void PDCELHalfEdgeLoop::updateIncidentEdge(PDCELHalfEdge *he) {
  if (_incident_edge == nullptr ||
      he->source()->point() < _incident_edge->source()->point()) {
    _incident_edge = he;
  }
}
