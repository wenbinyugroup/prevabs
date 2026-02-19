#include "GeometryRepository.hpp"

#include "PBaseLine.hpp"
#include "PDCELVertex.hpp"

#include <cassert>
#include <iostream>
#include <string>

void testGeometryRepositoryAddVertex() {
  std::cout << "testGeometryRepositoryAddVertex..." << std::endl;

  GeometryRepository repo;

  auto* v1 = new PDCELVertex("point1", 0.0, 0.0, 0.0);
  auto* v2 = new PDCELVertex("point2", 1.0, 0.0, 0.0);
  auto* v3 = new PDCELVertex("point3", 1.0, 1.0, 0.0);

  repo.addVertex(v1);
  repo.addVertex(v2);
  repo.addVertex(v3);

  auto& vertices = repo.vertices();
  assert(vertices.size() == 3);

  assert(repo.getPointByName("point1") == v1);
  assert(repo.getPointByName("point2") == v2);
  assert(repo.getPointByName("point3") == v3);
  assert(repo.getPointByName("nonexistent") == nullptr);

  std::cout << "  PASSED" << std::endl;
}

void testGeometryRepositoryAddBaseline() {
  std::cout << "testGeometryRepositoryAddBaseline..." << std::endl;

  GeometryRepository repo;

  auto* bl1 = new Baseline("baseline1", "polyline");
  auto* bl2 = new Baseline("baseline2", "circle");

  repo.addBaseline(bl1);
  repo.addBaseline(bl2);

  auto& baselines = repo.baselines();
  assert(baselines.size() == 2);

  assert(repo.getBaselineByName("baseline1") == bl1);
  assert(repo.getBaselineByName("baseline2") == bl2);
  assert(repo.getBaselineByName("nonexistent") == nullptr);

  std::cout << "  PASSED" << std::endl;
}

void testGeometryRepositoryBaselineCopy() {
  std::cout << "testGeometryRepositoryBaselineCopy..." << std::endl;

  GeometryRepository repo;

  auto* bl1 = new Baseline("baseline1", "polyline");
  repo.addBaseline(bl1);

  auto* bl1_copy = repo.getBaselineByNameCopy("baseline1");
  assert(bl1_copy != nullptr);
  assert(bl1_copy->getName() == "baseline1");

  auto* bl_nonexistent = repo.getBaselineByNameCopy("nonexistent");
  assert(bl_nonexistent == nullptr);

  std::cout << "  PASSED" << std::endl;
}

int main() {
  std::cout << "GeometryRepository unit tests" << std::endl;
  std::cout << "==============================" << std::endl;

  testGeometryRepositoryAddVertex();
  testGeometryRepositoryAddBaseline();
  testGeometryRepositoryBaselineCopy();

  std::cout << "All tests passed!" << std::endl;
  return 0;
}
