#include "PComponent.hpp"

#include "plog.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace {

enum class OrderVisitState {
  unvisited,
  visiting,
  visited
};

static std::string formatDependencyCycle(
    const std::vector<PComponent *> &stack, PComponent *node)
{
  std::ostringstream oss;
  bool found = false;
  for (auto component : stack) {
    if (component == node) {
      found = true;
    }
    if (found) {
      if (oss.tellp() > 0) {
        oss << " -> ";
      }
      oss << component->name();
    }
  }
  if (oss.tellp() > 0) {
    oss << " -> ";
  }
  oss << node->name();
  return oss.str();
}

static int resolveComponentOrder(
    PComponent *component,
    std::unordered_map<PComponent *, OrderVisitState> &states,
    std::vector<PComponent *> &stack,
    bool &has_cycle)
{
  const std::unordered_map<PComponent *, OrderVisitState>::const_iterator it =
      states.find(component);
  const OrderVisitState state =
      (it == states.end()) ? OrderVisitState::unvisited : it->second;

  if (state == OrderVisitState::visited) {
    return component->order();
  }

  if (state == OrderVisitState::visiting) {
    PLOG(error) << "order: circular dependency detected: "
                << formatDependencyCycle(stack, component);
    has_cycle = true;
    return 0;
  }

  states[component] = OrderVisitState::visiting;
  stack.push_back(component);

  int resolved_order = 1;
  for (auto dependency : component->dependents()) {
    resolved_order = std::max(
        resolved_order,
        resolveComponentOrder(dependency, states, stack, has_cycle) + 1);
  }

  stack.pop_back();
  states[component] = OrderVisitState::visited;
  if (!has_cycle) {
    component->setOrder(resolved_order);
  }
  return resolved_order;
}

} // namespace

int PComponent::count_tmp = 0;

void PComponent::print() {
  std::cout << "name: " << _name << " | "
            << "order: " << _order << " | "
            << "cyclic: " << (_laminate.cycle ? "true" : "false")
            << std::endl;
}

void PComponent::print(int /*i_type*/, int /*i_indent*/) {
    PLOG(debug) << "name: " + _name;
    PLOG(debug) << "order: " + std::to_string(_order);
    PLOG(debug) << "cyclic: " + std::to_string(_laminate.cycle);
    PLOG(debug) << "segments:";
  std::stringstream ss;
  ss << std::setw(4) << "no." << std::setw(16) << "name"
     << std::setw(16) << "base line"
     << std::setw(32) << "layup"
     << std::setw(16) << "side"
     << std::setw(8) << "level";
    PLOG(debug) << ss.str();
  for (int i = 0; i < _laminate.segments.size(); i++) {
    std::stringstream ss_seg;
    ss_seg << std::setw(4) << (i+1) << _laminate.segments[i];
        PLOG(debug) << ss_seg.str();
  }
  return;
}

int PComponent::order() {
  if (_order == 0) {
    std::unordered_map<PComponent *, OrderVisitState> states;
    std::vector<PComponent *> stack;
    bool has_cycle = false;
    const int resolved_order =
        resolveComponentOrder(this, states, stack, has_cycle);
    if (has_cycle) {
      return 0;
    }
    _order = resolved_order;
  }

  return _order;
}

void PComponent::setName(std::string name) { _name = name; }

void PComponent::addSegment(Segment *s) { _laminate.segments.push_back(s); }

void PComponent::setOrder(int order) { _order = order; }

void PComponent::addDependent(PComponent *component) {
  _dependencies.push_back(component);
}









void PComponent::build(const BuilderConfig &bcfg) {
  PLogContext component_context("component: " + _name);
  if (bcfg.dcel != nullptr) {
    bcfg.dcel->resetSplitLineageCounters();
  }

  // i_indent++;

    PLOG(info) << "building component: " + _name;

  // Laminate type component
  if (_type == ComponentType::laminate) {

    buildLaminate(bcfg);

  }


  // Fill type component
  else if (_type == ComponentType::fill) {

    buildFilling(bcfg);

  }

  std::size_t component_faces = 0;
  for (Segment *segment : _laminate.segments) {
    if (segment->face() != nullptr) {
      ++component_faces;
    }
  }
  if (_type == ComponentType::fill && _filling.face != nullptr) {
    component_faces = 1;
  }

  PLOG(info) << "built component " << _name
             << ": " << _laminate.segments.size() << " segments, "
             << component_faces << " faces, "
             << (bcfg.dcel->halfedges().size() / 2) << " dcel edges";

  // i_indent--;

}









void PComponent::buildDetails(const BuilderConfig &bcfg) {
  PLogContext component_details_context("component details: " + _name);
  if (bcfg.dcel != nullptr) {
    bcfg.dcel->resetSplitLineageCounters();
  }


  if (_type == ComponentType::fill) {
    PLOG(debug) << "buildDetails: skipping filling component '" << _name
                << "' because step 2 only builds laminate segment areas.";
    return;
  }

  if (_type != ComponentType::laminate) {
    PLOG(error) << "buildDetails: unsupported component type for '" << _name
                << "'";
    return;
  }

      PLOG(info) << "building component details: " + _name;

  for (auto sgm : _laminate.segments) {

    sgm->buildAreas(bcfg);

  }

  std::size_t total_areas = 0;
  std::size_t total_layers = 0;
  for (Segment *segment : _laminate.segments) {
    total_areas += segment->areaCount();
    total_layers += segment->layerCount();
  }

  PLOG(info) << "built details for " << _name
             << ": " << total_areas << " areas, "
             << total_layers << " layers";
}









// ===================================================================
//
// Non-Member Functions
//
// ===================================================================

bool compareOrder(PComponent *c1, PComponent *c2) {
  return c1->order() < c2->order();
}
