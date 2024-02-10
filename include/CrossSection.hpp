#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PModel.hpp"
#include "PSegment.hpp"
#include "utilities.hpp"
#include "globalConstants.hpp"
#include "PBaseLine.hpp"

#include "gmsh/SPoint3.h"
#include "gmsh/STensor3.h"

#include <list>
#include <string>
#include <vector>

class Segment;
// class Connection;
class Filling;
class PModel;
class PComponent;

// ===================================================================

/** @ingroup cs
 * A cross-section class.
 */
class CrossSection {
private:
  PModel *_pmodel;

  // // Analysis
  // int csflag_model; // structural model (0: classical, 1: timoshenko)
  // int csflag_thermal;
  // int csflag_curvature;
  // int csflag_oblique;
  // int csflag_trapeze;
  // int csflag_vlasov;
  // std::vector<double> cs_curvatures;
  // std::vector<double> cs_obliques;
  std::string csname;

  // Design
  // std::string cstype; // General or airfoil

  SPoint3 csorigin;
  std::vector<double> _translate;
  double _scale;
  double _rotate;
  STensor3 _rotatem;

  // double csmeshsize;
  // std::string cselementtype;

  std::vector<Material *> csusedmaterials;
  std::vector<LayerType *> csusedlayertypes;

  std::vector<Segment> cssegments;
  // std::vector<Connection> csconnections;
  std::vector<Filling> csfillings;

  std::list<PComponent *> _components;

  // Others

public:
  static int used_material_index;
  static int used_layertype_index;

  CrossSection(std::string); // assign name, others default
  CrossSection(std::string, PModel *);
  // CrossSection(std::string name, std::string type, SPoint2 origin,
  //              std::vector<double> translate, double scale, double rotate,
  //              STensor3 rotatem, double meshsize, std::string elementtype,
  //              std::vector<Segment> segments,
  //              std::vector<Connection> connections,
  //              std::vector<Filling> fillings)
  //     : csname(name), cstype(type), csorigin(origin), _translate(translate),
  //       _scale(scale), _rotate(rotate), _rotatem(rotatem),
  //       csmeshsize(meshsize), cselementtype(elementtype),
  //       cssegments(segments), csconnections(connections),
  //       csfillings(fillings) {}

  void printCrossSection();

  PModel *pmodel() { return _pmodel; }

  std::string getName() { return csname; }

  SPoint3 getOrigin() { return csorigin; }
  std::vector<double> translate() { return _translate; }
  double scale() { return _scale; }
  double rotate() { return _rotate; }
  STensor3 getRotateMatrix() { return _rotatem; }

  // double getMeshsize() { return csmeshsize; }
  // std::string getElementtype() { return cselementtype; }
  // std::vector<Material *>::size_type getNumOfUsedMaterials() const { return csusedmaterials.size(); }
  unsigned int getNumOfUsedMaterials() const { return csusedmaterials.size(); }
  // std::vector<LayerType *>::size_type getNumOfUsedLayerTypes() const { return csusedlayertypes.size(); }
  unsigned int getNumOfUsedLayerTypes() const { return csusedlayertypes.size(); }
  std::vector<Material *> getUsedMaterials() { return csusedmaterials; }
  std::vector<LayerType *> getUsedLayerTypes() { return csusedlayertypes; }
  LayerType *getUsedLayerTypeByMaterialAngle(Material *, double);
  LayerType *getUsedLayerTypeByMaterialNameAngle(std::string, double);
  std::vector<Segment> getSegments() { return cssegments; }
  // std::vector<Connection> getConnections() { return csconnections; }
  // std::vector<Filling> getFillings() { return csfillings; }
  std::list<PComponent *> components() { return _components; }

  void setPModel(PModel *pmodel) { _pmodel = pmodel; }

  void setOrigin(SPoint3);
  void setTranslate(std::vector<double>);
  void setScale(double);
  void setRotate(double);
  void setRotateMatrix(STensor3);
  // void setMeshsize(double);
  // void setElementtype(std::string);

  void addUsedMaterial(Material *);
  void addUsedLayerType(LayerType *);

  void addSegment(Segment);
  void addComponent(PComponent *);

  void sortComponents();

  void build(Message *);
};
