#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELUtils.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <iostream>
#include <sstream>

std::string PDCELHalfEdgeLoop::label() const {
  std::stringstream ss;
  ss << "loop#" << _id;
  return ss.str();
}

void PDCELHalfEdgeLoop::log() {
  PLOG(debug) << label() << " direction: "
              << ((direction() == 1) ? "outer" : "inner");
  PLOG(debug) << label() << " face: "
              << (_face ? _face->displayLabel() : "nullptr");
  PLOG(debug) << label() << " half edges:";
  walkLoopWithLimit(_incident_edge, [](PDCELHalfEdge *he) {
    PLOG(debug) << he->printString();
  });
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
    std::cout << _face->displayLabel() << std::endl;
  } else {
    std::cout << "nullptr" << std::endl;
  }

  std::cout << "half edges:" << std::endl;
  walkLoopWithLimit(_incident_edge, [](PDCELHalfEdge *he) {
    he->print2();
  });

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
