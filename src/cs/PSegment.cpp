#include "PSegment.hpp"

#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

// void calcBoundVertices(std::vector<PDCELVertex *> &, SVector3 &, SVector3 &,
// Layup *);

std::ostream &operator<<(std::ostream &out, Segment *s) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << s->_name << std::setw(16) << s->_curve_base->getName()
      << std::setw(32) << s->_layup->getName() << std::setw(16) << s->slayupside
      << std::setw(8) << s->slevel;
  return out;
}










void Segment::print() {
  std::cout << "segment: " << _name << std::endl;
  std::cout << "prev segment: ";
  if (_prev) {
    std::cout << _prev->getName() << std::endl;
  } else {
    std::cout << "nullptr" << std::endl;
  }
  std::cout << "prev bound vector: " << _prev_bound << std::endl;
  std::cout << "prev bound vertices: " << std::endl;
  for (int i = 0; i < _prev_bound_vertices.size(); ++i) {
    std::cout << " " << _prev_bound_vertices[i];
    for (int vi : _prev_bound_indices) {
      if (i == vi) {
        std::cout << " - layer interface";
      }
    }
    std::cout << std::endl;
  }

  std::cout << "next segment: ";
  if (_next) {
    std::cout << _next->getName() << std::endl;
  } else {
    std::cout << "nullptr" << std::endl;
  }
  std::cout << "next bound vector: " << _next_bound << std::endl;
  std::cout << "next bound vertices: " << std::endl;
  for (int i = 0; i < _next_bound_vertices.size(); ++i) {
    std::cout << " " << _next_bound_vertices[i];
    for (int vi : _next_bound_indices) {
      if (i == vi) {
        std::cout << " - layer interface";
      }
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}









void Segment::printBaseOffsetLink() {
  std::size_t n = _curve_base->vertices().size();
  std::cout << "\nsegment " << _name << std::endl;
  std::cout << "base vertices: " << _curve_base->vertices().size() << std::endl;
  std::cout << "offset vertices: " << _curve_offset->vertices().size() << std::endl;
  std::cout << "base vertex -- link to, offset vertex\n";
  for (auto k = 0; k < n; k++) {
    std::cout << k << ": " << _curve_base->vertices()[k]
    << " -- " << _offset_indices_base_link_to[k];
    if (k < _curve_offset->vertices().size()) {
      std::cout << ", " << k << ": " << _curve_offset->vertices()[k];
    }
    std::cout << std::endl;
  }
}









void Segment::printBaseOffsetPairs(Message *pmessage) {

  PLOG(debug) << pmessage->message("base vertices -- base_link_to_offset_indices");

  // std::cout << _base_offset_indices_pairs.size() << std::endl;
  PLOG(debug) << pmessage->message("number of pairs: " + std::to_string(_base_offset_indices_pairs.size()));

  for (auto i = 0; i < _base_offset_indices_pairs.size(); i++) {
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    std::string s = std::to_string(_base_offset_indices_pairs[i][0]) + ": "
      + _curve_base->vertices()[_base_offset_indices_pairs[i][0]]->printString()
      + " -- " + std::to_string(_base_offset_indices_pairs[i][1]) + ": "
      + _curve_offset->vertices()[_base_offset_indices_pairs[i][1]]->printString();

    // std::cout << s << std::endl;
    PLOG(debug) << pmessage->message(s);
  }

}










int Segment::layupSide() {
  if (slayupside == "left") {
    return 1;
  } else if (slayupside == "right") {
    return -1;
  }
  return 0;
}


PDCELVertex *Segment::getBeginVertex() {
  return _curve_base->vertices().front();
}


PDCELVertex *Segment::getEndVertex() { return _curve_base->vertices().back(); }


SVector3 Segment::getBeginTangent() {
  return SVector3(_curve_base->vertices()[0]->point(),
                  _curve_base->vertices()[1]->point());
}


SVector3 Segment::getEndTangent() {
  std::size_t n = _curve_base->vertices().size();
  return SVector3(_curve_base->vertices()[n - 2]->point(),
                  _curve_base->vertices()[n - 1]->point());
}


void Segment::addArea(PArea *area) { _areas.push_back(area); }


void Segment::setPrevBoundVertices(std::vector<PDCELVertex *> vertices) {
  _prev_bound_vertices = vertices;
}


void Segment::setNextBoundVertices(std::vector<PDCELVertex *> vertices) {
  _next_bound_vertices = vertices;
}










void Segment::offsetCurveBase(Message *pmessage) {

  PLOG(debug) << "offsetting the base curve of segment: " << _name;


  if (_curve_base->vertices().front() == _curve_base->vertices().back()) {
    _closed = true;
  }

  int side = 1;
  if (slayupside == "right") {
    side = -1;
  }


  // New offset function
  _curve_offset = new Baseline();
  offset_2(_curve_base->vertices(), side, _layup->getTotalThickness(),
         _curve_offset->vertices(), pmessage);


  // Old offset function
  // _curve_offset = new Baseline();
  // offset(_curve_base->vertices(), side, _layup->getTotalThickness(),
  //        _curve_offset->vertices(), _offset_indices_base_link_to,
  //        _base_offset_indices_pairs, pmessage);

  PLOG(debug) << pmessage->message("base line: ")
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  PLOG(debug) << pmessage->message("offset line: ")
    << _curve_offset->vertices().front()->printString() << " -> "
    << _curve_offset->vertices().back()->printString();

  updateBaseOffsetIndexPairs(pmessage);

  PLOG(debug) << "done";
}









/// @brief Update the base-offset index pairs
/// @param pmessage 
void Segment::updateBaseOffsetIndexPairs(Message *pmessage) {
  pmessage->increaseIndent();

  PLOG(info) << pmessage->message("updating base-offset index pairs");

  // Check if curves exist
  if (!_curve_base || !_curve_offset) {
    PLOG(error) << pmessage->message("cannot update base-offset index pairs: curves not initialized");
    pmessage->decreaseIndent();
    return;
  }

  // Check if vertices exist
  if (_curve_base->vertices().empty() || _curve_offset->vertices().empty()) {
    PLOG(error) << pmessage->message("cannot update base-offset index pairs: empty vertex lists");
    pmessage->decreaseIndent();
    return;
  }

  // Clear the existing pairs
  _base_offset_indices_pairs.clear();

  // Create new pairs
  _base_offset_indices_pairs = create_polyline_vertex_pairs(
    _curve_base->vertices(), _curve_offset->vertices());

  // Check if the first pair is (0, 0), if not, add it
  if (_base_offset_indices_pairs.front()[0] != 0 && 
      _base_offset_indices_pairs.front()[1] != 0) {
    _base_offset_indices_pairs.insert(_base_offset_indices_pairs.begin(), {0, 0});
  }

  // Get the last indices for base and offset curves
  size_t last_base_idx = _curve_base->vertices().size() - 1;
  size_t last_offset_idx = _curve_offset->vertices().size() - 1;

  // Check if the last pair contains the last vertices, if not, add it
  if (_base_offset_indices_pairs.back()[0] != static_cast<int>(last_base_idx) || 
      _base_offset_indices_pairs.back()[1] != static_cast<int>(last_offset_idx)) {
    _base_offset_indices_pairs.push_back({
      static_cast<int>(last_base_idx),
      static_cast<int>(last_offset_idx)
    });
  }


  // Log the updated pairs
  PLOG(debug) << pmessage->message("updated base-offset index pairs:");
  for (const auto &pair : _base_offset_indices_pairs) {
    PLOG(debug) << pmessage->message(std::to_string(pair[0]) + " -- " + std::to_string(pair[1]));
  }

  pmessage->decreaseIndent();
}




















void Segment::build(Message *pmessage) {
  // pmessage->increaseIndent();

  PLOG(debug) << pmessage->message("building the overall shape of segment: " + _name);

  PDCELHalfEdge *he;

  PLOG(debug) << pmessage->message("creating half edges for the base curve");

  // Log the number of vertices of the base curve
  PLOG(debug) << "base curve:\n"
              << vertices_to_string(_curve_base->vertices());

  // _pmodel->dcel()->write_dcel_to_file("_tmp_dcel.txt");

  for (auto i = 0; i < _curve_base->vertices().size() - 1; ++i) {

    // Debug log the two vertices i and i+1
    PLOG(debug) << "  half edge: v[i] "
      << i << " " << _curve_base->vertices()[i]
      << " -- v[i+1] " << i + 1 << " " << _curve_base->vertices()[i + 1];

    he = _pmodel->dcel()->findHalfEdge(_curve_base->vertices()[i],
                                       _curve_base->vertices()[i + 1]);

    if (he == nullptr) {
      _pmodel->dcel()->addEdge(_curve_base->vertices()[i],
                               _curve_base->vertices()[i + 1]);
    }

  }
  // _pmodel->dcel()->print_dcel();


  PLOG(debug) << pmessage->message("creating half edges for the offset curve");

  for (int i = 0; i < _curve_offset->vertices().size() - 1; ++i) {
    _pmodel->dcel()->addEdge(_curve_offset->vertices()[i],
                             _curve_offset->vertices()[i + 1]);
  }

  // Create outer half edge loop and face

  PLOG(debug) << pmessage->message("creating the half edge loop and face");

  PDCELHalfEdgeLoop *hel;
  he = _pmodel->dcel()->findHalfEdge(_curve_base->vertices()[0],
                                     _curve_base->vertices()[1]);

  if (slayupside == "right") {
    he = he->twin();
  }

  hel = _pmodel->dcel()->addHalfEdgeLoop(he);

  hel->log();

  _face = _pmodel->dcel()->addFace(hel);
  _face->setName(_name + "_face");

  _face->print();

  hel->setKeep(true);
  hel->setFace(_face);

  // Set the half edge loop for the twin half edge
  // PDCELHalfEdgeLoop *hel_twin = _pmodel->dcel()->update_half_edge_loop(he->twin());
  // _pmodel->dcel()->link_inner_half_edge_loop(hel_twin);
  // PLOG(debug) << "hel_twin: ";
  // hel_twin->log();

  // _pmodel->dcel()->write_dcel_to_file("_tmp_dcel.txt");

  // pmessage->decreaseIndent();
}









// ===================================================================
//                                                       Class Filling
void Filling::setLayerType(LayerType *p_layertype) {
  p_flayertype = p_layertype;
}
