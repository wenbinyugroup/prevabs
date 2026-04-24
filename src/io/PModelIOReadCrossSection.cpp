#include "PModelIO.hpp"

#include "CrossSection.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <list>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __linux__
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#elif _WIN32
#include <tchar.h>
#include <windows.h>
#endif

namespace {

unsigned int parseElementShapeValue(const std::string &value) {
  const std::string s = lowerString(trim(value));
  if (s == "3" || s == "tri" || s == "triangle") {
    return 3;
  }
  if (s == "4" || s == "quad" || s == "quadrilateral") {
    return 4;
  }
  throw std::runtime_error(
    "Invalid <element_shape> value: " + value
    + " (expected 3/tri/triangle or 4/quad/quadrilateral)"
  );
}

}  // namespace

int readCrossSection(const std::string &filenameCrossSection,
                     const std::string &filePath, PModel *pmodel) {
  rapidxml::xml_document<> xmlDocCrossSection;
  std::ifstream fileCrossSection{filenameCrossSection};
  if (!fileCrossSection.is_open()) {
    PLOG(error) << "unable to open file: " << filenameCrossSection;
    return 1;
  } else {
    PLOG(info) << "reading main input file: " << filenameCrossSection;
  }

  std::vector<char> buffer{(std::istreambuf_iterator<char>(fileCrossSection)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xmlDocCrossSection.parse<0>(&buffer[0]);
  } catch (rapidxml::parse_error &e) {
    throw std::runtime_error(
      "XML parse error in '" + filenameCrossSection + "': " + e.what()
    );
  }

  rapidxml::xml_node<> *p_xn_sg{xmlDocCrossSection.first_node("cross_section")};
  if (!p_xn_sg) {
    p_xn_sg = xmlDocCrossSection.first_node("sg");
  }
  if (!p_xn_sg) {
    throw std::runtime_error("Missing root XML element <cross_section> or <sg>");
  }
  rapidxml::xml_attribute<> *p_xa_name{p_xn_sg->first_attribute("name")};
  if (!p_xa_name) {
    throw std::runtime_error("Missing required attribute 'name' on root element");
  }
  std::string csName{p_xa_name->value()};
  std::string cs_type{"general"};
  if (p_xn_sg->first_attribute("type")) {
    cs_type = p_xn_sg->first_attribute("type")->value();
  }
  PLOG(debug) << "find cross-section: " + csName;

  // CrossSection cs{csName};
  CrossSection *cs = new CrossSection(csName);

  // format: optional; 0 = layups in a separate include file,
  //         1 (default) = layups inline in the main file
  int format_version{1};
  rapidxml::xml_attribute<> *p_xa_fmt{p_xn_sg->first_attribute("format")};
  if (p_xa_fmt) {
    std::string ss{p_xa_fmt->value()};
    if (ss[0] != '\0') {
      format_version = parseFormatVersionValue(
        ss, "root attribute 'format'"
      );
    }
  }

  // -----------------------------------------------------------------
  // Read settings
  rapidxml::xml_node<> *p_xn_settings{p_xn_sg->first_node("settings")};
  if (p_xn_settings) {
        PLOG(debug) << "reading settings...";

    rapidxml::xml_node<> *p_xn_tolerance{p_xn_settings->first_node("tolerance")};
    if (p_xn_tolerance) {
      std::string ss{p_xn_tolerance->value()};
      if (ss[0] != '\0') {
        config.app.geo_tol = atof(ss.c_str());
      }
    }
  }

  // -----------------------------------------------------------------
  // Read analysis
  // All fields in <analysis> are optional; defaults represent a linear,
  // classical, undamped, isothermal, straight 1-D beam with no obliqueness.
    PLOG(debug) << "reading analysis...";

  int model_dim = 1;    // optional: 1D beam (default) or 2D plate
  int model = 0;        // optional: 0 = classical, 1 = refined
  int flag_damping = 0; // optional: 0 = no damping
  int flag_thermal = 0; // optional: 0 = isothermal
  int flag_curvature = 0;
  int flag_oblique = 0;
  int flag_trapeze = 0;
  int flag_vlasov = 0;
  double k1 = 0.0;      // optional: initial twist (rad/m)
  double k2 = 0.0;      // optional: initial curvature k2
  double k3 = 0.0;      // optional: initial curvature k3
  double cos11 = 1.0;   // optional: oblique direction cosine (default: normal)
  double cos21 = 0.0;

  rapidxml::xml_node<> *p_xn_analysis{p_xn_sg->first_node("analysis")};
  if (p_xn_analysis) {

    rapidxml::xml_node<> *p_xn_model_dim{p_xn_analysis->first_node("model_dim")};
    if (p_xn_model_dim) {
      std::string ss{p_xn_model_dim->value()};
      if (ss[0] != '\0') {
        model_dim = std::stoi(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
    if (p_xn_model) {
      std::string ss{p_xn_model->value()};
      if (ss[0] != '\0') {
        model = std::stoi(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_physics{p_xn_analysis->first_node("physics")};
    if (p_xn_physics) {
      std::string ss{p_xn_physics->value()};
      int _physics = std::stoi(ss.c_str());
      pmodel->setAnalysisPhysics(_physics);
    }

    rapidxml::xml_node<> *p_xn_damping{p_xn_analysis->first_node("damping")};
    if (p_xn_damping) {
      std::string ss{p_xn_damping->value()};
      if (ss[0] != '\0') {
        flag_damping = std::stoi(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_thermal{p_xn_analysis->first_node("thermal")};
    if (p_xn_thermal) {
      std::string ss{p_xn_thermal->value()};
      if (ss[0] != '\0') {
        flag_thermal = std::stoi(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_k1{p_xn_analysis->first_node("initial_twist")};
    if (p_xn_k1) {
      std::string ss{p_xn_k1->value()};
      if (ss[0] != '\0') {
        k1 = std::stod(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_k2{p_xn_analysis->first_node("initial_curvature_2")};
    if (p_xn_k2) {
      std::string ss{p_xn_k2->value()};
      if (ss[0] != '\0') {
        k2 = std::stod(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_k3{p_xn_analysis->first_node("initial_curvature_3")};
    if (p_xn_k3) {
      std::string ss{p_xn_k3->value()};
      if (ss[0] != '\0') {
        k3 = std::stod(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_cos11{p_xn_analysis->first_node("oblique_y1")};
    if (p_xn_cos11) {
      std::string ss{p_xn_cos11->value()};
      if (ss[0] != '\0') {
        cos11 = std::stod(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_cos21{p_xn_analysis->first_node("oblique_y2")};
    if (p_xn_cos21) {
      std::string ss{p_xn_cos21->value()};
      if (ss[0] != '\0') {
        cos21 = std::stod(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_trapeze{p_xn_analysis->first_node("trapeze")};
    if (p_xn_trapeze) {
      std::string ss{p_xn_trapeze->value()};
      if (ss[0] != '\0') {
        flag_trapeze = std::stoi(ss.c_str());
      }
    }

    rapidxml::xml_node<> *p_xn_vlasov{p_xn_analysis->first_node("vlasov")};
    if (p_xn_vlasov) {
      std::string ss{p_xn_vlasov->value()};
      if (ss[0] != '\0') {
        flag_vlasov = std::stoi(ss.c_str());
      }
    }

  }

  if (k1 != 0.0 || k2 != 0.0 || k3 != 0.0) {
    flag_curvature = 1;
  }

  if (cos11 != 1.0 || cos21 != 0.0) {
    flag_oblique = 1;
  }

    PLOG(debug) << "finished reading analysis.";

  pmodel->setAnalysisModelDim(model_dim);
  pmodel->setAnalysisModel(model);
  pmodel->setAnalysisDamping(flag_damping);
  pmodel->setAnalysisThermal(flag_thermal);
  pmodel->setAnalysisCurvature(flag_curvature);
  pmodel->setAnalysisOblique(flag_oblique);
  pmodel->setAnalysisTrapeze(flag_trapeze);
  pmodel->setAnalysisVlasov(flag_vlasov);

  pmodel->setCurvatures(k1, k2, k3);
  pmodel->setObliques(cos11, cos21);

  // -----------------------------------------------------------------
  // Read general
    PLOG(debug) << "reading general...";

  rapidxml::xml_node<> *nodeGeneral{p_xn_sg->first_node("general")};
  if (!nodeGeneral) {
    throw std::runtime_error("Missing required XML element <general>");
  }

  double dx{0}, dy{0}, dz{0};
  rapidxml::xml_node<> *nodeTranslate{nodeGeneral->first_node("translate")};
  if (nodeTranslate) {
    std::stringstream ss{nodeTranslate->value()};
    if (ss.str()[0] != '\0') {
      ss >> dy >> dz;
    }
  }

  double sfactor{1.0};
  rapidxml::xml_node<> *nodeScale{nodeGeneral->first_node("scale")};
  if (nodeScale) {
    std::string sscale{nodeScale->value()};
    if (sscale[0] != '\0')
      sfactor = std::stod(sscale.c_str());
  }
  double rangle{0.0};
  rapidxml::xml_node<> *nodeRotate{nodeGeneral->first_node("rotate")};
  if (nodeRotate) {
    std::string srotate{nodeRotate->value()};
    if (srotate[0] != '\0')
      rangle = std::stod(srotate.c_str());
  }
  double meshsize{0.0};
  rapidxml::xml_node<> *nodeMeshsize{nodeGeneral->first_node("mesh_size")};
  if (nodeMeshsize) {
    std::string smeshsize{nodeMeshsize->value()};
    if (smeshsize[0] != '\0')
      meshsize = std::stod(smeshsize.c_str());
  }
  int elementtype = 2;
  rapidxml::xml_node<> *nodeElementType{nodeGeneral->first_node("element_type")};
  if (nodeElementType) {
    if (nodeElementType->value()[0] != '\0') {
      std::string et{nodeElementType->value()};
      if (et == "linear" || et == "1") {
        elementtype = 1;
      } else if (et == "quadratic" || et == "2") {
        elementtype = 2;
      }
    }
  }
  pmodel->setElementType(elementtype);

  unsigned int elementshape = 3;
  rapidxml::xml_node<> *nodeElementShape{nodeGeneral->first_node("element_shape")};
  if (nodeElementShape) {
    std::string es{trim(nodeElementShape->value())};
    if (!es.empty()) {
      elementshape = parseElementShapeValue(es);
    }
  }
  pmodel->setElementShape(elementshape);

  rapidxml::xml_node<> *nodeTransfiniteAuto{
    nodeGeneral->first_node("transfinite_auto")
  };
  if (nodeTransfiniteAuto) {
    std::string s{trim(nodeTransfiniteAuto->value())};
    if (!s.empty()) {
      pmodel->setTransfiniteAuto(
        parseXmlBoolValue(s, "<general>/<transfinite_auto>")
      );
    }
  }

  rapidxml::xml_node<> *nodeTransfiniteCornerAngle{
    nodeGeneral->first_node("transfinite_corner_angle")
  };
  if (nodeTransfiniteCornerAngle) {
    std::string s{trim(nodeTransfiniteCornerAngle->value())};
    if (!s.empty()) {
      pmodel->setTransfiniteCornerAngle(std::stod(s));
    }
  }

  rapidxml::xml_node<> *nodeTransfiniteRecombine{
    nodeGeneral->first_node("transfinite_recombine")
  };
  if (nodeTransfiniteRecombine) {
    std::string s{trim(nodeTransfiniteRecombine->value())};
    if (!s.empty()) {
      pmodel->setTransfiniteRecombine(
        parseXmlBoolValue(s, "<general>/<transfinite_recombine>")
      );
    }
  }

  rapidxml::xml_node<> *nodeRecombine{nodeGeneral->first_node("recombine")};
  if (nodeRecombine) {
    std::string s{trim(nodeRecombine->value())};
    if (!s.empty()) {
      pmodel->setRecombine(parseXmlBoolValue(s, "<general>/<recombine>"));
    }
  }

  rapidxml::xml_node<> *nodeRecombineAngle{
    nodeGeneral->first_node("recombine_angle")
  };
  if (nodeRecombineAngle) {
    std::string s{trim(nodeRecombineAngle->value())};
    if (!s.empty()) {
      pmodel->setRecombineAngle(std::stod(s));
    }
  }

  double omega{1.0};
  rapidxml::xml_node<> *p_xn_omega{nodeGeneral->first_node("omega")};
  if (p_xn_omega) {
    std::string stol{p_xn_omega->value()};
    if (stol[0] != '\0') {
      omega = std::stod(stol.c_str());
    }
  }
  pmodel->setOmega(omega);

  rapidxml::xml_node<> *p_xn_tol{nodeGeneral->first_node("tolerance")};
  if (p_xn_tol) {
    std::string stol{p_xn_tol->value()};
    if (stol[0] != '\0')
      config.app.tol = atof(stol.c_str());
  }
  std::stringstream ss_tol;
  ss_tol << config.app.tol;
    PLOG(debug) << "tolerance = " + ss_tol.str();

  rapidxml::xml_node<> *p_xn_itf;

  bool track_interface;
  p_xn_itf = nodeGeneral->first_node("track_interface");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      track_interface = std::stoi(ss.c_str());
      pmodel->setInterfaceOutput(track_interface);
    }
  }

  double itf_t1d_th;
  p_xn_itf = nodeGeneral->first_node("interface_theta1_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t1d_th = std::stod(ss);
      pmodel->setInterfaceTheta1DiffThreshold(itf_t1d_th);
    }
  }

  double itf_t3d_th;
  p_xn_itf = nodeGeneral->first_node("interface_theta3_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t3d_th = std::stod(ss);
      pmodel->setInterfaceTheta3DiffThreshold(itf_t3d_th);
    }
  }

    PLOG(debug) << "finished reading general.";

  // -----------------------------------------------------------------
  // Read include
    PLOG(debug) << "finding includings...";
  rapidxml::xml_node<> *nodeInclude{p_xn_sg->first_node("include")};
  rapidxml::xml_node<> *nodeBaselines;

  // -----------------------------------------------------------------
  // Read geometry (base points and base lines)
    PLOG(debug) << "reading geometry...";

  // 1: Try to read geometry from a seperated file
  if (nodeInclude) {
    rapidxml::xml_node<> *p_xn_include_bsl{nodeInclude->first_node("baseline")};
    if (p_xn_include_bsl) {

      // General type cross-section
      if (cs_type == "general") {
        std::string filenameBaselines{p_xn_include_bsl->value()};
        filenameBaselines = filePath + filenameBaselines + ".xml";
        PLOG(debug) << "reading base line file: " + filenameBaselines;

        std::ifstream fileBaselines;
        openFile(fileBaselines, filenameBaselines);
        rapidxml::xml_document<> xmlDocBaselines;
        std::vector<char> buffer_bsl{(std::istreambuf_iterator<char>(fileBaselines)),
                                std::istreambuf_iterator<char>()};
        buffer_bsl.push_back('\0');

        try {
          xmlDocBaselines.parse<0>(&buffer_bsl[0]);
        }
        catch (rapidxml::parse_error &e) {
          throw std::runtime_error(
            "XML parse error in '" + filenameBaselines + "': " + e.what()
          );
        }
        nodeBaselines = xmlDocBaselines.first_node("baselines");

        readBaselines(nodeBaselines, pmodel, filePath, dx, dy, dz, sfactor, rangle);

      }

      // Airfoil type cross-section (removed)
      else if (cs_type == "airfoil") {
        throw std::runtime_error(
          "cs_type='airfoil' is no longer supported; "
          "use cs_type='general' with explicit <baselines>"
        );
      }

    }
  }

  // 2: Try to read geometry in the main input file
  nodeBaselines = p_xn_sg->first_node("baselines");

  if (nodeBaselines) {

    readBaselines(nodeBaselines, pmodel, filePath, dx, dy, dz, sfactor, rangle);

  }

  if (config.debug) {
    PLOG(debug) << " ";
    PLOG(debug) << "summary of base lines (before transformation)";
    for (auto bsl : pmodel->baselines()) {
      bsl->print();
      PLOG(debug) << " ";
    }
  }

  PLOG(debug) << "transforming base geometry";

  for (auto v : pmodel->vertices()) {
    v->translate(dx, dy, dz);
    v->scale(sfactor);
    v->rotate(rangle);
  }

  if (config.debug) {
    PLOG(debug) << " ";
    PLOG(debug) << "summary of base lines (after transformation)";
    for (auto bsl : pmodel->baselines()) {
      bsl->print();
      PLOG(debug) << " ";
    }
  }

    PLOG(debug) << "finished reading geometry.";

  // -----------------------------------------------------------------
  // Read materials (global)
    PLOG(debug) << "reading materials...";

  std::string fn_material_global = "";
// Use defualt database
#ifdef __linux__
  char buffer2[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", buffer2, sizeof(buffer2) - 1);
  if (count != -1) {
    buffer2[count] = '\0';
    fn_material_global = dirname(buffer2);
    fn_material_global = fn_material_global + "/";
  }
#elif _WIN32
  char buffer2[MAX_PATH];
  GetModuleFileNameA(NULL, buffer2, sizeof(buffer2));
  std::string s_fullpath{buffer2};
  std::vector<std::string> vs;
  vs = splitFilePath(s_fullpath);
  fn_material_global = vs[0];
#endif
  fn_material_global = fn_material_global + "MaterialDB.xml";

  std::string fn_material_local = "";
  if (nodeInclude) {
    rapidxml::xml_node<> *xn_material{nodeInclude->first_node("material")};
    if (xn_material) {
      fn_material_local = filePath + xn_material->value() + ".xml";
    }
  }

  if (fn_material_global != "") {
    readMaterialsFile(fn_material_global, pmodel);
  }

  if (fn_material_local != "") {
    readMaterialsFile(fn_material_local, pmodel);
  }

  rapidxml::xml_node<> *p_xn_materials{p_xn_sg->first_node("materials")};
  if (p_xn_materials) {
    readMaterials(p_xn_materials, pmodel);
  }

  PLOG(debug) << "finished reading materials.";

  // -----------------------------------------------------------------
  // Read layups
    PLOG(debug) << "reading layups...";

  rapidxml::xml_node<> *nodeLayups;
  if (format_version == 0) {
    if (nodeInclude) {
      rapidxml::xml_node<> *p_xn_include_lyp{nodeInclude->first_node("layup")};
      if (!p_xn_include_lyp) {
        throw std::runtime_error(
          "Missing required XML element <include>/<layup> for format version 0"
        );
      }
      std::string filenameLayups{p_xn_include_lyp->value()};
      filenameLayups = filePath + filenameLayups + ".xml";
      PLOG(info) << "include layups file: " << filenameLayups;
      std::ifstream fileLayups;
      openFile(fileLayups, filenameLayups);
      rapidxml::xml_document<> xmlDocLayups;
      std::vector<char> buffer_lyp{(std::istreambuf_iterator<char>(fileLayups)),
                              std::istreambuf_iterator<char>()};
      buffer_lyp.push_back('\0');

      try {
        xmlDocLayups.parse<0>(&buffer_lyp[0]);
      } catch (rapidxml::parse_error &e) {
        throw std::runtime_error(
          "XML parse error in '" + filenameLayups + "': " + e.what()
        );
      }
      nodeLayups = xmlDocLayups.first_node("layups");
      readLayups(nodeLayups, pmodel);
            PLOG(debug) << "finished reading layups.";
    }
    else {
      throw std::runtime_error(
        "Format version 0 requires an <include> section with <layup>"
      );
    }
  }

  else if (format_version == 1) {
    nodeLayups = p_xn_sg->first_node("layups");
    if (nodeLayups) {
      readLayups(nodeLayups, pmodel);
            PLOG(debug) << "finished reading layups.";
    }
  }
  else {
    throw std::runtime_error(
      "Unsupported format version '" + std::to_string(format_version)
      + "' on root element"
    );
  }

  // -----------------------------------------------------------------
  // Read components
  PLOG(debug) << "reading components...";

  std::vector<Layup *> p_layups{};  // All layups used in this cross section
  int num_combined_layups = 0;  // Number of combined layups

  std::vector<std::vector<std::string>> tmp_dependents_all;

  for (auto xn_component = p_xn_sg->first_node("component");
       xn_component; xn_component = xn_component->next_sibling("component")) {
    PComponent *p_component;

    p_component = readXMLElementComponent(
      xn_component, tmp_dependents_all, p_layups, num_combined_layups, cs, pmodel
    );

    cs->addComponent(p_component);
  }
    PLOG(debug) << "finished reading components.";

  // Turn the dependency of component names into pointers
  for (auto cmp : cs->components()) {
    for (auto depend_list : tmp_dependents_all) {
      if (cmp->name() == depend_list.front()) {
        for (int cni = 1; cni < depend_list.size(); ++cni) {
          for (auto cmp_d : cs->components()) {
            if (cmp_d->name() == depend_list[cni]) {
              cmp->addDependent(cmp_d);
              break;
            }
          }
        }
        break;
      }
    }
  }

  // Arrange the building order of components according to the dependencies
  cs->sortComponents();

  // Set default mesh size as the smallest layer thickness
  if (meshsize == 0.0) {
    PLOG(info) << "use default mesh size";
    Layup *p_layup{};
    for (auto it = p_layups.begin(); it != p_layups.end(); ++it) {
      p_layup = *it;
      std::vector<Layer> layers{};
      layers = p_layup->getLayers();
      for (auto it2 = layers.begin(); it2 != layers.end(); ++it2) {
        double thickness{it2->getLamina()->getThickness()};
        int stack{it2->getStack()};
        if (meshsize > thickness * stack || meshsize == 0.0)
          meshsize = thickness * stack;
      }
    }
  }
  pmodel->setGlobalMeshSize(meshsize);

  pmodel->setCrossSection(cs);

  return 0;
}
