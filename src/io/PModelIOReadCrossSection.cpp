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

// #include "gmsh/GModel.h"
// #include "gmsh/MTriangle.h"
// #include "gmsh/MVertex.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"
#include "gmsh_mod/StringUtils.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
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

using namespace rapidxml;

int CrossSection::used_material_index = 0;
int CrossSection::used_layertype_index = 0;
int PComponent::count_tmp = 0;
int Segment::count_tmp = 0;


void read_analysis(const xml_node<> *p_xn_analysis, PModel *pmodel, Message *pmessage) {
  PLOG(debug) << pmessage->message("reading analysis...");

  int model_dim = 1; // default 1D beam model
  int model = 0; // default classical model
  int flag_damping = 0;
  int flag_thermal = 0;
  int flag_curvature = 0;
  int flag_oblique = 0;
  int flag_trapeze = 0;
  int flag_vlasov = 0;
  double k1 = 0.0;
  double k2 = 0.0;
  double k3 = 0.0;
  double cos11 = 1.0;
  double cos21 = 0.0;

  if (!p_xn_analysis) {
    PLOG(error) << pmessage->message("null pointer reference in read_analysis");
    return;
  }

  xml_node<> *p_xn_model_dim{p_xn_analysis->first_node("model_dim")};
  if (p_xn_model_dim && p_xn_model_dim->value()) {
    std::string ss{p_xn_model_dim->value()};
    if (ss[0] != '\0') {
      model_dim = atoi(ss.c_str());
    }
  }

  xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
  if (p_xn_model && p_xn_model->value()) {
    std::string ss{p_xn_model->value()};
    if (ss[0] != '\0') {
      model = atoi(ss.c_str());
    }
  }

  xml_node<> *p_xn_physics{p_xn_analysis->first_node("physics")};
  if (p_xn_physics && p_xn_physics->value()) {
    std::string ss{p_xn_physics->value()};
    int _physics = atoi(ss.c_str());
    pmodel->setAnalysisPhysics(_physics);
  }

  xml_node<> *p_xn_damping{p_xn_analysis->first_node("damping")};
  if (p_xn_damping && p_xn_damping->value()) {
    std::string ss{p_xn_damping->value()};
    if (ss[0] != '\0') {
      flag_damping = atoi(ss.c_str());
    }
  }

  xml_node<> *p_xn_thermal{p_xn_analysis->first_node("thermal")};
  if (p_xn_thermal && p_xn_thermal->value()) {
    std::string ss{p_xn_thermal->value()};
    if (ss[0] != '\0') {
      flag_thermal = atoi(ss.c_str());
    }
  }

  xml_node<> *p_xn_k1{p_xn_analysis->first_node("initial_twist")};
  if (p_xn_k1 && p_xn_k1->value()) {
    std::string ss{p_xn_k1->value()};
    if (ss[0] != '\0') {
      k1 = atof(ss.c_str());
    }
  }

  xml_node<> *p_xn_k2{p_xn_analysis->first_node("initial_curvature_2")};
  if (p_xn_k2 && p_xn_k2->value()) {
    std::string ss{p_xn_k2->value()};
    if (ss[0] != '\0') {
      k2 = atof(ss.c_str());
    }
  }

  xml_node<> *p_xn_k3{p_xn_analysis->first_node("initial_curvature_3")};
  if (p_xn_k3 && p_xn_k3->value()) {
    std::string ss{p_xn_k3->value()};
    if (ss[0] != '\0') {
      k3 = atof(ss.c_str());
    }
  }

  xml_node<> *p_xn_cos11{p_xn_analysis->first_node("oblique_y1")};
  if (p_xn_cos11 && p_xn_cos11->value()) {
    std::string ss{p_xn_cos11->value()};
    if (ss[0] != '\0') {
      cos11 = atof(ss.c_str());
    }
  }

  xml_node<> *p_xn_cos21{p_xn_analysis->first_node("oblique_y2")};
  if (p_xn_cos21 && p_xn_cos21->value()) {
    std::string ss{p_xn_cos21->value()};
    if (ss[0] != '\0') {
      cos21 = atof(ss.c_str());
    }
  }

  xml_node<> *p_xn_trapeze{p_xn_analysis->first_node("trapeze")};
  if (p_xn_trapeze && p_xn_trapeze->value()) {
    std::string ss{p_xn_trapeze->value()};
    if (ss[0] != '\0') {
      flag_trapeze = atoi(ss.c_str());
    }
  }

  xml_node<> *p_xn_vlasov{p_xn_analysis->first_node("vlasov")};
  if (p_xn_vlasov && p_xn_vlasov->value()) {
    std::string ss{p_xn_vlasov->value()};
    if (ss[0] != '\0') {
      flag_vlasov = atoi(ss.c_str());
    }
  }


  if (k1 != 0.0 || k2 != 0.0 || k3 != 0.0) {
    flag_curvature = 1;
  }

  if (cos11 != 1.0 || cos21 != 0.0) {
    flag_oblique = 1;
  }
  
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

  PLOG(debug) << pmessage->message("finished reading analysis.");
}









void read_general(const xml_node<> *p_xn_general, PModel *pmodel, Message *pmessage) {
  PLOG(debug) << pmessage->message("reading general...");

  double dx{0}, dy{0}, dz{0};
  xml_node<> *nodeTranslate{p_xn_general->first_node("translate")};
  if (nodeTranslate) {
    std::stringstream ss{nodeTranslate->value()};
    if (ss.str()[0] != '\0') {
      ss >> dy >> dz;
    }
  }
  pmodel->setGlobalTranslate(dy, dz);

  double sfactor{1.0};
  xml_node<> *nodeScale{p_xn_general->first_node("scale")};
  if (nodeScale) {
    std::string sscale{nodeScale->value()};
    if (sscale[0] != '\0')
      sfactor = atof(sscale.c_str());
  }
  pmodel->setGlobalScale(sfactor);

  double rangle{0.0};
  xml_node<> *nodeRotate{p_xn_general->first_node("rotate")};
  if (nodeRotate) {
    std::string srotate{nodeRotate->value()};
    if (srotate[0] != '\0')
      rangle = atof(srotate.c_str());
  }
  pmodel->setGlobalRotate(rangle);

  double meshsize{0.0};
  xml_node<> *nodeMeshsize{p_xn_general->first_node("mesh_size")};
  if (nodeMeshsize) {
    std::string smeshsize{nodeMeshsize->value()};
    if (smeshsize[0] != '\0')
      meshsize = atof(smeshsize.c_str());
  }
  pmodel->setGlobalMeshSize(meshsize);

  int elementtype = 2;
  xml_node<> *nodeElementType{p_xn_general->first_node("element_type")};
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


  double omega{1.0};
  xml_node<> *p_xn_omega{p_xn_general->first_node("omega")};
  if (p_xn_omega) {
    std::string stol{p_xn_omega->value()};
    if (stol[0] != '\0') {
      omega = atof(stol.c_str());
    }
  }
  pmodel->setOmega(omega);


  xml_node<> *p_xn_tol{p_xn_general->first_node("tolerance")};
  if (p_xn_tol) {
    std::string stol{p_xn_tol->value()};
    if (stol[0] != '\0')
      config.tol = atof(stol.c_str());
  }
  std::stringstream ss_tol;
  ss_tol << config.tol;
  PLOG(debug) << pmessage->message("tolerance = " + ss_tol.str());


  xml_node<> *p_xn_itf;

  bool track_interface;
  p_xn_itf = p_xn_general->first_node("track_interface");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      track_interface = atoi(ss.c_str());
      pmodel->setInterfaceOutput(track_interface);
    }
  }


  double itf_t1d_th;
  p_xn_itf = p_xn_general->first_node("interface_theta1_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t1d_th = atof(ss.c_str());
      pmodel->setInterfaceTheta1DiffThreshold(itf_t1d_th);
    }
  }


  double itf_t3d_th;
  p_xn_itf = p_xn_general->first_node("interface_theta3_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t3d_th = atof(ss.c_str());
      pmodel->setInterfaceTheta3DiffThreshold(itf_t3d_th);
    }
  }


  PLOG(debug) << pmessage->message("finished reading general.");
}









void read_geometry(
  const xml_node<> *p_xn_geometry_include, const xml_node<> *p_xn_geometry_main,
  const std::string &cs_type, const std::string &filePath,
  const xml_node<> *p_xn_sg, PModel *pmodel, Message *pmessage
  ) {
  PLOG(debug) << pmessage->message("reading geometry...");

  double dx = 0.0;
  double dy = pmodel->global_translate()[0];
  double dz = pmodel->global_translate()[1];
  double sfactor = pmodel->global_scale();
  double rangle = pmodel->global_rotate();

  // 1: Try to read geometry from a seperated file
  if (p_xn_geometry_include) {

    // General type cross-section
    if (cs_type == "general") {
      std::string filenameBaselines{p_xn_geometry_include->value()};
      filenameBaselines = filePath + filenameBaselines + ".xml";
      // if (debug) {
      //   std::cout << markInfo << " Include Baselines File: " << filenameBaselines
      //             << std::endl;
      // }
      PLOG(debug) << pmessage->message("reading base line file: " + filenameBaselines);

      std::ifstream fileBaselines;
      openFile(fileBaselines, filenameBaselines);
      xml_document<> xmlDocBaselines;
      std::vector<char> buffer_bsl{(std::istreambuf_iterator<char>(fileBaselines)),
                              std::istreambuf_iterator<char>()};
      buffer_bsl.push_back('\0');

      try {
        xmlDocBaselines.parse<0>(&buffer_bsl[0]);
      }
      catch (parse_error &e) {
        // std::cout << markError << " Unable to parse the file: " << filenameBaselines
        //           << std::endl;
        PLOG(error) << pmessage->message("unable to parse the file: " + filenameBaselines);
        std::cerr << e.what() << std::endl;
        // std::cout << e.where() << std::endl;
      }
      // print(std::cout, xmlDocBaselines, 0);
      // xml_node<> *nodeBaselines{xmlDocBaselines.first_node("baselines")};
      xml_node<> *nodeBaselines = xmlDocBaselines.first_node("baselines");

      readBaselines(nodeBaselines, pmodel, filePath, dx, dy, dz, sfactor, rangle, pmessage);

    }

    // Airfoil type cross-section
    else if (cs_type == "airfoil") {

      xml_attribute<> *baselineType{p_xn_geometry_include->first_attribute("type")};

      // Based on topology library
      if (baselineType) {
        std::string baselineTypeString{baselineType->value()};
        if (baselineTypeString != "airfoil"){
            std::cout << markError << " Undefined baseline type: " << baselineTypeString
                      << std::endl;
        }

        // Read topology information
        // std::cout << "\nreading topology...\n";
        // Box: two straight webs
        // I: one straight webs
        xml_node<> *nodeTopology{p_xn_sg->first_node("topology")};
        std::vector<std::pair<double, double>> websArray;
        if (nodeTopology) {
          xml_node<> *nodeLEWeb{nodeTopology->first_node("leading_web")};
          xml_node<> *nodeTEWeb{nodeTopology->first_node("trailling_web")};
          xml_node<> *nodeMidWeb{nodeTopology->first_node("mid_web")};

          xml_attribute<> *p_xa_fillLE = nodeLEWeb->first_attribute("fill");
          xml_attribute<> *p_xa_curvedLE = nodeLEWeb->first_attribute("curved");
          xml_attribute<> *p_xa_fillTE = nodeTEWeb->first_attribute("fill");
          xml_attribute<> *p_xa_curvedTE = nodeTEWeb->first_attribute("curved");

          bool fillLE = p_xa_fillLE ? (strcmp(p_xa_fillLE->value(), "true") == 0) : true;
          bool curvedLE = p_xa_curvedLE ? (strcmp(p_xa_curvedLE->value(), "true") == 0) : false;
          bool fillTE = p_xa_fillTE ? (strcmp(p_xa_fillTE->value(), "true") == 0) : true;
          bool curvedTE = p_xa_curvedTE ? (strcmp(p_xa_curvedTE->value(), "true") == 0) : false;

          double xtLE, xbLE, xtTE, xbTE;
          xtLE = atof(nodeLEWeb->first_node("pos_top")->value());
          xbLE = atof(nodeLEWeb->first_node("pos_bot")->value());
          xtTE = atof(nodeTEWeb->first_node("pos_top")->value());
          xbTE = atof(nodeTEWeb->first_node("pos_bot")->value());

          const double PI = atan(1) * 4;
          double ytLE,ybLE,xmLE,angLE;
          ytLE = getWebEnd(p_xn_geometry_include, filePath, xtLE);
          ybLE = getWebEnd(p_xn_geometry_include, filePath, xbLE, false);
          xmLE = (0 - ybLE) / (ytLE - ybLE) * (xtLE - xbLE) + xbLE;
          angLE = atan2(ytLE - ybLE, xtLE - xbLE) * 180 / PI;
          PDCELVertex *pvLEmid = new PDCELVertex{"Pweb_le", 0, xmLE , 0};
          addBaselineByPointAndAngle(pmodel, "web_le", pvLEmid, angLE);
          websArray.push_back(std::make_pair(xmLE, angLE));

          double ytTE,ybTE,xmTE,angTE;
          ytTE = getWebEnd(p_xn_geometry_include, filePath, xtTE);
          ybTE = getWebEnd(p_xn_geometry_include, filePath, xbTE, false);
          xmTE = (0 - ybTE) / (ytTE - ybTE) * (xtTE - xbTE) + xbTE;
          angTE = atan2(ytTE - ybTE, xtTE - xbTE) * 180 / PI;
          PDCELVertex *pvTEmid = new PDCELVertex{"Pweb_te", 0, xmTE , 0};
          addBaselineByPointAndAngle(pmodel, "web_te", pvTEmid, angTE);
          websArray.push_back(std::make_pair(xmTE, angTE));        

          if (nodeMidWeb) {
            double xtM, xbM;
            xtM = atof(nodeMidWeb->first_node("pos_top")->value());
            xbM = atof(nodeMidWeb->first_node("pos_bot")->value());

            double ytM,ybM,xmM,angM;
            ytM = getWebEnd(p_xn_geometry_include, filePath, xtM);
            ybM = getWebEnd(p_xn_geometry_include, filePath, xbM, false);
            xmM = (0 - ybM) / (ytM - ybM) * (xtM - xbM) + xbM;
            angM = atan2(ytM - ybM, xtM - xbM) * 180 / PI;
            PDCELVertex *pvMmid = new PDCELVertex{"Pweb_M", 0, xmM , 0};
            addBaselineByPointAndAngle(pmodel, "web_M", pvMmid, angM);
            websArray.push_back(std::make_pair(xmM, angM));   
          }
        }
        xml_node<> *nodeSpar{p_xn_sg->first_node("spar")};
        if (nodeSpar) {
          xml_attribute<> *p_xa_spar = nodeSpar->first_attribute("type");
          std::string sparType{p_xa_spar->value()};
          std::cout << "\nSpar type: " << sparType << std::endl;

          if (sparType == "box") {
            std::stringstream ss(nodeSpar->value());
            double x1, angle1, x2, angle2;
            ss >> x1 >> angle1 >> x2 >> angle2;
            PDCELVertex *pvMid1 = new PDCELVertex{"Pweb_le", 0, x1 , 0};
            PDCELVertex *pvMid2 = new PDCELVertex{"Pweb_te", 0, x2 , 0};
            addBaselineByPointAndAngle(pmodel, "web_le", pvMid1, angle1);
            addBaselineByPointAndAngle(pmodel, "web_te", pvMid2, angle2);
            websArray.push_back(std::make_pair(x1, angle1));
            websArray.push_back(std::make_pair(x2, angle2));
          }
          else if (sparType == "I") {
            std::stringstream ss(nodeSpar->value());
            double x, angle;
            ss >> x >> angle;
            PDCELVertex *pvMid = new PDCELVertex{"Pweb", 0, x , 0};
            addBaselineByPointAndAngle(pmodel, "web", pvMid, angle);
            websArray.push_back(std::make_pair(x, angle));
          }
          else {
            std::cout << markError << " Undefined spar type: " << sparType
                      << std::endl;
          }
        }
        addBaselinesFromAirfoil(p_xn_geometry_include, pmodel, filePath, websArray, dx, dy, dz, sfactor, rangle, pmessage);

      }


    }

  }


  // 2: Try to read geometry in the main input file
  if (p_xn_geometry_main) {

    readBaselines(p_xn_geometry_main, pmodel, filePath, dx, dy, dz, sfactor, rangle, pmessage);

  }

  if (config.debug) {
    // pmessage->printBlank();
    // pmessage->print(9, "summary of base lines");
    PLOG(debug) << pmessage->message(" ");
    PLOG(debug) << pmessage->message("summary of base lines (before transformation)");
    for (auto bsl : pmodel->baselines()) {
      bsl->print(pmessage);
      // pmessage->printBlank();
      PLOG(debug) << pmessage->message(" ");
    }
  }

  // translate; scale; rotate
  // std::cout << "\ntransforming...\n";
  PLOG(debug) << pmessage->message("transforming base geometry");

  for (auto v : pmodel->vertices()) {
    v->translate(dx, dy, dz);
    v->scale(sfactor);
    v->rotate(rangle);
  }

  if (config.debug) {
    // pmessage->printBlank();
    // pmessage->print(9, "summary of base lines");
    PLOG(debug) << pmessage->message(" ");
    PLOG(debug) << pmessage->message("summary of base lines (after transformation)");
    for (auto bsl : pmodel->baselines()) {
      bsl->print(pmessage);
      // pmessage->printBlank();
      PLOG(debug) << pmessage->message(" ");
    }
  }

  PLOG(debug) << pmessage->message("finished reading geometry.");
}









void read_materials(
  const xml_node<> *p_xn_material_include, const xml_node<> *p_xn_material_main,
  const std::string &filePath,
  PModel *pmodel, Message *pmessage) {

  PLOG(debug) << pmessage->message("reading materials...");

  // Global include file
  std::string fn_material_global = "";
#ifdef __linux__
  char buffer2[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", buffer2, sizeof(buffer2) - 1);
  if (count != -1) {
    buffer2[count] = '\0';
    // s_fullpath = buffer;
    fn_material_global = dirname(buffer2);
    fn_material_global = fn_material_global + "/";
  }
#elif _WIN32
  char buffer2[MAX_PATH];
  GetModuleFileName(NULL, buffer2, sizeof(buffer2));
  std::string s_fullpath{buffer2};
  // s_fullpath = buffer;
  // std::cout << s_fullpath << std::endl;
  std::vector<std::string> vs;
  vs = gmshSplitFileName(s_fullpath);
  fn_material_global = vs[0];
#endif
  fn_material_global = fn_material_global + "MaterialDB.xml";

  if (fn_material_global != "") {
    try {
      readMaterialsFile(fn_material_global, pmodel, pmessage);
    } catch (std::exception &e) {
      PLOG(error) << pmessage->message("error while reading global material file: " + std::string(e.what()));
    }
  }

  // Local include file
  if (p_xn_material_include) {
    std::string fn_material_local = "";
    fn_material_local = filePath + p_xn_material_include->value() + ".xml";
    if (fn_material_local != "") {
      try {
        readMaterialsFile(fn_material_local, pmodel, pmessage);
      } catch (std::exception &e) {
        PLOG(error) << pmessage->message("error while reading local material file: " + std::string(e.what()));
      }
    }
  }

  // Main input file
  if (p_xn_material_main) {
    try {
      readMaterials(p_xn_material_main, pmodel, pmessage);
    } catch (std::exception &e) {
      PLOG(error) << pmessage->message("error while reading material from main input file: " + std::string(e.what()));
    }
  }

  PLOG(debug) << pmessage->message("finished reading materials.");
}









void read_layups(
  const xml_node<> *p_xn_layup_include, const xml_node<> *p_xn_layup_main,
  const std::string &filePath,
  PModel *pmodel, Message *pmessage
) {
  PLOG(debug) << pmessage->message("reading layups...");

  if (p_xn_layup_include) {
    std::string filenameLayups{p_xn_layup_include->value()};
    filenameLayups = filePath + filenameLayups + ".xml";

    if (debug) {
      std::cout << markInfo << " Include Layups File: " << filenameLayups
                << std::endl;
    }

    std::ifstream fileLayups;
    if (!openFile(fileLayups, filenameLayups)) {
      PLOG(error) << pmessage->message("cannot open file: " + filenameLayups);
      return;
    }

    xml_document<> xmlDocLayups;
    std::vector<char> buffer_lyp{(std::istreambuf_iterator<char>(fileLayups)),
                            std::istreambuf_iterator<char>()};
    buffer_lyp.push_back('\0');

    try {
      xmlDocLayups.parse<0>(&buffer_lyp[0]);
    } catch (const parse_error &e) {
      PLOG(error) << pmessage->message("error while parsing file: " + filenameLayups + ": " + std::string(e.what()));
      return;
    }

    xml_node<> *nodeLayups = xmlDocLayups.first_node("layups");
    if (!nodeLayups) {
      PLOG(error) << pmessage->message("error while parsing file: " + filenameLayups + ": could not find node 'layups'");
      return;
    }

    readLayups(nodeLayups, pmodel, pmessage);
  }

  if (p_xn_layup_main) {
    readLayups(p_xn_layup_main, pmodel, pmessage);
  }
  
  PLOG(debug) << pmessage->message("finished reading layups.");
}









void read_components(
  const xml_node<> *p_xn_sg, CrossSection *cs, PModel *pmodel, Message *pmessage
) {
  PLOG(debug) << pmessage->message("reading components...");

  std::vector<Layup *> p_layups{};  // All layups used in this cross section
  int num_combined_layups = 0;  // Number of combined layups

  std::vector<std::vector<std::string>> tmp_dependents_all;

  for (auto xn_component = p_xn_sg->first_node("component");
       xn_component; xn_component = xn_component->next_sibling("component")) {
    PComponent *p_component;

    p_component = readXMLElementComponent(
      xn_component, tmp_dependents_all, p_layups, num_combined_layups, cs, pmodel, pmessage
    );

    cs->addComponent(p_component);
  }
  
  
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
  double meshsize = pmodel->globalMeshSize();
  if (meshsize == 0.0) {
    if (debug)
      std::cout << "\n"
                << markInfo << " Use Default Mesh Size"
                << "\n\n";
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
    pmodel->setGlobalMeshSize(meshsize);
  }

  PLOG(debug) << pmessage->message("finished reading components.");
}









int readCrossSection(const std::string &filenameCrossSection,
                     const std::string &filePath, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();

  // int material_id = 1;  // Material ID
  // int layertype_id = 1; // Layer Type ID

  xml_document<> xmlDocCrossSection;
  std::ifstream fileCrossSection{filenameCrossSection};
  if (!fileCrossSection.is_open()) {
    PLOG(error) << pmessage->message("unable to open file: " + filenameCrossSection);
    return 1;
  } else {
    PLOG(info) << pmessage->message("reading main input file: " + filenameCrossSection);
  }

  std::vector<char> buffer{(std::istreambuf_iterator<char>(fileCrossSection)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xmlDocCrossSection.parse<0>(&buffer[0]);
  } catch (parse_error &e) {
    std::cout << markError
              << " Unable to parse the file: " << filenameCrossSection
              << std::endl;
    std::cerr << e.what() << std::endl;
  }


  xml_node<> *p_xn_sg{xmlDocCrossSection.first_node("cross_section")};
  if (!p_xn_sg) {
    p_xn_sg = xmlDocCrossSection.first_node("sg");
  }


  std::string csName{p_xn_sg->first_attribute("name")->value()};
  std::string cs_type{"general"};
  if (p_xn_sg->first_attribute("type")) {
    cs_type = p_xn_sg->first_attribute("type")->value();
  }

  PLOG(debug) << pmessage->message("found cross-section: " + csName);


  double d_fmt{1};
  xml_attribute<> *p_xa_fmt{p_xn_sg->first_attribute("format")};
  if (p_xa_fmt) {
    std::string ss{p_xa_fmt->value()};
    if (ss[0] != '\0') {
      d_fmt = atof(ss.c_str());
    }
  }


  // -----------------------------------------------------------------
  // Read settings
  xml_node<> *p_xn_settings{p_xn_sg->first_node("settings")};
  if (p_xn_settings) {
    PLOG(debug) << pmessage->message("reading settings...");

    xml_node<> *p_xn_tolerance{p_xn_settings->first_node("tolerance")};
    if (p_xn_tolerance && p_xn_tolerance->value()) {
      std::string ss{p_xn_tolerance->value()};
      try {
        config.geo_tol = std::stof(ss);
      } catch (const std::invalid_argument& ia) {
        std::cerr << markError << "Invalid argument for tolerance: " << ss << std::endl;
      } catch (const std::out_of_range& oor) {
        std::cerr << markError << "Tolerance out of range: " << ss << std::endl;
      }
    }
  }


  // -----------------------------------------------------------------
  // Read analysis
  xml_node<> *p_xn_analysis{p_xn_sg->first_node("analysis")};
  if (p_xn_analysis) {
    read_analysis(p_xn_analysis, pmodel, pmessage);
  }


  // -----------------------------------------------------------------
  // Read general
  xml_node<> *p_xn_general{p_xn_sg->first_node("general")};
  if (p_xn_general) {
    read_general(p_xn_general, pmodel, pmessage);
  }


  // -----------------------------------------------------------------
  // Read include
  PLOG(debug) << pmessage->message("finding includings...");
  xml_node<> *nodeInclude{p_xn_sg->first_node("include")};

  
  // -----------------------------------------------------------------
  // Read geometry (base points and base lines)
  
  xml_node<> *p_xn_geo_include = nullptr;
  if (nodeInclude) {
    p_xn_geo_include = nodeInclude->first_node("baseline");
  }
  xml_node<> *p_xn_geo_main = p_xn_sg->first_node("baselines");
  read_geometry(
    p_xn_geo_include, p_xn_geo_main, cs_type, filePath, p_xn_sg, pmodel, pmessage);


  // -----------------------------------------------------------------
  // Read materials (global)
  xml_node<> *p_xn_material_include = nullptr;
  if (nodeInclude) {
    p_xn_material_include = nodeInclude->first_node("material");
  }
  xml_node<> *p_xn_material_main = p_xn_sg->first_node("materials");

  read_materials(p_xn_material_include, p_xn_material_main, filePath, pmodel, pmessage);


  // -----------------------------------------------------------------
  // Read layups
  xml_node<> *p_xn_layup_include = nullptr;
  if (nodeInclude) {
    p_xn_layup_include = nodeInclude->first_node("layup");
  }
  xml_node<> *p_xn_layup_main = p_xn_sg->first_node("layups");

  read_layups(p_xn_layup_include, p_xn_layup_main, filePath, pmodel, pmessage);


  // -----------------------------------------------------------------
  // Read components  
  CrossSection *cs = new CrossSection(csName, pmodel);

  read_components(p_xn_sg, cs, pmodel, pmessage);



  pmodel->setCrossSection(cs);

  pmessage->decreaseIndent();

  return 0;
}
