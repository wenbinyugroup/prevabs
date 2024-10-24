#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PDCEL.hpp"
#include "PSegment.hpp"
#include "utilities.hpp"

// #include "gmsh/GModel.h"

#include <string>
#include <vector>
// #include <map>
#include <utility>

// class PDCEL;
// class Basepoint;
// class Baseline;
// class CrossSection;
// class PDCELVertex;

struct LoadCase {
  int measure{0};  // 0: generalized stress, 1: generalized strain
  std::vector<double> displacement{0, 0, 0};
  std::vector<std::vector<double>> rotation{
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1}
  };
  std::vector<double> force{0, 0, 0};
  std::vector<double> moment{0, 0, 0};
  std::vector<double> load{};
  std::vector<double> distr_force{0, 0, 0};
  std::vector<double> distr_force_d1{0, 0, 0};
  std::vector<double> distr_force_d2{0, 0, 0};
  std::vector<double> distr_force_d3{0, 0, 0};
  std::vector<double> distr_moment{0, 0, 0};
  std::vector<double> distr_moment_d1{0, 0, 0};
  std::vector<double> distr_moment_d2{0, 0, 0};
  std::vector<double> distr_moment_d3{0, 0, 0};

  std::string envelope_axis1, envelope_axis2;
  int envelope_div;
};




class LocalState {
private:
  int _loadcase_id;
  std::string _loadcase_name = "";

  // Nodal data
  PElementNodeData *_u = nullptr;

  // Element data
  PElementNodeData *_s = nullptr;
  PElementNodeData *_e = nullptr;
  PElementNodeData *_sm = nullptr;
  PElementNodeData *_em = nullptr;

  PElementNodeData *_sr = nullptr;
  PElementNodeData *_fi = nullptr;
  double _sr_min = 0;
  int _eid_sr_min = 0;

  // Element integration point data
  std::vector<PElementNodeData *> _si = {};
  std::vector<PElementNodeData *> _ei = {};
  std::vector<PElementNodeData *> _sim = {};
  std::vector<PElementNodeData *> _eim = {};

  // Element nodal data
  std::vector<PElementNodeData *> _sn = {};
  std::vector<PElementNodeData *> _en = {};
  std::vector<PElementNodeData *> _snm = {};
  std::vector<PElementNodeData *> _enm = {};


public:
  LocalState() = default;
  LocalState(int id) : _loadcase_id(id) {};

  int id() const { return _loadcase_id; }
  std::string name() const { return _loadcase_name; }

  void setId(int id) { _loadcase_id = id; }
  void setName(std::string name) { _loadcase_name = name; }

  void setU(PElementNodeData *u) { _u = u; }
  void setS(PElementNodeData *s) { _s = s; }
  void setE(PElementNodeData *e) { _e = e; }
  void setSM(PElementNodeData *sm) { _sm = sm; }
  void setEM(PElementNodeData *em) { _em = em; }

  void setSR(PElementNodeData *sr) { _sr = sr; }
  void setFI(PElementNodeData *fi) { _fi = fi; }
  void setMinSR(double sr_min) { _sr_min = sr_min; }
  void setMinSRElementId(int eid_sr_min) { _eid_sr_min = eid_sr_min; }

  void addSN(PElementNodeData *sn) { _sn.push_back(sn); }
  void addEN(PElementNodeData *en) { _en.push_back(en); }
  void addSNM(PElementNodeData *snm) { _snm.push_back(snm); }
  void addENM(PElementNodeData *enm) { _enm.push_back(enm); }

  void writeGmshMsh(FILE *, FILE *, Message *);
  // void writeGmshMsh(std::string &, std::string &, Message *);

  // PElementNodeData *u() { return _u; }
  // PElementNodeData *s() { return _s; }
  // PElementNodeData *e() { return _e; }
  // PElementNodeData *sm() { return _sm; }
  // PElementNodeData *em() { return _em; }
};




/** @ingroup cs
 * A cross-sectional model class.
 */
class PModel {
private:
  std::string _name;
  // GModel *_gmodel, *_gmodel_debug;
  PDCEL *_dcel;

  double _global_mesh_size;
  unsigned int _element_type; // 1: linear, 2: quadratic

  // Analysis
  unsigned int _analysis_model_dim; // model dimension (1: beam, 2: plate/shell, 3: solid)
  unsigned int _analysis_model; // structural model (0: classical, 1: timoshenko)
  unsigned int _analysis_dehomo; // 1: nonlinear, 2: linear
  unsigned int _analysis_damping; // 0: off, 1: on
  unsigned int _analysis_thermal;
  unsigned int _analysis_curvature;
  unsigned int _analysis_oblique;
  unsigned int _analysis_trapeze;
  unsigned int _analysis_vlasov;
  std::vector<double> _analysis_curvatures;
  std::vector<double> _analysis_obliques;

  // analysis physics
  // 0: elastic
  // 1: thermoelastic
  // 2: conduction
  // 3: piezoelectric/piezomagnetic
  // 4: thermopiezoelectric/thermopiezomagnetic
  // 5: piezoelectromagnetic
  // 6: thermopiezoelectromagnetic
  // 7: viscoelastic
  // 8: thermoviscoelastic
  unsigned int _physics{0};

  // Repositories
  // std::vector<Material *> _materials;
  // std::vector<Lamina *> _laminas;
  // std::vector<Basepoint> basepoints;
  std::vector<PDCELVertex *> _vertices;
  std::vector<Baseline *> _baselines;
  std::vector<Material *> _materials;
  std::vector<Lamina *> _laminas;
  std::vector<LayerType *> _layertypes;
  std::vector<Layup *> _layups;
  // std::vector<CrossSection *> crosssections;

  std::vector<LoadCase> _load_cases;

  CrossSection *_cross_section;

  PMesh *_pmesh;
  std::vector<std::vector<int>> _node_elements;
  size_t _num_nodes = 0;
  size_t _num_elements = 0;

  std::vector<LocalState *> _local_states;

  int debug_plot_count = 0;

  double _omega = 1.0;  // For SwiftComp. Length/area/volume of the periodic dimensions

  // For supplement files
  bool _itf_output = false;
  double _itf_theta1_diff_th = 30;
  double _itf_theta3_diff_th = 30;
  // double _itf_radius;
  std::vector<std::pair<int, int>> _itf_material_pairs;
  std::vector<std::pair<double, double>> _itf_theta1_pairs;
  std::vector<std::pair<double, double>> _itf_theta3_pairs;
  std::vector<std::vector<PDCELVertex *>> _itf_vertices;
  std::vector<std::vector<PDCELHalfEdge *>> _itf_halfedges;
  // std::vector<int> _joint_node_ids;
  // std::vector<int> _joint_element_ids;

public:
  std::vector<std::vector<int>> node_elements;

public:
  PModel() {}
  PModel(std::string);

  void initialize();
  void finalize();

  // =================================================================
  // Getters

  // GModel *gmodel() { return _gmodel; }
  PDCEL *dcel() { return _dcel; }

  double globalMeshSize() { return _global_mesh_size; }
  unsigned int elementType() { return _element_type; }

  unsigned int analysisModelDim() { return _analysis_model_dim; }
  unsigned int analysisModel() { return _analysis_model; }
  unsigned int analysisDehomo() { return _analysis_dehomo; }
  unsigned int analysisDamping() { return _analysis_damping; }
  unsigned int analysisThermal() { return _analysis_thermal; }
  unsigned int analysisCurvature() { return _analysis_curvature; }
  unsigned int analysisOblique() { return _analysis_oblique; }
  unsigned int analysisTrapeze() { return _analysis_trapeze; }
  unsigned int analysisVlasov() { return _analysis_vlasov; }

  unsigned int analysisPhysics() { return _physics; }

  std::vector<double> &curvatures() { return _analysis_curvatures; }
  std::vector<double> &obliques() { return _analysis_obliques; }

  std::size_t getNumOfMaterials() { return _materials.size(); }
  std::size_t getNumOfLayerTypes() { return _layertypes.size(); }
  std::size_t getNumOfNodes() { return _num_nodes; }
  std::size_t getNumOfElements() { return _num_elements; }

  void getNodes(std::vector<size_t> &, std::vector<double> &, Message *);
  void getElements(
    std::vector<int> &,
    std::vector<std::vector<size_t>> &,
    std::vector<std::vector<size_t>> &,
    int, int, Message *
    );

  std::vector<PDCELVertex *> &vertices() { return _vertices; }
  std::vector<Baseline *> &baselines() { return _baselines; }
  std::vector<Material *> &materials() { return _materials; }
  std::vector<LayerType *> &layertypes() { return _layertypes; }
  std::vector<Layup *> &layups() { return _layups; }

  // std::vector<PDCELVertex *> &getJointVertices() { return _joint_vertices; }
  // std::vector<PDCELHalfEdge *> &getJointHalfEdges() { return _joint_halfedges; }

  std::vector<LoadCase> getLoadCases() const { return _load_cases; }

  CrossSection *cs() { return _cross_section; }
  PMesh *getMesh() { return _pmesh; }
  std::vector<std::vector<int>> getNodeElements() { return _node_elements; }

  PDCELVertex *getPointByName(std::string);
  Baseline *getBaselineByName(std::string);
  /// Get a copy of the baseline with the given name
  Baseline *getBaselineByNameCopy(std::string);
  Material *getMaterialByName(std::string);
  Lamina *getLaminaByName(std::string);
  LayerType *getLayerTypeByMaterialAngle(Material *, double);
  LayerType *getLayerTypeByMaterialNameAngle(std::string, double);
  Layup *getLayupByName(std::string);

  std::vector<std::pair<int, int>> getInterfaceMaterialPairs() { return _itf_material_pairs; }
  std::vector<std::pair<double, double>> getInterfaceTheta1Pairs() { return _itf_theta1_pairs; }
  std::vector<std::pair<double, double>> getInterfaceTheta3Pairs() { return _itf_theta3_pairs; }
  std::vector<std::vector<PDCELVertex *>> getInterfaceVertices() { return _itf_vertices; }
  std::vector<std::vector<PDCELHalfEdge *>> getInterfaceHalfEdges() { return _itf_halfedges; }

  /// Turn and collect integer type parameters for Gmsh writing SG input file
  /*!
    \return A list of integer type parameters.
      (trans_flag is   1)
    
    index    description
        0    vabs (1) or swiftcomp (2)
        1    element type (linear (1) or quadratic (2))
        2    (vabs/sc) number of materals
        3    (vabs/sc) number of layer types
        4    (vabs/sc) analysis type (vabs has only elastic and thermalelastic)
        5    (vabs/sc) model type (classical (0), timoshenko (1), vlasov (2 for sc), trapeze (3 for sc))
        6    (vabs/sc) trapeze_flag
        7    (vabs/sc) vlasov_flag
        8    (vabs) curve_flag
        9    (vabs) oblique_flag
       10    (sc) elem_flag (degenerated elements or not)
       11    (sc) temp_flag (uniform temperature or not)
       12    (sc) slave nodes
  */
  std::vector<int> &getIntParamsForGmsh();

  /// Collect double type parameters for Gmsh writing SG input file
  /*!
    \return A list of double type parameters.

    index    description
        0    curvature k1
        1    curvature k2
        2    curvature k3
        3    oblique cos11
        4    oblique cos21
   */
  std::vector<double> &getDoubleParamsForGmsh();

  double getOmega() { return _omega; }

  //
  bool interfaceOutput() { return _itf_output; }

  // =================================================================
  // Setters

  void setGlobalMeshSize(double s) { _global_mesh_size = s; }
  void setElementType(int t) { _element_type = t; }

  void setAnalysisModelDim(int a) { _analysis_model_dim = a; }
  void setAnalysisModel(int a) { _analysis_model = a; }
  void setAnalysisDehomo(int a) { _analysis_dehomo = a; }
  void setAnalysisDamping(int a) { _analysis_damping = a; }
  void setAnalysisThermal(int a) { _analysis_thermal = a; }
  void setAnalysisCurvature(int a) { _analysis_curvature = a; }
  void setAnalysisOblique(int a) { _analysis_oblique = a; }
  void setAnalysisTrapeze(int a) { _analysis_trapeze = a; }
  void setAnalysisVlasov(int a) { _analysis_vlasov = a; }

  void setAnalysisPhysics(int a) { _physics = a; }

  void setCurvatures(double, double, double);
  void setObliques(double, double);

  void setCrossSection(CrossSection *cs) { _cross_section = cs; }

  void setMesh(PMesh *mesh) { _pmesh = mesh; }
  void setNumNodes(size_t n) { _num_nodes = n; }
  void setNumElements(size_t n) { _num_elements = n; }

  void setOmega(double o) { _omega = o; }

  void setInterfaceOutput(bool itf) { _itf_output = itf; }
  void setInterfaceTheta1DiffThreshold(double t) { _itf_theta1_diff_th = t; }
  void setInterfaceTheta3DiffThreshold(double t) { _itf_theta3_diff_th = t; }

  // =================================================================
  // Adders

  void addVertex(PDCELVertex *v) { _vertices.push_back(v); }
  void addBaseline(Baseline *l) { _baselines.push_back(l); }
  void addMaterial(Material *m) { _materials.push_back(m); }
  void addLamina(Lamina *l) { _laminas.push_back(l); }
  void addLayerType(LayerType *lt) { _layertypes.push_back(lt); }
  void addLayerType(Material *m, double a);
  void addLayerType(std::string name, double a);
  void addLayup(Layup *l) { _layups.push_back(l); }

  // void addJointVertex(PDCELVertex *v) { _joint_vertices.push_back(v); }
  // void addJointHalfEdge(PDCELHalfEdge *e) { _joint_halfedges.push_back(e); }

  void addLoadCase(LoadCase lc) { _load_cases.push_back(lc); }
  void addLocalState(LocalState *ls) { _local_states.push_back(ls); }

  void addNodeElement(int nid, int eid) { _node_elements[nid-1].push_back(eid); }

  // =================================================================
  // IO

  int indexGmshElements();

  int writeGmshGeo(const std::string &, Message *);
  int writeGmshMsh(const std::string &, Message *);
  int writeGmshOpt(const std::string &, Message *);

  /// Write the Gmsh file (.geo, .msh, and .opt)
  /*!
    \param fn File name without the extension
   */
  int writeGmsh(const std::string &, Message *);

  /// Write the SG model data into a file.
  /*!
    \param fn File name without the extension
    \param fmt Format (1: VABS, 2: SwiftComp)
   */
  int writeSG(std::string fn, int fmt, Message *);

  void writeNodes(FILE *, const std::vector<size_t> &, const std::vector<double> &, Message *);

  void writeElements(
    FILE *,
    const std::vector<std::vector<int>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<size_t> &,
    const std::vector<double> &,
    Message *
    );

  void writeElementsVABS(
    FILE *,
    const std::vector<std::vector<int>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<size_t> &,
    const std::vector<double> &,
    Message *
    );

  void writeElementsSC(FILE *, Message *);
  // void writeMaterialsVABS(FILE *, Message *);
  // void writeSettingsVABS(FILE *, Message *);
  int writeGLB(std::string fn, Message *);

  // Write supplement files
  int writeSupp(Message *);


  // =================================================================

  void summary(Message *);


  // Homogenization
  void homogenize(Message *);
  void build(Message *);

  // void initGmshModel(Message *);
  void createGmshVertices(Message *);
  void createGmshEdges(Message *);
  void createGmshFaces(Message *);
  void createGmshEmbeddedEntities(Message *);
  void createGmshPhyscialGroups(Message *);
  void createGmshGeo(Message *);
  void recordInterface(PDCELHalfEdge *, Message *);

  void buildGmsh(Message *);


  // Dehomogenization
  void dehomogenize(Message *);

  void recoverVABS();
  void failureVABS(Message *);
  void dehomoSC();
  void failureSC();


  // Execution
  void run(Message *);


  // Plot functions
  void plotGeoDebug(Message *, bool create_gmsh_geo=true);
  void plot(Message *);
  void plotDehomo(Message *);
  
  int postVABS(Message *);
  int postSCDehomo();
  int postSCFailure();
};
