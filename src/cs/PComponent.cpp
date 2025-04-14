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
#include "PModel.hpp"

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

  // pmessage->increaseIndent();

  // PLOG(info) << pmessage->message("building component: " + _name);

  // Laminate type component
  if (_type == 1) {

    buildLaminate(pmessage);

  }


  // Fill type component
  else if (_type == 2) {

    buildFilling(pmessage);

  }




  // Update the DCEL (half edge loops)

  // Set the half edge loop for the twin half edge
  Segment *sgm = _segments[0];
  std::string slayupside = sgm->getLayupside();

  // Use the base curve of the first segment
  PDCELHalfEdge *he_base = _pmodel->dcel()->findHalfEdge(
    sgm->curveBase()->vertices()[0],
    sgm->curveBase()->vertices()[1]
  );
  if (slayupside == "right") {
    he_base = he_base->twin();
  }
  PLOG(debug) << "he_base: " << he_base;
  PDCELHalfEdgeLoop *hel_base_twin = _pmodel->dcel()->update_half_edge_loop(he_base->twin());
  if (hel_base_twin->direction() > 0) {
    // If this is an outer loop, check if there is a face attached to it
    if (hel_base_twin->face() == nullptr) {
      PDCELFace *fnew = _pmodel->dcel()->addFace(hel_base_twin);
      fnew->setName("_temp");
      fnew->setGBuild(false);
    }
  }
  else if (hel_base_twin->direction() < 0) {
    // If this is an inner loop, link it to the outer loop
    _pmodel->dcel()->link_inner_half_edge_loop(hel_base_twin);
  }
  PLOG(debug) << "hel_base_twin: ";
  hel_base_twin->log();

  // Check the offset curve of the first segment
  PDCELHalfEdge *he_offset = _pmodel->dcel()->findHalfEdge(
    sgm->curveOffset()->vertices()[0],
    sgm->curveOffset()->vertices()[1]
  );
  if (slayupside == "left") {
    he_offset = he_offset->twin();
  }
  PLOG(debug) << "he_offset: " << he_offset;
  PDCELHalfEdgeLoop *hel_offset_twin = _pmodel->dcel()->update_half_edge_loop(he_offset->twin());
  if (hel_offset_twin->direction() > 0) {
    // If this is an outer loop, check if there is a face attached to it
    if (hel_offset_twin->face() == nullptr) {
      PDCELFace *fnew = _pmodel->dcel()->addFace(hel_offset_twin);
      fnew->setName("_temp");
      fnew->setGBuild(false);
    }
  }
  else if (hel_offset_twin->direction() < 0) {
    // If this is an inner loop, link it to the outer loop
    _pmodel->dcel()->link_inner_half_edge_loop(hel_offset_twin);
  }
  PLOG(debug) << "hel_offset_twin: ";
  hel_offset_twin->log();




  // Check DCEL for debug
  if (config.debug) {
    PLOG(debug) << "writing DCEL to file after building segments";
    _pmodel->dcel()->write_dcel_to_file(
      config.file_directory 
      + config.file_base_name
      + "_dcel_after_building_general_shape_component_"
      + _name
      + ".txt"
    );
  }

  // pmessage->decreaseIndent();

}









void PComponent::buildDetails(Message *pmessage) {

  // i_indent++;
  pmessage->increaseIndent();

  if (_type == 1) {

    PLOG(info) << pmessage->message("building component details: " + _name);

    for (auto sgm : _segments) {

      sgm->updateBaseOffsetIndexPairs(pmessage);

      sgm->buildAreas(pmessage);

      if (config.debug) _pmodel->plotGeoDebug(pmessage);

    }

  }

  // i_indent--;
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
