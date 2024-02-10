#include "PComponent.hpp"

#include "Material.hpp"
#include "PDCEL.hpp"
#include "PGeoClasses.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>

void PComponent::print() {
  std::cout << "name: " << _name << " | "
            << "order: " << _order << " | "
            << "cyclic: " << (_cycle ? "true" : "false") << std::endl;
}

void PComponent::print(Message *pmessage, int i_type, int i_indent) {
  pmessage->print(i_type, "name: " + _name);
  pmessage->print(i_type, "order: " + std::to_string(_order));
  pmessage->print(i_type, "cyclic: " + std::to_string(_cycle));
  pmessage->print(i_type, "segments:");
  std::stringstream ss;
  ss << std::setw(4) << "no." << std::setw(16) << "name"
     << std::setw(16) << "base line"
     << std::setw(32) << "layup"
     << std::setw(16) << "side"
     << std::setw(8) << "level";
  pmessage->print(i_type, ss.str());
  for (int i = 0; i < _segments.size(); i++) {
    std::stringstream ss;
    ss << std::setw(4) << (i+1) << _segments[i];
    pmessage->print(i_type, ss.str());
  }
  return;
}

int PComponent::order() {
  if (_order == 0) {
    // Update the order
    if (_dependencies.empty()) {
      _order = 1;
    } else {
      for (auto dc : _dependencies) {
        _order = std::max(_order, dc->order());
      }
      _order += 1;
    }
  }

  return _order;
}

void PComponent::setName(std::string name) { _name = name; }

void PComponent::addSegment(Segment *s) { _segments.push_back(s); }

void PComponent::setOrder(int order) { _order = order; }

void PComponent::addDependent(PComponent *component) {
  _dependencies.push_back(component);
}









void PComponent::build(Message *pmessage) {

  i_indent++;
  pmessage->increaseIndent();

  PLOG(info) << pmessage->message("building component: " + _name);

  // Laminate type component
  if (_type == 1) {

    buildLaminate(pmessage);

  }


  // Fill type component
  else if (_type == 2) {

    buildFilling(pmessage);

  }

  i_indent--;
  pmessage->decreaseIndent();

}









void PComponent::buildDetails(Message *pmessage) {

  i_indent++;
  pmessage->increaseIndent();

  if (_type == 1) {

    PLOG(info) << pmessage->message("building component details: " + _name);

    for (auto sgm : _segments) {

      sgm->buildAreas(pmessage);

      if (config.debug) _pmodel->plotGeoDebug(pmessage);

    }

  }

  i_indent--;
  pmessage->decreaseIndent();

}









// ===================================================================
//
// Non-Member Functions
//
// ===================================================================

bool compareOrder(PComponent *c1, PComponent *c2) {
  return c1->order() < c2->order();
}
