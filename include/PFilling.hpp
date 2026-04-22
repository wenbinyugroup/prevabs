#pragma once

class Baseline;
class LayerType;
class Material;

#include <string>

class Filling {
private:
  std::string _name;
  Baseline *_baseline{nullptr};
  Material *_material{nullptr};
  LayerType *_layertype{nullptr};
  std::string _fillside;

public:
  Filling() {}
  Filling(std::string name, Baseline *p_baseline, Material *p_material,
          std::string fillside)
      : _name(name), _baseline(p_baseline), _material(p_material),
        _fillside(fillside) {}
  Filling(std::string name, Baseline *p_baseline, Material *p_material,
          std::string fillside, LayerType *p_layertype)
      : _name(name), _baseline(p_baseline), _material(p_material),
        _layertype(p_layertype), _fillside(fillside) {}

  std::string getName() { return _name; }
  Baseline *getBaseline() { return _baseline; }
  Material *getMaterial() { return _material; }
  LayerType *getLayerType() { return _layertype; }
  std::string getFillSide() { return _fillside; }

  void setLayerType(LayerType *);
};
