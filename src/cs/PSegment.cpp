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

#include "geo_types.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

int Segment::count_tmp = 0;

namespace {

const char *toString(Segment::LifecycleState state) {
  switch (state) {
  case Segment::LifecycleState::BaseReady:
    return "BaseReady";
  case Segment::LifecycleState::OffsetReady:
    return "OffsetReady";
  case Segment::LifecycleState::ShellBuilt:
    return "ShellBuilt";
  case Segment::LifecycleState::AreasBuilt:
    return "AreasBuilt";
  }
  return "Unknown";
}

void deleteUnregisteredVertices(const std::vector<PDCELVertex *> &vertices) {
  std::unordered_set<PDCELVertex *> visited;
  for (PDCELVertex *vertex : vertices) {
    if (vertex == nullptr) {
      continue;
    }
    if (!visited.insert(vertex).second) {
      continue;
    }
    if (!vertex->isRegistered()) {
      delete vertex;
    }
  }
}

} // namespace

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

Segment::~Segment() {
  releaseOwnedResources();
}

Segment::Segment(Segment &&other) noexcept
    : _name(std::move(other._name)),
      _curve_base(other._curve_base),
      _curve_offset(other._curve_offset),
      _u_begin(other._u_begin),
      _u_end(other._u_end),
      _layup(other._layup),
      _areas(std::move(other._areas)),
      slayupside(std::move(other.slayupside)),
      slevel(other.slevel),
      _prev(other._prev),
      _next(other._next),
      _prev_bound(other._prev_bound),
      _next_bound(other._next_bound),
      _closed(other._closed),
      _mat_orient_e1(std::move(other._mat_orient_e1)),
      _mat_orient_e2(std::move(other._mat_orient_e2)),
      _prev_bound_vertices(std::move(other._prev_bound_vertices)),
      _next_bound_vertices(std::move(other._next_bound_vertices)),
      _prev_bound_indices(std::move(other._prev_bound_indices)),
      _next_bound_indices(std::move(other._next_bound_indices)),
      _vertices_outer(std::move(other._vertices_outer)),
      _face(other._face),
      _free(other._free),
      _head_vertex_offset(other._head_vertex_offset),
      _tail_vertex_offset(other._tail_vertex_offset),
      _inner_bounds_end(std::move(other._inner_bounds_end)),
      _inner_bounds(std::move(other._inner_bounds)),
      _inner_bounds_tt(std::move(other._inner_bounds_tt)),
      _offset_vertices_link_to(std::move(other._offset_vertices_link_to)),
      _base_offset_indices_pairs(std::move(other._base_offset_indices_pairs)),
      _ib_begin(other._ib_begin),
      _ib_end(other._ib_end),
      _inner_bounds_dc(std::move(other._inner_bounds_dc)),
      _state(other._state) {
  other._curve_base = nullptr;
  other._curve_offset = nullptr;
  other._layup = nullptr;
  other._prev = nullptr;
  other._next = nullptr;
  other._face = nullptr;
  other._head_vertex_offset = nullptr;
  other._tail_vertex_offset = nullptr;
  other._areas.clear();
  other._state = LifecycleState::BaseReady;
}

Segment &Segment::operator=(Segment &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  releaseOwnedResources();

  _name = std::move(other._name);
  _curve_base = other._curve_base;
  _curve_offset = other._curve_offset;
  _u_begin = other._u_begin;
  _u_end = other._u_end;
  _layup = other._layup;
  _areas = std::move(other._areas);
  slayupside = std::move(other.slayupside);
  slevel = other.slevel;
  _prev = other._prev;
  _next = other._next;
  _prev_bound = other._prev_bound;
  _next_bound = other._next_bound;
  _closed = other._closed;
  _mat_orient_e1 = std::move(other._mat_orient_e1);
  _mat_orient_e2 = std::move(other._mat_orient_e2);
  _prev_bound_vertices = std::move(other._prev_bound_vertices);
  _next_bound_vertices = std::move(other._next_bound_vertices);
  _prev_bound_indices = std::move(other._prev_bound_indices);
  _next_bound_indices = std::move(other._next_bound_indices);
  _vertices_outer = std::move(other._vertices_outer);
  _face = other._face;
  _free = other._free;
  _head_vertex_offset = other._head_vertex_offset;
  _tail_vertex_offset = other._tail_vertex_offset;
  _inner_bounds_end = std::move(other._inner_bounds_end);
  _inner_bounds = std::move(other._inner_bounds);
  _inner_bounds_tt = std::move(other._inner_bounds_tt);
  _offset_vertices_link_to = std::move(other._offset_vertices_link_to);
  _base_offset_indices_pairs = std::move(other._base_offset_indices_pairs);
  _ib_begin = other._ib_begin;
  _ib_end = other._ib_end;
  _inner_bounds_dc = std::move(other._inner_bounds_dc);
  _state = other._state;

  other._curve_base = nullptr;
  other._curve_offset = nullptr;
  other._layup = nullptr;
  other._prev = nullptr;
  other._next = nullptr;
  other._face = nullptr;
  other._head_vertex_offset = nullptr;
  other._tail_vertex_offset = nullptr;
  other._areas.clear();
  other._state = LifecycleState::BaseReady;

  return *this;
}

void Segment::releaseOwnedResources() {
  for (PArea *area : _areas) {
    delete area;
  }
  _areas.clear();

  if (_curve_offset != nullptr) {
    deleteUnregisteredVertices(_curve_offset->vertices());
    delete _curve_offset;
    _curve_offset = nullptr;
  }
  _face = nullptr;
  _head_vertex_offset = nullptr;
  _tail_vertex_offset = nullptr;
  _state = LifecycleState::BaseReady;
}

bool Segment::requireBaseDefinition(const char *caller) const {
  const bool valid = (_curve_base != nullptr && _layup != nullptr);
  if (valid) {
    return true;
  }

  const std::string message =
      std::string("Segment::") + caller
      + " requires both base curve and layup for segment '"
      + _name + "'";
  PLOG(error) << message;
  assert(valid && "Segment requires both base curve and layup");
  return false;
}

bool Segment::requireStateAtLeast(
    LifecycleState minimum_state, const char *caller) const {
  if (static_cast<int>(_state) >= static_cast<int>(minimum_state)) {
    return true;
  }

  const std::string message =
      std::string("Segment::") + caller + " requires state >= "
      + toString(minimum_state) + " but current state is "
      + toString(_state) + " for segment '" + _name + "'";
  PLOG(error) << message;
  assert(static_cast<int>(_state) >= static_cast<int>(minimum_state));
  return false;
}

bool Segment::requireExactState(
    LifecycleState expected_state, const char *caller) const {
  if (_state == expected_state) {
    return true;
  }

  const std::string message =
      std::string("Segment::") + caller + " requires state "
      + toString(expected_state) + " but current state is "
      + toString(_state) + " for segment '" + _name + "'";
  PLOG(error) << message;
  assert(_state == expected_state);
  return false;
}

bool Segment::validateStateInvariants(const char *caller) const {
  if (!requireBaseDefinition(caller)) {
    return false;
  }

  const bool offset_ready = (_curve_offset != nullptr);
  const bool shell_built = (_face != nullptr);
  const bool areas_built = !_areas.empty();

  bool valid = true;
  switch (_state) {
  case LifecycleState::BaseReady:
    valid = !offset_ready && !shell_built && !areas_built;
    break;
  case LifecycleState::OffsetReady:
    valid = offset_ready && !shell_built && !areas_built;
    break;
  case LifecycleState::ShellBuilt:
    valid = offset_ready && shell_built && !areas_built;
    break;
  case LifecycleState::AreasBuilt:
    valid = offset_ready && shell_built && areas_built;
    break;
  }

  if (valid) {
    return true;
  }

  std::ostringstream oss;
  oss << "Segment::" << caller << " violates lifecycle invariants for segment '"
      << _name << "'"
      << " [state=" << toString(_state)
      << ", has_offset=" << offset_ready
      << ", has_face=" << shell_built
      << ", areas=" << _areas.size() << "]";
  PLOG(error) << oss.str();
  assert(valid && "Segment lifecycle invariants violated");
  return false;
}

bool Segment::requireOffsetCurve(const char *caller) const {
  if (!requireStateAtLeast(LifecycleState::OffsetReady, caller)) {
    return false;
  }
  return validateStateInvariants(caller);
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
  if (!requireOffsetCurve("printBaseOffsetLink")) {
    return;
  }

  std::vector<int> link_to_offset(
      _curve_base->vertices().size(), 0);
  for (const BaseOffsetPair &pair : _base_offset_indices_pairs) {
    if (pair.base >= 0
        && pair.base < static_cast<int>(link_to_offset.size())) {
      link_to_offset[pair.base] = pair.offset;
    }
  }

  std::cout << "\nsegment " << _name << std::endl;
  std::cout << "base vertices: " << _curve_base->vertices().size() << std::endl;
  std::cout << "offset vertices: " << _curve_offset->vertices().size() << std::endl;
  std::cout << "base vertex -- link to, offset vertex\n";
  for (std::size_t k = 0; k < link_to_offset.size(); k++) {
    std::cout << k << ": " << _curve_base->vertices()[k]
    << " -- " << link_to_offset[k];
    if (link_to_offset[k] >= 0
        && link_to_offset[k] < static_cast<int>(_curve_offset->vertices().size())) {
      std::cout << ", " << link_to_offset[k] << ": "
                << _curve_offset->vertices()[link_to_offset[k]];
    }
    std::cout << std::endl;
  }
}

void Segment::printBaseOffsetPairs() {
  MESSAGE_SCOPE(g_msg);
  if (!requireOffsetCurve("printBaseOffsetPairs")) {
    return;
  }

    PLOG(debug) << "base vertices -- base_link_to_offset_indices";

  // std::cout << _base_offset_indices_pairs.size() << std::endl;
    PLOG(debug) << "number of pairs: " + std::to_string(_base_offset_indices_pairs.size());

  for (auto i = 0; i < _base_offset_indices_pairs.size(); i++) {
    // std::cout << "        " << i << ": " << base[i]
    // << " -- " << link_to_2[i] << std::endl;
    const BaseOffsetPair &pair = _base_offset_indices_pairs[i];
    std::string s = std::to_string(pair.base) + ": "
      + _curve_base->vertices()[pair.base]->printString()
      + " -- " + std::to_string(pair.offset) + ": "
      + _curve_offset->vertices()[pair.offset]->printString();

    // std::cout << s << std::endl;
        PLOG(debug) << s;
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

void Segment::offsetCurveBase() {
  MESSAGE_SCOPE(g_msg);
  if (!requireBaseDefinition("offsetCurveBase")) {
    return;
  }
  if (_state == LifecycleState::ShellBuilt ||
      _state == LifecycleState::AreasBuilt) {
    const std::string message =
        "Segment::offsetCurveBase cannot run after shell/areas build for segment '"
        + _name + "'";
    PLOG(error) << message;
    assert(false && "offsetCurveBase cannot run after build/buildAreas");
    return;
  }

  // if (config.debug) {
  //   pmessage->print(9, "offsetting the base curve of segment: " + _name);
  // }
    PLOG(debug) << "offsetting the base curve of segment: " + _name;
  // pmessage->print(9, "offsetting the base curve of segment: " + _name);

  // std::cout << "\n[debug] base line:" << std::endl;
  // for (auto v : _curve_base->vertices()) {
  //   std::cout << "        " << v << std::endl;
  // }

  if (_curve_base->vertices().front() == _curve_base->vertices().back()) {
    _closed = true;
  }
  else {
    _closed = false;
  }

  if (_curve_offset != nullptr) {
    deleteUnregisteredVertices(_curve_offset->vertices());
    delete _curve_offset;
    _curve_offset = nullptr;
  }
  _base_offset_indices_pairs.clear();

  int side = 1;
  if (slayupside == "right") {
    side = -1;
  }

  // New offset function
  // _curve_offset = new Baseline();
  // offset2(_curve_base->vertices(), side, _layup->getTotalThickness(),
  //        _curve_offset->vertices());

  // Old offset function
  _curve_offset = new Baseline();
  offset(_curve_base->vertices(), side, _layup->getTotalThickness(),
         _curve_offset->vertices(), _base_offset_indices_pairs);
  _face = nullptr;
  _areas.clear();
  _head_vertex_offset = nullptr;
  _tail_vertex_offset = nullptr;
  _state = LifecycleState::OffsetReady;
  validateStateInvariants("offsetCurveBase");

  // if (config.debug) {
  //   std::cout << "base line: " <<  _curve_base->vertices().front();
  //   std::cout << " -> " <<  _curve_base->vertices().back() << std::endl;
  //   std::cout << "offset line: " <<  _curve_offset->vertices().front();
  //   std::cout << " -> " <<  _curve_offset->vertices().back() << std::endl;
  // }
  PLOG(debug) << "base line: "
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  PLOG(debug) << "offset line: "
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

}

void Segment::build(const BuilderConfig &bcfg) {
  MESSAGE_SCOPE(g_msg);
  if (!requireExactState(LifecycleState::OffsetReady, "build")) {
    return;
  }
  if (!validateStateInvariants("build")) {
    return;
  }

  if (bcfg.debug) {
    // std::cout << "[debug] building the overall shape of segment: " << _name
    //           << std::endl;
    // fprintf(config.fdeb, "- building segment areas: %s\n", _name.c_str());
    // pmessage->print(9, "building the overall shape of segment: " + _name);
        PLOG(debug) << "building the overall shape of segment: " + _name;
    // std::cout << "base line: " <<  _curve_base->vertices().front();
    // std::cout << " -> " <<  _curve_base->vertices().back() << std::endl;
    PLOG(debug) << "base line: "
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  }
  // pmessage->print(9, "building the overall shape of segment: " + _name);

  // printBaseOffsetLink();

  PDCELHalfEdge *he;

    PLOG(debug) << "creating half edges for the base curve";

  // Log the number of vertices of the base curve
    PLOG(debug) << "number of vertices of the base curve: " + std::to_string(_curve_base->vertices().size());

  for (auto i = 0; i < _curve_base->vertices().size() - 1; ++i) {

    // Debug log the two vertices i and i+1
        PLOG(debug) << "vertices: " + std::to_string(i) + " -- " + std::to_string(i + 1);

    he = bcfg.dcel->findHalfEdgeBetween(_curve_base->vertices()[i],
                                            _curve_base->vertices()[i + 1]);

    if (he == nullptr) {
      bcfg.dcel->addEdge(_curve_base->vertices()[i],
                               _curve_base->vertices()[i + 1]);
    }

  }
  // _pmodel->dcel()->print_dcel();

  // std::cout << "[debug] creating half edges for the offset curve" <<
  // std::endl;
  // if (config.debug) {
  //   pmessage->print(9, "creating half edges for the offset curve");
  // }
    PLOG(debug) << "creating half edges for the offset curve";
  // pmessage->print(9, "creating half edges for the offset curve");
  for (int i = 0; i < _curve_offset->vertices().size() - 1; ++i) {
    bcfg.dcel->addEdge(_curve_offset->vertices()[i],
                             _curve_offset->vertices()[i + 1]);
  }

  // Create half edge loop and face
  // if (config.debug) {
  //   pmessage->print(9, "creating the half edge loop and face");
  // }
    PLOG(debug) << "creating the half edge loop and face";
  // pmessage->print(9, "creating the half edge loop and face");
  PDCELHalfEdgeLoop *hel;
  he = bcfg.dcel->findHalfEdgeBetween(_curve_base->vertices()[0],
                                          _curve_base->vertices()[1]);
  if (slayupside == "right") {
    he = he->twin();
  }

  // std::cout << "[debug] half edges of vertex " << he->target() << std::endl;
  // he->target()->printAllLeavingHalfEdges();

  // std::cout << "\n[debug] half edge he: " << he << std::endl;
  hel = bcfg.dcel->addHalfEdgeLoop(he);
  // std::cout << "\nhalf edge loop:\n";
  // std::cout << hel << std::endl;

  _face = bcfg.dcel->addFace(hel);
  bcfg.model->faceData(_face).name = _name + "_face";
  // std::cout << "        face _face:" << std::endl;
  // _face->print();

  bcfg.dcel->setLoopKept(hel, true);
  hel->setFace(_face);
  _state = LifecycleState::ShellBuilt;
  validateStateInvariants("build");

  // for (auto f : _pmodel->dcel()->faces()) {
  //   f->outer()->print2();
  // }
}

// ===================================================================
//                                                       Class Filling
void Filling::setLayerType(LayerType *p_layertype) {
  p_flayertype = p_layertype;
}
