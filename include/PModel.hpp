#pragma once

// Forward declarations first — before any includes — to break circular
// dependencies in the include chain.
class Baseline;
class CrossSection;
class Lamina;
class LayerType;
class Layup;
class Material;
class Message;
class PDCEL;
class PDCELFace;
class PDCELHalfEdge;
class PDCELVertex;
class PElementNodeData;
class PMesh;

#include "declarations.hpp"
#include "globalVariables.hpp"
#include "Material.hpp"
#include "PDCEL.hpp"
#include "PDCELFaceData.hpp"
#include "PDCELVertexData.hpp"
#include "PSegment.hpp"
#include "utilities.hpp"
#include "GeometryRepository.hpp"
#include "MaterialRepository.hpp"
#include "MeshData.hpp"

// #include "gmsh/GModel.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

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

  void writeGmshMsh(FILE *, FILE *);

  // PElementNodeData *u() { return _u; }
  // PElementNodeData *s() { return _s; }
  // PElementNodeData *e() { return _e; }
  // PElementNodeData *sm() { return _sm; }
  // PElementNodeData *em() { return _em; }
};


/// Groups post-processing results: load cases and per-load-case local states.
struct PostProcessingData {
  std::vector<LoadCase>    load_cases;
  std::vector<LocalState*> local_states;
};



/** @ingroup cs
 * A cross-sectional model class.
 */
class PModel : public IMaterialLookup {
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

  // Sub-repositories (Step 6 split)
  MaterialRepository  _mat_repo;
  GeometryRepository  _geo_repo;
  MeshData            _mesh_data;
  PostProcessingData  _pp_data;

  CrossSection *_cross_section;

  // Domain property maps — keyed by DCEL object pointer; owned by PModel.
  std::unordered_map<PDCELFace*,   PDCELFaceData>   _face_data;
  std::unordered_map<PDCELVertex*, PDCELVertexData> _vertex_data;

  // Gmsh entity maps — Gmsh integer tags for DCEL objects, owned by the Gmsh bridge.
  // Keyed by pointer; cleared between debug geometry plots.
  std::unordered_map<PDCELVertex*, int> _gmsh_vertex_tags;
  std::unordered_map<PDCELHalfEdge*, int> _gmsh_edge_tags;
  std::unordered_map<PDCELFace*, int> _gmsh_face_tags;
  std::unordered_map<PDCELFace*, std::vector<int>> _gmsh_face_embedded_vertex_tags;
  std::unordered_map<PDCELFace*, std::vector<int>> _gmsh_face_embedded_edge_tags;
  std::unordered_map<PDCELFace*, int> _gmsh_face_physical_group_tags;

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

public:
  std::vector<std::vector<int>> node_elements;

public:
  PModel() {}
  PModel(std::string);

  void initialize();
  void finalize();

  // =================================================================
  // Sub-repository accessors

  MaterialRepository& materialRepository() { return _mat_repo; }
  GeometryRepository& geometryRepository() { return _geo_repo; }
  MeshData&           meshData()           { return _mesh_data; }
  PostProcessingData& ppData()             { return _pp_data; }

  // =================================================================
  // Getters

  // GModel *gmodel() { return _gmodel; }
  PDCEL *dcel() { return _dcel; }

  // Face property map accessor.  Returns a reference to the PDCELFaceData
  // for face f, default-constructing an entry if f is not yet present.
  PDCELFaceData&   faceData(PDCELFace* f)     { return _face_data[f]; }
  // Vertex property map accessor.  Returns a reference to the PDCELVertexData
  // for vertex v, default-constructing an entry if v is not yet present.
  PDCELVertexData& vertexData(PDCELVertex* v)  { return _vertex_data[v]; }

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

  std::size_t getNumOfMaterials()  { return _mat_repo.numMaterials(); }
  std::size_t getNumOfLayerTypes() { return _mat_repo.numLayerTypes(); }
  std::size_t getNumOfNodes()      { return _mesh_data.num_nodes; }
  std::size_t getNumOfElements()   { return _mesh_data.num_elements; }

  void getNodes(std::vector<size_t> &, std::vector<double> &);
  void getElements(
    std::vector<int> &,
    std::vector<std::vector<size_t>> &,
    std::vector<std::vector<size_t>> &,
    int, int
    );

  // Delegating to sub-repositories
  std::vector<PDCELVertex *> &vertices()  { return _geo_repo.vertices(); }
  std::vector<Baseline *>    &baselines() { return _geo_repo.baselines(); }
  std::vector<Material *>    &materials() { return _mat_repo.materials(); }
  std::vector<LayerType *>   &layertypes(){ return _mat_repo.layertypes(); }
  std::vector<Layup *>       &layups()    { return _mat_repo.layups(); }

  std::vector<LoadCase> getLoadCases() const { return _pp_data.load_cases; }

  CrossSection *cs() { return _cross_section; }
  PMesh *getMesh()   { return _mesh_data.mesh; }
  std::vector<std::vector<int>> getNodeElements() { return _mesh_data.node_elements; }

  PDCELVertex *getPointByName(std::string name)
    { return _geo_repo.getPointByName(name); }
  Baseline *getBaselineByName(std::string name)
    { return _geo_repo.getBaselineByName(name); }
  /// Get a copy of the baseline with the given name
  Baseline *getBaselineByNameCopy(std::string name)
    { return _geo_repo.getBaselineByNameCopy(name); }

  Material  *getMaterialByName(std::string name)
    { return _mat_repo.getMaterialByName(name); }
  Lamina    *getLaminaByName(std::string name)
    { return _mat_repo.getLaminaByName(name); }
  LayerType *getLayerTypeByMaterialAngle(Material *m, double angle) override
    { return _mat_repo.getLayerTypeByMaterialAngle(m, angle); }
  LayerType *getLayerTypeByMaterialNameAngle(std::string name, double angle)
    { return _mat_repo.getLayerTypeByMaterialNameAngle(name, angle); }
  Layup     *getLayupByName(std::string name)
    { return _mat_repo.getLayupByName(name); }

  /// Look up the Gmsh edge tag for a half-edge (returns 0 if not found).
  int getGmshEdgeTag(PDCELHalfEdge* he) const {
    auto it = _gmsh_edge_tags.find(he);
    return (it != _gmsh_edge_tags.end()) ? it->second : 0;
  }

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

  void setMesh(PMesh *mesh)       { _mesh_data.mesh = mesh; }
  void setNumNodes(size_t n)      { _mesh_data.num_nodes = n; }
  void setNumElements(size_t n)   { _mesh_data.num_elements = n; }

  void setOmega(double o) { _omega = o; }

  void setInterfaceOutput(bool itf) { _itf_output = itf; }
  void setInterfaceTheta1DiffThreshold(double t) { _itf_theta1_diff_th = t; }
  void setInterfaceTheta3DiffThreshold(double t) { _itf_theta3_diff_th = t; }

  // =================================================================
  // Adders

  void addVertex(PDCELVertex *v)  { _geo_repo.addVertex(v); }
  void addBaseline(Baseline *l)   { _geo_repo.addBaseline(l); }
  void addMaterial(Material *m)   { _mat_repo.addMaterial(m); }
  void addLamina(Lamina *l)       { _mat_repo.addLamina(l); }
  void addLayerType(LayerType *lt){ _mat_repo.addLayerType(lt); }
  void addLayerType(Material *m, double a)      { _mat_repo.addLayerType(m, a); }
  void addLayerType(std::string name, double a) { _mat_repo.addLayerType(name, a); }
  void addLayup(Layup *l)         { _mat_repo.addLayup(l); }

  void addLoadCase(LoadCase lc)   { _pp_data.load_cases.push_back(lc); }
  void addLocalState(LocalState *ls) { _pp_data.local_states.push_back(ls); }

  void addNodeElement(int nid, int eid) { _mesh_data.node_elements[nid-1].push_back(eid); }

  // =================================================================
  // IO

  int indexGmshElements();

  int writeGmshGeo(const std::string &);
  int writeGmshMsh(const std::string &);
  int writeGmshOpt(const std::string &);

  /// Write the Gmsh file (.geo, .msh, and .opt)
  /*!
    \param fn File name without the extension
   */
  int writeGmsh(const std::string &);

  /// Write the SG model data into a file.
  /*!
    \param fn File name without the extension
    \param wcfg Writer configuration (analysis tool, dehomo flag, etc.)
   */
  int writeSG(std::string fn, const WriterConfig &);

  void writeNodes(FILE *, const std::vector<size_t> &, const std::vector<double> &);

  void writeElements(
    FILE *,
    const std::vector<std::vector<int>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<size_t> &,
    const std::vector<double> &
    );

  void writeElementsVABS(
    FILE *,
    const std::vector<std::vector<int>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<std::vector<std::vector<size_t>>> &,
    const std::vector<size_t> &,
    const std::vector<double> &
    );

  void writeElementsSC(FILE *);
  int writeGLB(std::string fn);

  // Write supplement files
  int writeSupp();


  // =================================================================

  void summary();


  // Homogenization
  void homogenize();
  void build();

  void createGmshVertices();
  void createGmshEdges();
  void createGmshFaces();
  void createGmshEmbeddedEntities();
  void createGmshPhyscialGroups();
  void createGmshGeo();
  void recordInterface(PDCELHalfEdge *);

  void buildGmsh();


  // Dehomogenization
  void dehomogenize();

  void recoverVABS();
  void failureVABS();
  void dehomoSC();
  void failureSC();


  // Execution
  void run();


  // Plot functions
  void plotGeoDebug(bool create_gmsh_geo=true);
  void plot();
  void plotDehomo();

  int postVABS();
  int postSCDehomo();
  int postSCFailure();
};
