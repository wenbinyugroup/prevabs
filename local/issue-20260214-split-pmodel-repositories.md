# Plan: Section 14 Step 6 — Split PModel into Sub-Repositories

## Context

`PModel` is a God object (60+ public methods) responsible for data storage, geometry, mesh, IO,
and post-processing. Step 6 of the recommended refactoring sequence extracts four distinct
data-owning sub-objects from `PModel`, improving cohesion and enabling O(1) lookups.
`PModel` delegates to these sub-objects and becomes a thin orchestrating shim.

This plan does NOT break any existing call sites — all public methods on `PModel` remain and
delegate to the appropriate sub-object.

---

## New Classes to Create

### 1. `MaterialRepository` — `include/MaterialRepository.hpp`

Owns Material, Lamina, LayerType, Layup with O(1) / O(log n) lookups.

```cpp
#pragma once
#include "Material.hpp"
#include "globalVariables.hpp"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class MaterialRepository : public IMaterialLookup {
public:
  // Adders
  void addMaterial(Material* m);
  void addLamina(Lamina* l);
  void addLayerType(LayerType* lt);
  void addLayerType(Material* m, double angle);
  void addLayerType(const std::string& name, double angle);
  void addLayup(Layup* l);

  // Getters
  Material*   getMaterialByName(const std::string& name) const;
  Lamina*     getLaminaByName(const std::string& name) const;
  LayerType*  getLayerTypeByMaterialAngle(Material* m, double angle) override;
  LayerType*  getLayerTypeByMaterialNameAngle(const std::string& name, double angle);
  Layup*      getLayupByName(const std::string& name) const;

  // Ordered access (for IO/write paths)
  std::vector<Material*>&  materials()   { return _materials; }
  std::vector<Lamina*>&    laminas()     { return _laminas; }
  std::vector<LayerType*>& layertypes()  { return _layertypes; }
  std::vector<Layup*>&     layups()      { return _layups; }

  std::size_t numMaterials()   const { return _materials.size(); }
  std::size_t numLayerTypes()  const { return _layertypes.size(); }

private:
  std::vector<Material*>  _materials;
  std::vector<Lamina*>    _laminas;
  std::vector<LayerType*> _layertypes;
  std::vector<Layup*>     _layups;

  std::unordered_map<std::string, Material*>  _material_map;
  std::unordered_map<std::string, Lamina*>    _lamina_map;
  std::unordered_map<std::string, Layup*>     _layup_map;
  // LayerType keyed by (material_ptr, angle) — log(n) lookup
  std::map<std::pair<Material*, double>, LayerType*> _layertype_map;
};
```

### 2. `GeometryRepository` — `include/GeometryRepository.hpp`

Owns named PDCELVertex points and Baselines with O(1) lookups.

```cpp
#pragma once
// Forward declarations
class Baseline;
class PDCELVertex;
#include <string>
#include <unordered_map>
#include <vector>

class GeometryRepository {
public:
  void addVertex(PDCELVertex* v);
  void addBaseline(Baseline* l);

  PDCELVertex* getPointByName(const std::string& name) const;
  Baseline*    getBaselineByName(const std::string& name) const;
  Baseline*    getBaselineByNameCopy(const std::string& name) const;

  std::vector<PDCELVertex*>& vertices()  { return _vertices; }
  std::vector<Baseline*>&    baselines() { return _baselines; }

private:
  std::vector<PDCELVertex*> _vertices;
  std::vector<Baseline*>    _baselines;
  std::unordered_map<std::string, PDCELVertex*> _vertex_map;
  std::unordered_map<std::string, Baseline*>    _baseline_map;
};
```

### 3. `MeshData` — `include/MeshData.hpp`

Simple struct owning post-Gmsh node/element data (already partially present as scattered
members on PModel).

```cpp
#pragma once
// Forward declarations
class PMesh;
#include <cstddef>
#include <vector>

struct MeshData {
  PMesh* mesh = nullptr;
  std::vector<std::vector<int>> node_elements;
  std::size_t num_nodes    = 0;
  std::size_t num_elements = 0;
};
```

### 4. `PostProcessingData` — `include/PostProcessingData.hpp`

Groups LoadCases and LocalState results.

```cpp
#pragma once
// LocalState and LoadCase are defined in PModel.hpp currently.
// Forward-declare here; move definitions to PostProcessingData.hpp as a follow-up.
class LocalState;
struct LoadCase;
#include <vector>

struct PostProcessingData {
  std::vector<LoadCase>      load_cases;
  std::vector<LocalState*>   local_states;
};
```

---

## Changes to `PModel`

### `include/PModel.hpp`

1. Add member objects:
   ```cpp
   MaterialRepository  _mat_repo;
   GeometryRepository  _geo_repo;
   MeshData            _mesh_data;
   PostProcessingData  _pp_data;
   ```

2. Remove the raw member vectors they replace:
   - `_materials`, `_laminas`, `_layertypes`, `_layups`
   - `_vertices`, `_baselines`
   - `_pmesh`, `_node_elements`, `_num_nodes`, `_num_elements`
   - `_local_states`, `_load_cases`

3. Add accessors to expose sub-objects:
   ```cpp
   MaterialRepository& materialRepository() { return _mat_repo; }
   GeometryRepository& geometryRepository() { return _geo_repo; }
   MeshData&           meshData()           { return _mesh_data; }
   ```

4. Update all existing PModel methods to delegate, e.g.:
   ```cpp
   Material* getMaterialByName(std::string n) { return _mat_repo.getMaterialByName(n); }
   void addMaterial(Material* m)              { _mat_repo.addMaterial(m); }
   // ... etc.
   ```

5. `PModel` still inherits `IMaterialLookup` and delegates:
   ```cpp
   LayerType* getLayerTypeByMaterialAngle(Material* m, double a) override {
     return _mat_repo.getLayerTypeByMaterialAngle(m, a);
   }
   ```

6. `BuilderConfig.materials` in `globalVariables.hpp` can point to `&_mat_repo` instead of
   the `PModel*` — a later cleanup; for now pointing `&pmodel` still works since PModel
   still implements `IMaterialLookup`.

---

## Implementation Files to Create/Modify

| Action | File |
|--------|------|
| **Create** | `include/MaterialRepository.hpp` |
| **Create** | `include/GeometryRepository.hpp` |
| **Create** | `include/MeshData.hpp` |
| **Create** | `include/PostProcessingData.hpp` |
| **Create** | `src/MaterialRepository.cpp` (add/get implementations) |
| **Create** | `src/GeometryRepository.cpp` (add/get implementations) |
| **Modify** | `include/PModel.hpp` (replace raw members with sub-objects; update method bodies to delegate) |
| **Modify** | `src/cs/PModel.cpp` (update implementations that directly accessed the removed vectors) |
| **Modify** | `CMakeLists.txt` (add `src/MaterialRepository.cpp` and `src/GeometryRepository.cpp`) |

No IO files (`PModelIORead*.cpp`) need to change — they call PModel methods which now delegate.

---

## Verification

1. Build with `tools\build_msvc.bat fast` — must compile clean.
2. Run integration test: `prevabs.exe -i test\box\input.xml -h` — must produce same output.
3. Confirm no functional change: `writeSG` still produces the same SG file.

---

## Notes

- `LocalState` and `LoadCase` definitions remain in `PModel.hpp` for now; moving them to
  `PostProcessingData.hpp` is a follow-up (requires updating every include).
- `getBaselineByNameCopy` creates a new heap-allocated copy — this logic moves to
  `GeometryRepository` unchanged.
- `addLayerType(std::string name, double angle)` calls `getMaterialByName` internally;
  this becomes `_mat_repo.getMaterialByName()` inside `MaterialRepository::addLayerType`.
