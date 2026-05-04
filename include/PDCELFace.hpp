#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PArea.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "globalConstants.hpp"

#include "geo_types.hpp"

#include <cmath>
#include <vector>

class PDCELHalfEdge;
class PDCELVertex;
class PArea;

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
  PArea *_area;
  Material *_material = nullptr;
  double _theta3, _theta1;
  LayerType *_layertype;
  SVector3 _y1{1, 0, 0}, _y2{0, 1, 0};

  // True for bounded (real) faces; false for the unbounded background face.
  bool _is_bounded;

public:
  PDCELFace();
  PDCELFace(PDCELHalfEdge *);
  PDCELFace(PDCELHalfEdge *, bool);

  void print();

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
  double theta1deg() { return atan2(_y2[2], _y2[1]) * 180.0 / PI; }

  bool isBounded() { return _is_bounded; }

  PDCELHalfEdge *getOuterHalfEdgeWithSource(PDCELVertex *);
  PDCELHalfEdge *getOuterHalfEdgeWithTarget(PDCELVertex *);
  PDCELHalfEdge *getInnerHalfEdgeWithSource(PDCELVertex *);
  PDCELHalfEdge *getInnerHalfEdgeWithTarget(PDCELVertex *);

  void setOuterComponent(PDCELHalfEdge *outer) { _outer = outer; }
  void addInnerComponent(PDCELHalfEdge *inner) { _inners.push_back(inner); }
  void setArea(PArea *area) { _area = area; }
  void setMaterial(Material *material) { _material = material; }
  void setTheta1(double theta1) { _theta1 = theta1; }
  void setTheta3(double theta3) { _theta3 = theta3; }

  void setLocaly1(SVector3 v) { _y1 = v; }
  void setLocaly2(SVector3 v) { _y2 = v; }
  double calcTheta1Fromy2(SVector3, bool deg = true);
  SVector3 calcy2FromTheta1(double, bool deg = true);

  void setBounded(bool b) { _is_bounded = b; }
  void setLayerType(LayerType *layertype) { _layertype = layertype; }
};
