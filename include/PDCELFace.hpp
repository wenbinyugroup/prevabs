#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PArea.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "globalConstants.hpp"

// #include "gmsh/GFace.h"
#include "gmsh_mod/SVector3.h"

#include <cmath>
#include <string>
#include <vector>

class PDCELHalfEdge;
class PDCELVertex;
class PArea;

/** @ingroup geo
 * A DCEL face class.
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
  double _mesh_size = -1;
  std::vector<PDCELVertex *> _embedded_vertices;

  // Gmsh
  bool _gbuild;
  // GFace *_gface;
  int _gface_tag = 0;
  std::vector<int> _embedded_gvertex_tags;
  std::vector<int> _embedded_gedge_tags;
  int _gmsh_physical_group_tag = 0;

  std::string _name;

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

  double getMeshSize() const { return _mesh_size; }
  std::vector<PDCELVertex *> getEmbeddedVertices() { return _embedded_vertices; }

  bool gbuild() { return _gbuild; }
  // GFace *gface() { return _gface; }
  int gfaceTag() { return _gface_tag; }
  std::vector<int> getEmbeddedGVertexTags() { return _embedded_gvertex_tags; }
  std::vector<int> getEmbeddedGEdgeTags() { return _embedded_gedge_tags; }
  void addEmbeddedGVertexTag(int tag) { _embedded_gvertex_tags.push_back(tag); }
  void addEmbeddedGEdgeTag(int tag) { _embedded_gedge_tags.push_back(tag); }
  int gmshPhysicalGroupTag() { return _gmsh_physical_group_tag; }

  std::string name() { return _name; }

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

  void setMeshSize(double ms) { _mesh_size = ms; }
  void addEmbeddedVertex(PDCELVertex *v) { _embedded_vertices.push_back(v); }

  void setGBuild(bool b) { _gbuild = b;}
  // void setGFace(GFace *gf) { _gface = gf; }
  void setGFaceTag(int tag) { _gface_tag = tag; }
  void setGmshPhysicalGroupTag(int tag) { _gmsh_physical_group_tag = tag; }
  void setLayerType(LayerType *layertype) { _layertype = layertype; }

  void setName(std::string name) { _name = name; }
};
