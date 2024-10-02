#include "PDCELHalfEdge.hpp"

#include "overloadOperator.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "gmsh_mod/SVector3.h"
#include "gmsh/GEdge.h"

#include <cmath>
#include <cstdio>
#include <iostream>




std::ostream &operator<<(std::ostream &out, PDCELHalfEdge *he) {
  out << he->_source << " -> " << he->_twin->_source << " sign: " << he->_sign;
  return out;
}









std::string PDCELHalfEdge::printString() {
  std::stringstream ss;

  // ss << _source->point2() << " -> " << _twin->_source->point2();
  ss << _source << " -> " << _twin->_source;

  // ss << " | loop: ";
  // if (_loop == nullptr) {
  //   ss << "nullptr";
  // } else {
  //   if (_loop->keep()) {
  //     ss << "keep";
  //   } else {
  //     ss << "temp";
  //   }
  // }
  ss << " | loop: " << (_loop ? (_loop->keep() ? "keep" : "temp") : "nullptr");

  // ss << " | face: ";
  // if (_face == nullptr) {
  //   ss << "nullptr";
  // } else {
  //   ss << _face->name();
  // }
  ss << " | face: " << (_face ? _face->name() : "nullptr");
  
  return ss.str();
}









std::string PDCELHalfEdge::printBrief() {
  std::stringstream ss;

  ss << _source << " -> " << _twin->_source << " sign: " << _sign;

  return ss.str();
}









void PDCELHalfEdge::print() {
  std::cout << _source << " -> " << _twin->_source
            << " sign: " << _sign << std::endl;
}









void PDCELHalfEdge::print2() {
  std::cout << _source->point2() << " -> " << _twin->_source->point2();

  // std::cout << " | sign: " << _sign;
  
  // std::cout << " | angle: " << angle();

  std::cout << " | loop: ";
  if (_loop == nullptr) {
    std::cout << "nullptr";
  } else {
    if (_loop->keep()) {
      std::cout << "keep";
    } else {
      std::cout << "temp";
    }
  }

  std::cout << " | face: ";
  if (_face == nullptr) {
    std::cout << "nullptr";
  } else {
    std::cout << _face->name();
  }

  // std::cout << " | address: ";
  // printf("%p", (void *)this);

  std::cout << std::endl;
}









bool PDCELHalfEdge::isFinite() {
  return (_source->isFinite() && _twin->source()->isFinite());
}

SVector3 PDCELHalfEdge::toVector() {
  return SVector3(_source->point(), _twin->source()->point());
}

PGeoLineSegment *PDCELHalfEdge::toLineSegment() {
  return new PGeoLineSegment(_source, _twin->source());
}

double PDCELHalfEdge::angle() {
  return atan2(toVector()[2], toVector()[1]);
}

void PDCELHalfEdge::setLoop(PDCELHalfEdgeLoop *hel) {
  _loop = hel;
  hel->updateVertexEdge(this);
}
