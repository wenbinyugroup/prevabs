#include "PDCELHalfEdge.hpp"

#include "overloadOperator.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo_types.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>

std::string PDCELHalfEdge::label() const {
  std::stringstream ss;
  ss << "he#" << _id;
  return ss.str();
}

std::ostream &operator<<(std::ostream &out, PDCELHalfEdge *he) {
  out << he->label() << " "
      << he->_source << " -> " << he->_twin->_source
      << " sign: " << he->_sign;
  return out;
}

std::string PDCELHalfEdge::printString() {
  std::stringstream ss;

  ss << label() << " "
     << _source << " -> " << _twin->_source;

  ss << " | loop: " << (_loop ? _loop->label() : "nullptr");

  ss << " | face: " << (_face ? _face->label() : "nullptr");
  
  return ss.str();
}

std::string PDCELHalfEdge::printBrief() {
  std::stringstream ss;

  ss << label() << " "
     << _source << " -> " << _twin->_source << " sign: " << _sign;

  return ss.str();
}

void PDCELHalfEdge::print() {
  std::cout << label() << " "
            << _source << " -> " << _twin->_source
            << " sign: " << _sign << std::endl;
}

void PDCELHalfEdge::print2() {
  std::cout << label() << " "
            << _source->point2() << " -> " << _twin->_source->point2();

  // std::cout << " | sign: " << _sign;
  
  // std::cout << " | angle: " << angle();

  std::cout << " | loop: ";
  if (_loop == nullptr) {
    std::cout << "nullptr";
  } else {
    std::cout << _loop->label();
  }

  std::cout << " | face: ";
  if (_face == nullptr) {
    std::cout << "nullptr";
  } else {
    std::cout << _face->label();
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
  if (hel) hel->updateIncidentEdge(this);
}
