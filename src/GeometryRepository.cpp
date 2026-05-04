#include "GeometryRepository.hpp"

#include "PBaseLine.hpp"
#include "PDCELVertex.hpp"

void GeometryRepository::addVertex(PDCELVertex* v) {
  _vertices.push_back(v);
  _vertex_map[v->name()] = v;
}

void GeometryRepository::addBaseline(Baseline* l) {
  _baselines.push_back(l);
  _baseline_map[l->getName()] = l;
}

PDCELVertex* GeometryRepository::getPointByName(const std::string& name) const {
  auto it = _vertex_map.find(name);
  if (it != _vertex_map.end()) {
    return it->second;
  }
  return nullptr;
}

Baseline* GeometryRepository::getBaselineByName(const std::string& name) const {
  auto it = _baseline_map.find(name);
  if (it != _baseline_map.end()) {
    return it->second;
  }
  return nullptr;
}

Baseline* GeometryRepository::getBaselineByNameCopy(const std::string& name) const {
  auto it = _baseline_map.find(name);
  if (it != _baseline_map.end()) {
    return new Baseline(it->second);
  }
  return nullptr;
}
