#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PSegment.hpp"
#include "PBaseLine.hpp"
#include "utilities.hpp"
#include "globalVariables.hpp"

#include <list>
#include <string>
#include <vector>

class Baseline;
class Material;
class PDCELFace;
class PDCELVertex;
class PModel;
class Segment;

/** @ingroup cs
 * A cross-sectional component class.
 */
enum class ComponentType {
  reserved = 0,
  laminate = 1,
  fill = 2
};

enum class JointStyle {
  step = 1,
  smooth = 2
};

enum class FillSide {
  right = -1,
  left = 1
};

class PComponent {
private:
  struct LaminateState {
    PDCELVertex *ref_vertex = nullptr;
    std::vector<Segment *> segments;
    JointStyle style = JointStyle::step;
    bool cycle = false;
    std::vector<JointStyle> joint_styles;
    std::vector<std::vector<std::string>> joint_segments;
    std::string mat_orient_e1{"normal"};
    std::string mat_orient_e2{"baseline"};
  };

  struct FillingState {
    Material *material = nullptr;
    LayerType *layertype = nullptr;
    PDCELVertex *location = nullptr;
    std::list<std::list<Baseline *>> baseline_groups;
    FillSide side = FillSide::left;
    Baseline *ref_baseline = nullptr;
    PDCELFace *face = nullptr;
    double theta1 = 0.0;
    double theta3 = 0.0;
  };

  std::string _name;
  int _order; // The number deciding the building order
  ComponentType _type;
  std::list<PComponent *> _dependencies;
  double _mesh_size = -1;
  std::vector<PDCELVertex *> _embedded_vertices;

  LaminateState _laminate;
  FillingState _filling;

  // Helper functions
  // void findFreeEndVertices(Segment *seg, const int &end,
  //                          std::vector<PDCELVertex *> &vts);
  // void calcBoundingVertices(Segment *s, PDCELVertex *v);
  // void calcBoundingVertices(Segment *s1, PDCELVertex *v, Segment *s2);

  void createSegmentFreeEnd(Segment *s, int e, const BuilderConfig &);
  void joinSegments(Segment *s, int e, PDCELVertex *v, const BuilderConfig &);
  void joinSegments(
      Segment *s1, Segment *s2, int e1, int e2,
      PDCELVertex *v, JointStyle style, const BuilderConfig &);
  void buildLaminate(const BuilderConfig &);
  void buildFilling(const BuilderConfig &);

public:
  static int count_tmp;

  PComponent() : _order(0), _type(ComponentType::reserved) {};

  void print();
  void print(int, int = 0);

  std::string name() { return _name; }
  std::vector<Segment *> segments() { return _laminate.segments; }
  int order();
  ComponentType type() { return _type; }
  JointStyle style() { return _laminate.style; }
  bool isCyclic() { return _laminate.cycle; }
  std::list<PComponent *> &dependents() { return _dependencies; }
  double getMeshSize() const { return _mesh_size; }
  std::vector<PDCELVertex *> getEmbeddedVertices() { return _embedded_vertices; }

  PDCELVertex *refVertex() { return _laminate.ref_vertex; }

  std::string getMatOrient1() { return _laminate.mat_orient_e1; }
  std::string getMatOrient2() { return _laminate.mat_orient_e2; }

  Material *material() { return _filling.material; }
  LayerType *filllayertype() { return _filling.layertype; }
  PDCELVertex *location() { return _filling.location; }
  std::list<std::list<Baseline *>> baselines() { return _filling.baseline_groups; }
  FillSide fillside() { return _filling.side; }
  Baseline *refbaseline() { return _filling.ref_baseline; }
  PDCELFace *fillface() { return _filling.face; }
  double fillTheta1() { return _filling.theta1; }
  double fillTheta3() { return _filling.theta3; }

  void setName(std::string);
  void addSegment(Segment *);
  void setOrder(int);
  void setType(ComponentType t) { _type = t; }
  void setStyle(JointStyle style) { _laminate.style = style; }
  void setCycle(bool cycle) { _laminate.cycle = cycle; }
  void addDependent(PComponent *);
  void setMeshSize(double ms) { _mesh_size = ms; }
  void addEmbeddedVertex(PDCELVertex *v) { _embedded_vertices.push_back(v); }

  void setRefVertex(PDCELVertex *v) { _laminate.ref_vertex = v; }

  void setMatOrient1(std::string orient) { _laminate.mat_orient_e1 = orient; }
  void setMatOrient2(std::string orient) { _laminate.mat_orient_e2 = orient; }

  void setFillMaterial(Material *m) { _filling.material = m; }
  void setFillLayertype(LayerType *l) { _filling.layertype = l; }
  void setFillLocation(PDCELVertex *v) { _filling.location = v; }
  void addFillBaselineGroup(std::list<Baseline *> bg) {
    _filling.baseline_groups.push_back(bg);
  }
  void setFillSide(FillSide s) { _filling.side = s; }
  void setFillRefBaseline(Baseline *b) { _filling.ref_baseline = b; }
  void setFillFace(PDCELFace *f) { _filling.face = f; }
  void setFillTheta1(double a) { _filling.theta1 = a; }
  void setFillTheta3(double a) { _filling.theta3 = a; }

  void addJointStyle(JointStyle s) { _laminate.joint_styles.push_back(s); }
  void addJointSegments(std::vector<std::string> sns) { _laminate.joint_segments.push_back(sns); }

  void build(const BuilderConfig &);
  void buildDetails(const BuilderConfig &);
};

bool compareOrder(PComponent *, PComponent *);
