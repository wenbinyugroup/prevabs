#include "PSegment.hpp"

#include "Material.hpp"
#include "PArea.hpp"
#include "PBaseLine.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "plog.hpp"
#include "utilities.hpp"

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




std::string faceLabel(PDCELFace *face, PModel *model) {
  if (face == nullptr) {
    return "nullptr";
  }

  std::ostringstream oss;
  oss << static_cast<void *>(face);
  std::string label = "face@" + oss.str();
  if (model != nullptr) {
    const std::string &name = model->faceData(face).name;
    if (!name.empty()) {
      label += " [" + name + "]";
    }
  }
  return label;
}




void logVertexEdgeRing(
    PDCELVertex *vertex, PModel *model, const std::string &prefix) {
  if (vertex == nullptr) {
    PLOG(error) << prefix << ": vertex=nullptr";
    return;
  }

  PLOG(error) << prefix << ": vertex=" << vertex->printString();
  if (vertex->edge() == nullptr) {
    PLOG(error) << prefix << ": edge ring is empty";
    return;
  }

  PDCELHalfEdge *start = vertex->edge();
  PDCELHalfEdge *he = start;
  int iter = 0;
  do {
    if (he == nullptr) {
      PLOG(error) << prefix << ": encountered nullptr half-edge in ring";
      return;
    }
    if (++iter > kDCELLoopHardCap) {
      PLOG(error) << prefix
                  << ": edge ring walk exceeded "
                  << kDCELLoopHardCap << " steps";
      return;
    }

    std::ostringstream line;
    line << prefix
         << ": outgoing[" << (iter - 1) << "] "
         << he->source()->printString()
         << " -> " << he->target()->printString()
         << " | face=" << faceLabel(he->face(), model)
         << " | loop=" << (he->loop() ? "set" : "nullptr");
    PLOG(error) << line.str();

    if (he->twin() == nullptr) {
      PLOG(error) << prefix << ": outgoing[" << (iter - 1)
                  << "] has nullptr twin";
      return;
    }
    he = he->twin()->next();
  } while (he != nullptr && he != start);

  if (he == nullptr) {
    PLOG(error) << prefix << ": edge ring terminated at nullptr next";
  }
}




void logMissingHalfEdgeBetween(
    const std::string &caller, const std::string &segment_name,
    PDCELVertex *source, PDCELVertex *target, const BuilderConfig &bcfg) {
  PLOG(error) << "Segment::" << caller
              << ": findHalfEdgeBetween returned nullptr"
              << " for segment '" << segment_name << "'"
              << ", source="
              << (source ? source->printString() : "nullptr")
              << ", target="
              << (target ? target->printString() : "nullptr");
  logVertexEdgeRing(
      source, bcfg.model,
      "Segment::" + caller + ": source edge ring for segment '"
          + segment_name + "'");
}

std::size_t countLoopSteps(PDCELHalfEdge *start) {
  if (start == nullptr) {
    return 0;
  }

  std::size_t count = 0;
  PDCELHalfEdge *he = start;
  do {
    if (he == nullptr) {
      throw std::runtime_error("countLoopSteps: encountered nullptr half-edge");
    }
    if (count >= static_cast<std::size_t>(kDCELLoopHardCap)) {
      throw std::runtime_error(
          "countLoopSteps: DCEL loop exceeded "
          + std::to_string(kDCELLoopHardCap) + " iterations");
    }
    ++count;
    he = he->next();
  } while (he != start);

  return count;
}

} // namespace

std::ostream &operator<<(std::ostream &out, Segment *s) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << s->_name << std::setw(16) << s->_curve_base->getName()
      << std::setw(32) << s->_layup->getName() << std::setw(16) << s->_layupside
      << std::setw(8) << s->_level;
  return out;
}

Segment::~Segment() {
  releaseOwnedResources();
}

Segment::Segment(Segment &&other) noexcept
    : _name(std::move(other._name)),
      _curve_base(other._curve_base),
      _curve_offset(std::move(other._curve_offset)),
      _layup(other._layup),
      _areas(std::move(other._areas)),
      _layupside(std::move(other._layupside)),
      _level(other._level),
      _prev(other._prev),
      _next(other._next),
      _prev_bound(other._prev_bound),
      _next_bound(other._next_bound),
      _mat_orient_e1(std::move(other._mat_orient_e1)),
      _mat_orient_e2(std::move(other._mat_orient_e2)),
      _prev_bound_vertices(std::move(other._prev_bound_vertices)),
      _next_bound_vertices(std::move(other._next_bound_vertices)),
      _prev_bound_indices(std::move(other._prev_bound_indices)),
      _next_bound_indices(std::move(other._next_bound_indices)),
      _face(other._face),
      _free(other._free),
      _head_vertex_offset(other._head_vertex_offset),
      _tail_vertex_offset(other._tail_vertex_offset),
      _base_offset_indices_pairs(std::move(other._base_offset_indices_pairs)),
      _state(other._state) {
  other._curve_base = nullptr;
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
  _curve_offset = std::move(other._curve_offset);
  _layup = other._layup;
  _areas = std::move(other._areas);
  _layupside = std::move(other._layupside);
  _level = other._level;
  _prev = other._prev;
  _next = other._next;
  _prev_bound = other._prev_bound;
  _next_bound = other._next_bound;
  _mat_orient_e1 = std::move(other._mat_orient_e1);
  _mat_orient_e2 = std::move(other._mat_orient_e2);
  _prev_bound_vertices = std::move(other._prev_bound_vertices);
  _next_bound_vertices = std::move(other._next_bound_vertices);
  _prev_bound_indices = std::move(other._prev_bound_indices);
  _next_bound_indices = std::move(other._next_bound_indices);
  _face = other._face;
  _free = other._free;
  _head_vertex_offset = other._head_vertex_offset;
  _tail_vertex_offset = other._tail_vertex_offset;
  _base_offset_indices_pairs = std::move(other._base_offset_indices_pairs);
  _state = other._state;

  other._curve_base = nullptr;
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
  _areas.clear();

  if (_curve_offset != nullptr) {
    deleteUnregisteredVertices(_curve_offset->vertices());
    _curve_offset.reset();
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

int Segment::requireValidLayupSide(const char *caller) const {
  if (_layupside == "left") {
    return 1;
  }
  if (_layupside == "right") {
    return -1;
  }

  const std::string message =
      std::string("Segment::") + caller
      + " requires layup side 'left' or 'right' but got '"
      + _layupside + "' for segment '" + _name + "'";
  PLOG(error) << message;
  assert(false && "Invalid segment layup side");
  return 0;
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
  if (!requireOffsetCurve("printBaseOffsetPairs")) {
    return;
  }

    PLOG(debug) << "base vertices -- base_link_to_offset_indices";

    PLOG(debug) << "number of pairs: " + std::to_string(_base_offset_indices_pairs.size());

  for (auto i = 0; i < _base_offset_indices_pairs.size(); i++) {
    const BaseOffsetPair &pair = _base_offset_indices_pairs[i];
    std::string s = std::to_string(pair.base) + ": "
      + _curve_base->vertices()[pair.base]->printString()
      + " -- " + std::to_string(pair.offset) + ": "
      + _curve_offset->vertices()[pair.offset]->printString();

        PLOG(debug) << s;
  }

}

int Segment::layupSide() {
  // Sign convention used throughout offset/build/buildAreas:
  // +1 = offset/build on the left of the directed base curve, -1 = right.
  return requireValidLayupSide("layupSide");
}

bool Segment::closed() const {
  return _curve_base != nullptr && _curve_base->isClosed();
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

void Segment::addArea(PArea *area) { _areas.emplace_back(area); }

void Segment::setCurveOffset(Baseline *c) { _curve_offset.reset(c); }

void Segment::setPrevBoundVertices(std::vector<PDCELVertex *> vertices) {
  _prev_bound_vertices = vertices;
}

void Segment::setNextBoundVertices(std::vector<PDCELVertex *> vertices) {
  _next_bound_vertices = vertices;
}

std::size_t Segment::layerCount() const {
  std::size_t count = 0;
  for (const auto &area : _areas) {
    count += area->faces().size();
  }
  return count;
}

void Segment::offsetCurveBase() {
  if (!requireBaseDefinition("offsetCurveBase")) {
    return;
  }

  if (_state == LifecycleState::OffsetReady) {
    if (!validateStateInvariants("offsetCurveBase")) {
      return;
    }
    PLOG(debug) << "offsetCurveBase: reusing existing offset curve for segment '"
                << _name << "'";
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

  PLOG(debug) << "offsetting the base curve of segment: " + _name;

  if (_curve_offset != nullptr) {
    PLOG(error) << "Segment::offsetCurveBase found stale offset state for segment '"
                << _name << "' while lifecycle state is BaseReady";
    deleteUnregisteredVertices(_curve_offset->vertices());
    _curve_offset.reset();
  }
  _base_offset_indices_pairs.clear();

  const int side = requireValidLayupSide("offsetCurveBase");
  if (side == 0) {
    return;
  }

  _curve_offset.reset(new Baseline());
  offset(_curve_base->vertices(), side, _layup->getTotalThickness(),
         _curve_offset->vertices(), _base_offset_indices_pairs);
  _face = nullptr;
  _areas.clear();
  _head_vertex_offset = nullptr;
  _tail_vertex_offset = nullptr;
  _state = LifecycleState::OffsetReady;
  validateStateInvariants("offsetCurveBase");

  PLOG(debug) << "base line: "
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  PLOG(debug) << "offset line: "
    << _curve_offset->vertices().front()->printString() << " -> "
    << _curve_offset->vertices().back()->printString();
}

void Segment::build(const BuilderConfig &bcfg) {
  PLogContext segment_context("segment shell: " + _name);
  if (!requireExactState(LifecycleState::OffsetReady, "build")) {
    return;
  }
  if (!validateStateInvariants("build")) {
    return;
  }

  if (bcfg.debug) {
    PLOG(debug) << "building the overall shape of segment: " + _name;
    PLOG(debug) << "base line: "
    << _curve_base->vertices().front()->printString() << " -> "
    << _curve_base->vertices().back()->printString();
  }

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

    PLOG(debug) << "creating half edges for the offset curve";
  for (int i = 0; i < _curve_offset->vertices().size() - 1; ++i) {
    bcfg.dcel->addEdge(_curve_offset->vertices()[i],
                             _curve_offset->vertices()[i + 1]);
  }

  // Create half edge loop and face
    PLOG(debug) << "creating the half edge loop and face";
  PDCELHalfEdgeLoop *hel;
  he = bcfg.dcel->findHalfEdgeBetween(_curve_base->vertices()[0],
                                          _curve_base->vertices()[1]);
  if (he == nullptr) {
    logMissingHalfEdgeBetween(
        "build", _name, _curve_base->vertices()[0],
        _curve_base->vertices()[1], bcfg);
    return;
  }
  if (requireValidLayupSide("build") < 0) {
    he = he->twin();
  }
  hel = bcfg.dcel->addHalfEdgeLoop(he);

  _face = bcfg.dcel->addFace(hel);
  bcfg.model->faceData(_face).name = _name + "_face";

  bcfg.dcel->setLoopKept(hel, true);
  hel->setFace(_face);
  _state = LifecycleState::ShellBuilt;
  validateStateInvariants("build");

  const std::size_t loop_steps = countLoopSteps(hel->incidentEdge());
  PLOG(info) << "built segment " << _name
             << ": " << _curve_base->vertices().size() << " base verts, "
             << _curve_offset->vertices().size() << " offset verts, "
             << "loop walked " << loop_steps << " steps";
}
