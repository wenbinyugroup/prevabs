#include "MaterialRepository.hpp"

#include "Material.hpp"

void MaterialRepository::addMaterial(Material* m) {
  _materials.push_back(m);
  _material_map[m->getName()] = m;
}

void MaterialRepository::addLamina(Lamina* l) {
  _laminas.push_back(l);
  _lamina_map[l->getName()] = l;
}

void MaterialRepository::addLayerType(LayerType* lt) {
  _layertypes.push_back(lt);
  _layertype_map[{lt->material(), lt->angle()}] = lt;
}

void MaterialRepository::addLayerType(Material* m, double angle) {
  LayerType* lt = new LayerType(0, m, angle);
  addLayerType(lt);
}

void MaterialRepository::addLayerType(const std::string& material_name, double angle) {
  Material* m = getMaterialByName(material_name);
  LayerType* lt = new LayerType(0, m, angle);
  addLayerType(lt);
}

void MaterialRepository::addLayup(Layup* l) {
  _layups.push_back(l);
  _layup_map[l->getName()] = l;
}

Material* MaterialRepository::getMaterialByName(const std::string& name) const {
  auto it = _material_map.find(name);
  if (it != _material_map.end()) {
    return it->second;
  }
  return nullptr;
}

Lamina* MaterialRepository::getLaminaByName(const std::string& name) const {
  auto it = _lamina_map.find(name);
  if (it != _lamina_map.end()) {
    return it->second;
  }
  return nullptr;
}

LayerType* MaterialRepository::getLayerTypeByMaterialAngle(Material* m, double angle) {
  auto it = _layertype_map.find({m, angle});
  if (it != _layertype_map.end()) {
    return it->second;
  }
  return nullptr;
}

LayerType* MaterialRepository::getLayerTypeByMaterialNameAngle(const std::string& name, double angle) {
  Material* m = getMaterialByName(name);
  if (m == nullptr) {
    return nullptr;
  }
  return getLayerTypeByMaterialAngle(m, angle);
}

Layup* MaterialRepository::getLayupByName(const std::string& name) const {
  auto it = _layup_map.find(name);
  if (it != _layup_map.end()) {
    return it->second;
  }
  return nullptr;
}
