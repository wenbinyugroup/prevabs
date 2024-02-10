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

#include "gmsh/SPoint3.h"
#include "gmsh/SVector3.h"

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
  int n = _curve_base->vertices().size();
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
  int n = _curve_base->vertices().size();
  return SVector3(_curve_base->vertices()[n - 2]->point(),
                  _curve_base->vertices()[n - 1]->point());
}

// void Segment::setPModel(PModel *pmodel) { _pmodel = pmodel; }








void Segment::addArea(PArea *area) { _areas.push_back(area); }

// void Segment::setLevel(int level) { slevel = level; }

// void Segment::setPrevSegment(Segment *prev) { _prev = prev; }

// void Segment::setNextSegment(Segment *next) { _next = next; }

// void Segment::setPrevBound(SVector3 &prev) { _prev_bound = prev; }

// void Segment::setNextBound(SVector3 &next) { _next_bound = next; }

void Segment::setPrevBoundVertices(std::vector<PDCELVertex *> vertices) {
  _prev_bound_vertices = vertices;
}










void Segment::setNextBoundVertices(std::vector<PDCELVertex *> vertices) {
  _next_bound_vertices = vertices;
}










void Segment::offsetCurveBase(Message *pmessage) {
  pmessage->increaseIndent();
  // if (config.debug) {
  //   pmessage->print(9, "offsetting the base curve of segment: " + _name);
  // }
  PLOG(debug) << pmessage->message("offsetting the base curve of segment: " + _name);
  // pmessage->print(9, "offsetting the base curve of segment: " + _name);

  // std::cout << "\n[debug] base line:" << std::endl;
  // for (auto v : _curve_base->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  if (_curve_base->vertices().front() == _curve_base->vertices().back()) {
    _closed = true;
  }

  int side = 1;
  if (slayupside == "right") {
    side = -1;
  }


  // New offset function
  // _curve_offset = new Baseline();
  // offset2(_curve_base->vertices(), side, _layup->getTotalThickness(),
  //        _curve_offset->vertices(), _offset_indices_base_link_to);


  // Old offset function
  _curve_offset = new Baseline();
  offset(_curve_base->vertices(), side, _layup->getTotalThickness(),
         _curve_offset->vertices(), _offset_indices_base_link_to,
         _base_offset_indices_pairs, pmessage);


  // if (config.debug) {
  //   std::cout << "base line: " <<  _curve_base->vertices().front();
  //   std::cout << " -> " <<  _curve_base->vertices().back() << std::endl;
  //   std::cout << "offset line: " <<  _curve_offset->vertices().front();
  //   std::cout << " -> " <<  _curve_offset->vertices().back() << std::endl;
  // }
  PLOG(debug) << pmessage->message("base line: ")
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  PLOG(debug) << pmessage->message("offset line: ")
    << _curve_offset->vertices().front()->printString() << " -> "
    << _curve_offset->vertices().back()->printString();

  // std::cout << "\n[debug] curve _curve_offset:" << std::endl;
  // for (auto v : _curve_offset->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  // std::cout << "        _offset_vertices_link_to:" << std::endl;
  // for (auto i = 0; i < _offset_vertices_link_to.size(); i++) {
  //   std::cout << "        " << i << " -- " << _offset_vertices_link_to[i] << std::endl;
  // }

  // std::cout << "        base-offset linking indices:" << std::endl;
  // for (auto i = 0; i < _offset_indices_base_link_to.size(); i++) {
  //   std::cout << "        " << i << " -- " << _offset_indices_base_link_to[i] << std::endl;
  // }
  pmessage->decreaseIndent();
}




















void Segment::build(Message *pmessage) {
  pmessage->increaseIndent();
  if (config.debug) {
    // std::cout << "[debug] building the overall shape of segment: " << _name
    //           << std::endl;
    // fprintf(config.fdeb, "- building segment areas: %s\n", _name.c_str());
    // pmessage->print(9, "building the overall shape of segment: " + _name);
    PLOG(debug) << pmessage->message("building the overall shape of segment: " + _name);
    // std::cout << "base line: " <<  _curve_base->vertices().front();
    // std::cout << " -> " <<  _curve_base->vertices().back() << std::endl;
    PLOG(debug) << pmessage->message("base line: ")
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  }
  // pmessage->print(9, "building the overall shape of segment: " + _name);

  // printBaseOffsetLink();


  PDCELHalfEdge *he;

  // std::cout << "[debug] creating half edges for the base curve" << std::endl;
  // if (config.debug) {
  //   pmessage->print(9, "creating half edges for the base curve");
  // }
  PLOG(debug) << pmessage->message("creating half edges for the base curve");
  // pmessage->print(9, "creating half edges for the base curve");
  for (int i = 0; i < _curve_base->vertices().size() - 1; ++i) {
    he = _pmodel->dcel()->findHalfEdge(_curve_base->vertices()[i],
                                       _curve_base->vertices()[i + 1]);
    if (he == nullptr) {
      _pmodel->dcel()->addEdge(_curve_base->vertices()[i],
                               _curve_base->vertices()[i + 1]);
    }
  }
  // _pmodel->dcel()->print_dcel();

  // std::cout << "[debug] creating half edges for the offset curve" <<
  // std::endl;
  // if (config.debug) {
  //   pmessage->print(9, "creating half edges for the offset curve");
  // }
  PLOG(debug) << pmessage->message("creating half edges for the offset curve");
  // pmessage->print(9, "creating half edges for the offset curve");
  for (int i = 0; i < _curve_offset->vertices().size() - 1; ++i) {
    _pmodel->dcel()->addEdge(_curve_offset->vertices()[i],
                             _curve_offset->vertices()[i + 1]);
  }

  // Create half edge loop and face
  // if (config.debug) {
  //   pmessage->print(9, "creating the half edge loop and face");
  // }
  PLOG(debug) << pmessage->message("creating the half edge loop and face");
  // pmessage->print(9, "creating the half edge loop and face");
  PDCELHalfEdgeLoop *hel;
  he = _pmodel->dcel()->findHalfEdge(_curve_base->vertices()[0],
                                     _curve_base->vertices()[1]);
  if (slayupside == "right") {
    he = he->twin();
  }

  // std::cout << "[debug] half edges of vertex " << he->target() << std::endl;
  // he->target()->printAllLeavingHalfEdges();

  // std::cout << "\n[debug] half edge he: " << he << std::endl;
  hel = _pmodel->dcel()->addHalfEdgeLoop(he);
  // std::cout << "\nhalf edge loop:\n";
  // std::cout << hel << std::endl;

  _face = _pmodel->dcel()->addFace(hel);
  _face->setName(_name + "_face");
  // std::cout << "        face _face:" << std::endl;
  // _face->print();

  hel->setKeep(true);
  hel->setFace(_face);

  // for (auto f : _pmodel->dcel()->faces()) {
  //   f->outer()->print2();
  // }
  pmessage->decreaseIndent();
}









// ===================================================================
//                                                       Class Filling
void Filling::setLayerType(LayerType *p_layertype) {
  p_flayertype = p_layertype;
}
