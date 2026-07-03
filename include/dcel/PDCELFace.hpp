#pragma once

#include "dcel/PDCELHalfEdge.hpp"
#include "dcel/PDCELVertex.hpp"
#include "dcel/DCELConfig.hpp"

#include "geo_types.hpp"

#include <cmath>
#include <string>
#include <vector>

// Domain types below are used only as pointers in this header, so they are
// forward-declared rather than included. Material.hpp / PArea.hpp both pull in
// declarations.hpp (the whole-domain god-header); forward-declaring here keeps
// the DCEL face header off the domain include graph. Migrating these fields
// into PDCELFaceData is Phase 2 — for now the pointers still live on the face.
class Material;
class LayerType;
class PArea;

namespace dcel {

class PDCELHalfEdge;
class PDCELVertex;

/** @ingroup geo
 * A DCEL face class.
 *
 * Topology only: _outer, _inners, _is_bounded.
 * Domain data (name, material, mesh_size, etc.) live in PDCELFaceData,
 * owned by PModel and accessed via BuilderConfig::model->faceData(f).
 */
class PDCELFace {
private:
  PDCELHalfEdge *_outer;
  std::vector<PDCELHalfEdge *> _inners;
  std::string _log_name;
  PArea *_area;
  Material *_material = nullptr;
  double _theta3, _theta1;
  LayerType *_layertype;
  SVector3 _y1{1, 0, 0}, _y2{0, 1, 0};

  // True for bounded (real) faces; false for the unbounded background face.
  bool _is_bounded;
  unsigned int _id = 0;

public:
  PDCELFace();
  PDCELFace(PDCELHalfEdge *);
  PDCELFace(PDCELHalfEdge *, bool);

  void print(bool verbose = false);
  std::string label() const;
  std::string displayLabel() const;

  PArea *area() { return _area; }
  PDCELHalfEdge *outer() { return _outer; }
  std::vector<PDCELHalfEdge *> &inners() { return _inners; }
  Material *material() { return _material; }
  double theta3() { return _theta3; }
  double theta1() { return _theta1; }
  LayerType *layertype() { return _layertype; }
  bool isBlank() { return _material == nullptr ? true : false; }

  SVector3 localy1() { return _y1; }
  SVector3 localy2() { return _y2; }
  double theta1deg() { return atan2(_y2[2], _y2[1]) * 180.0 / kPi; }

  bool isBounded() { return _is_bounded; }
  unsigned int id() const { return _id; }
  const std::string &logName() const { return _log_name; }

  PDCELHalfEdge *getOuterHalfEdgeWithSource(PDCELVertex *);
  PDCELHalfEdge *getOuterHalfEdgeWithTarget(PDCELVertex *);
  PDCELHalfEdge *getInnerHalfEdgeWithSource(PDCELVertex *);
  PDCELHalfEdge *getInnerHalfEdgeWithTarget(PDCELVertex *);

  void setOuterComponent(PDCELHalfEdge *outer) { _outer = outer; }
  void addInnerComponent(PDCELHalfEdge *inner) { _inners.push_back(inner); }
  void setArea(PArea *area) { _area = area; }
  void setLogName(const std::string &name) { _log_name = name; }
  void setMaterial(Material *material) { _material = material; }
  void setTheta1(double theta1) { _theta1 = theta1; }
  void setTheta3(double theta3) { _theta3 = theta3; }

  void setLocaly1(SVector3 v) { _y1 = v; }
  void setLocaly2(SVector3 v) { _y2 = v; }
  double calcTheta1Fromy2(SVector3, bool deg = true);
  SVector3 calcy2FromTheta1(double, bool deg = true);

  void setBounded(bool b) { _is_bounded = b; }
  void setId(unsigned int id) { _id = id; }
  void setLayerType(LayerType *layertype) { _layertype = layertype; }
};

}  // namespace dcel
