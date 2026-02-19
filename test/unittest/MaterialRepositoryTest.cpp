#include "MaterialRepository.hpp"

#include "Material.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

void testMaterialRepositoryAddAndGet() {
  std::cout << "testMaterialRepositoryAddAndGet..." << std::endl;

  MaterialRepository repo;

  auto* m1 = new Material(1, "mat1", "isotropic", 1.5, std::vector<double>{1.0});
  auto* m2 = new Material(2, "mat2", "orthotropic", 2.0, std::vector<double>{1.0, 0.3});

  repo.addMaterial(m1);
  repo.addMaterial(m2);

  assert(repo.numMaterials() == 2);
  assert(repo.getMaterialByName("mat1") == m1);
  assert(repo.getMaterialByName("mat2") == m2);
  assert(repo.getMaterialByName("nonexistent") == nullptr);

  std::cout << "  PASSED" << std::endl;
}

void testMaterialRepositoryLayertype() {
  std::cout << "testMaterialRepositoryLayertype..." << std::endl;

  MaterialRepository repo;

  auto* m1 = new Material(1, "carbon", "isotropic", 1.5, std::vector<double>{1.0});
  repo.addMaterial(m1);

  repo.addLayerType(m1, 0.0);
  repo.addLayerType(m1, 45.0);
  repo.addLayerType(m1, 90.0);

  assert(repo.numLayerTypes() == 3);

  auto* lt0 = repo.getLayerTypeByMaterialAngle(m1, 0.0);
  auto* lt45 = repo.getLayerTypeByMaterialAngle(m1, 45.0);
  auto* lt90 = repo.getLayerTypeByMaterialAngle(m1, 90.0);

  assert(lt0 != nullptr);
  assert(lt45 != nullptr);
  assert(lt90 != nullptr);

  assert(std::abs(lt0->angle() - 0.0) < 1e-9);
  assert(std::abs(lt45->angle() - 45.0) < 1e-9);
  assert(std::abs(lt90->angle() - 90.0) < 1e-9);

  auto* lt_missing = repo.getLayerTypeByMaterialAngle(m1, 30.0);
  assert(lt_missing == nullptr);

  std::cout << "  PASSED" << std::endl;
}

void testMaterialRepositoryLayup() {
  std::cout << "testMaterialRepositoryLayup..." << std::endl;

  MaterialRepository repo;

  auto* layup1 = new Layup("layup1");
  auto* layup2 = new Layup("layup2");

  repo.addLayup(layup1);
  repo.addLayup(layup2);

  assert(repo.getLayupByName("layup1") == layup1);
  assert(repo.getLayupByName("layup2") == layup2);
  assert(repo.getLayupByName("nonexistent") == nullptr);

  std::cout << "  PASSED" << std::endl;
}

void testMaterialRepositoryLamina() {
  std::cout << "testMaterialRepositoryLamina..." << std::endl;

  MaterialRepository repo;

  auto* m1 = new Material(1, "mat1", "isotropic", 1.5, std::vector<double>{1.0});
  auto* lamina1 = new Lamina("lamina1", m1, 0.1);
  auto* lamina2 = new Lamina("lamina2", m1, 0.2);

  repo.addLamina(lamina1);
  repo.addLamina(lamina2);

  assert(repo.getLaminaByName("lamina1") == lamina1);
  assert(repo.getLaminaByName("lamina2") == lamina2);
  assert(repo.getLaminaByName("nonexistent") == nullptr);

  std::cout << "  PASSED" << std::endl;
}

int main() {
  std::cout << "MaterialRepository unit tests" << std::endl;
  std::cout << "==============================" << std::endl;

  testMaterialRepositoryAddAndGet();
  testMaterialRepositoryLayertype();
  testMaterialRepositoryLayup();
  testMaterialRepositoryLamina();

  std::cout << "All tests passed!" << std::endl;
  return 0;
}
