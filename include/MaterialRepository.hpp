#pragma once

// Forward declarations — full definitions are in Material.hpp,
// included in MaterialRepository.cpp.
class Lamina;
class LayerType;
class Layup;
class Material;

#include "globalVariables.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * Owns all material-related objects (Material, Lamina, LayerType, Layup)
 * and provides O(1)/O(log n) lookups by name or material+angle key.
 *
 * Extracted from PModel as part of the Step 6 God-object split.
 */
class MaterialRepository : public IMaterialLookup {
public:
  // -----------------------------------------------------------------
  // Adders

  void addMaterial(Material* m);
  void addLamina(Lamina* l);

  /// Add an already-constructed LayerType.
  void addLayerType(LayerType* lt);
  /// Create and add a LayerType for the given material pointer and angle.
  void addLayerType(Material* m, double angle);
  /// Create and add a LayerType looked up by material name and angle.
  void addLayerType(const std::string& material_name, double angle);

  void addLayup(Layup* l);

  // -----------------------------------------------------------------
  // Getters

  Material*  getMaterialByName(const std::string& name) const;
  Lamina*    getLaminaByName(const std::string& name) const;

  /// Implements IMaterialLookup — O(log n) via map.
  LayerType* getLayerTypeByMaterialAngle(Material* m, double angle) override;
  LayerType* getLayerTypeByMaterialNameAngle(const std::string& name, double angle);

  Layup*     getLayupByName(const std::string& name) const;

  // -----------------------------------------------------------------
  // Ordered access for IO/write paths (preserves insertion order)

  std::vector<Material*>&  materials()  { return _materials; }
  std::vector<Lamina*>&    laminas()    { return _laminas; }
  std::vector<LayerType*>& layertypes() { return _layertypes; }
  std::vector<Layup*>&     layups()     { return _layups; }

  const std::vector<Material*>&  materials()  const { return _materials; }
  const std::vector<LayerType*>& layertypes() const { return _layertypes; }

  std::size_t numMaterials()  const { return _materials.size(); }
  std::size_t numLayerTypes() const { return _layertypes.size(); }

private:
  // Ordered vectors (for iteration in write path)
  std::vector<Material*>  _materials;
  std::vector<Lamina*>    _laminas;
  std::vector<LayerType*> _layertypes;
  std::vector<Layup*>     _layups;

  // Fast lookup maps
  std::unordered_map<std::string, Material*> _material_map;
  std::unordered_map<std::string, Lamina*>   _lamina_map;
  std::unordered_map<std::string, Layup*>    _layup_map;

  // LayerType keyed by (material_ptr, angle) — O(log n)
  std::map<std::pair<Material*, double>, LayerType*> _layertype_map;
};
