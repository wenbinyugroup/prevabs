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

int PComponent::count_tmp = 0;

void PComponent::print() {
  std::cout << "name: " << _name << " | "
            << "order: " << _order << " | "
            << "cyclic: " << (_cycle ? "true" : "false") << std::endl;
}

void PComponent::print(int i_type, int /*i_indent*/) {
  MESSAGE_SCOPE(g_msg);
  PLOG(debug) << g_msg->message("name: " + _name);
  PLOG(debug) << g_msg->message("order: " + std::to_string(_order));
  PLOG(debug) << g_msg->message("cyclic: " + std::to_string(_cycle));
  PLOG(debug) << g_msg->message("segments:");
  std::stringstream ss;
  ss << std::setw(4) << "no." << std::setw(16) << "name"
     << std::setw(16) << "base line"
     << std::setw(32) << "layup"
     << std::setw(16) << "side"
     << std::setw(8) << "level";
  PLOG(debug) << g_msg->message(ss.str());
  for (int i = 0; i < _segments.size(); i++) {
    std::stringstream ss_seg;
    ss_seg << std::setw(4) << (i+1) << _segments[i];
    PLOG(debug) << g_msg->message(ss_seg.str());
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









void PComponent::build(const BuilderConfig &bcfg) {

  // i_indent++;
  MESSAGE_SCOPE(g_msg);

  PLOG(info) << g_msg->message("building component: " + _name);

  // Laminate type component
  if (_type == 1) {

    buildLaminate(bcfg);

  }


  // Fill type component
  else if (_type == 2) {

    buildFilling(bcfg);

  }

  // i_indent--;

}









void PComponent::buildDetails(const BuilderConfig &bcfg) {

  MESSAGE_SCOPE(g_msg);

  if (_type == 1) {

    PLOG(info) << g_msg->message("building component details: " + _name);

    for (auto sgm : _segments) {

      sgm->buildAreas(bcfg);

      if (bcfg.debug && bcfg.plotDebug) bcfg.plotDebug(g_msg);

    }

  }

}









// ===================================================================
//
// Non-Member Functions
//
// ===================================================================

bool compareOrder(PComponent *c1, PComponent *c2) {
  return c1->order() < c2->order();
}
