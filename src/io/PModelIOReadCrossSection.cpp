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

int readCrossSection(const std::string &filenameCrossSection,
                     const std::string &filePath, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();
  // i_indent++;

  int material_id = 1;  // Material ID
  int layertype_id = 1; // Layer Type ID

  xml_document<> xmlDocCrossSection;
  std::ifstream fileCrossSection{filenameCrossSection};
  if (!fileCrossSection.is_open()) {
    // std::cout << markError << " Unable to open file: " << filenameCrossSection
    //           << std::endl;
    PLOG(error) << pmessage->message("unable to open file: " + filenameCrossSection);
    return 1;
  } else {
    // printInfo(i_indent, "reading main input file: " + filenameCrossSection);
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
    // std::cout << e.where() << std::endl;
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
  // std::cout << "cs_type = " << cs_type << std::endl;

  // if (debug) {
  //   std::cout << markInfo << " Find Cross Section: " << csName << std::endl;
  // }
  PLOG(debug) << pmessage->message("find cross-section: " + csName);

  // CrossSection cs{csName};
  CrossSection *cs = new CrossSection(csName, pmodel);


  double d_fmt{1};
  xml_attribute<> *p_xa_fmt{p_xn_sg->first_attribute("format")};
  if (p_xa_fmt) {
    std::string ss{p_xa_fmt->value()};
    if (ss[0] != '\0') {
      d_fmt = atof(ss.c_str());
    }
  }
  // std::cout << "format = " << d_fmt << std::endl;









  // -----------------------------------------------------------------
  // Read settings
  xml_node<> *p_xn_settings{p_xn_sg->first_node("settings")};
  if (p_xn_settings) {
    PLOG(debug) << pmessage->message("reading settings...");

    xml_node<> *p_xn_tolerance{p_xn_settings->first_node("tolerance")};
    if (p_xn_tolerance) {
      std::string ss{p_xn_tolerance->value()};
      if (ss[0] != '\0') {
        config.geo_tol = atof(ss.c_str());
      }
    }
  }










  // -----------------------------------------------------------------
  // Read analysis
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

  xml_node<> *p_xn_analysis{p_xn_sg->first_node("analysis")};
  if (p_xn_analysis) {

    xml_node<> *p_xn_model_dim{p_xn_analysis->first_node("model_dim")};
    if (p_xn_model_dim) {
      std::string ss{p_xn_model_dim->value()};
      if (ss[0] != '\0') {
        model_dim = atoi(ss.c_str());
      }
    }

    xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
    if (p_xn_model) {
      std::string ss{p_xn_model->value()};
      if (ss[0] != '\0') {
        model = atoi(ss.c_str());
      }
    }

    xml_node<> *p_xn_physics{p_xn_analysis->first_node("physics")};
    if (p_xn_physics) {
      std::string ss{p_xn_physics->value()};
      int _physics = atoi(ss.c_str());
      pmodel->setAnalysisPhysics(_physics);
    }

    xml_node<> *p_xn_damping{p_xn_analysis->first_node("damping")};
    if (p_xn_damping) {
      std::string ss{p_xn_damping->value()};
      if (ss[0] != '\0') {
        flag_damping = atoi(ss.c_str());
      }
    }

    xml_node<> *p_xn_thermal{p_xn_analysis->first_node("thermal")};
    if (p_xn_thermal) {
      std::string ss{p_xn_thermal->value()};
      if (ss[0] != '\0') {
        flag_thermal = atoi(ss.c_str());
      }
    }

    xml_node<> *p_xn_k1{p_xn_analysis->first_node("initial_twist")};
    if (p_xn_k1) {
      std::string ss{p_xn_k1->value()};
      if (ss[0] != '\0') {
        k1 = atof(ss.c_str());
      }
    }

    xml_node<> *p_xn_k2{p_xn_analysis->first_node("initial_curvature_2")};
    if (p_xn_k2) {
      std::string ss{p_xn_k2->value()};
      if (ss[0] != '\0') {
        k2 = atof(ss.c_str());
      }
    }

    xml_node<> *p_xn_k3{p_xn_analysis->first_node("initial_curvature_3")};
    if (p_xn_k3) {
      std::string ss{p_xn_k3->value()};
      if (ss[0] != '\0') {
        k3 = atof(ss.c_str());
      }
    }

    xml_node<> *p_xn_cos11{p_xn_analysis->first_node("oblique_y1")};
    if (p_xn_cos11) {
      std::string ss{p_xn_cos11->value()};
      if (ss[0] != '\0') {
        cos11 = atof(ss.c_str());
      }
    }

    xml_node<> *p_xn_cos21{p_xn_analysis->first_node("oblique_y2")};
    if (p_xn_cos21) {
      std::string ss{p_xn_cos21->value()};
      if (ss[0] != '\0') {
        cos21 = atof(ss.c_str());
      }
    }

    xml_node<> *p_xn_trapeze{p_xn_analysis->first_node("trapeze")};
    if (p_xn_trapeze) {
      std::string ss{p_xn_trapeze->value()};
      if (ss[0] != '\0') {
        flag_trapeze = atoi(ss.c_str());
      }
    }

    xml_node<> *p_xn_vlasov{p_xn_analysis->first_node("vlasov")};
    if (p_xn_vlasov) {
      std::string ss{p_xn_vlasov->value()};
      if (ss[0] != '\0') {
        flag_vlasov = atoi(ss.c_str());
      }
    }

  }

  if (k1 != 0.0 || k2 != 0.0 || k3 != 0.0) {
    flag_curvature = 1;
  }

  if (cos11 != 1.0 || cos21 != 0.0) {
    flag_oblique = 1;
  }

  PLOG(debug) << pmessage->message("finished reading analysis.");

  // cs->setModel(model);
  // cs->setFlagThermal(flag_thermal);
  // cs->setFlagCurvature(flag_curvature);
  // cs->setCurvatures(k1, k2, k3);
  // cs->setObliques(cos11, cos21);
  // cs->setFlagOblique(flag_oblique);
  // cs->setFlagTrapeze(flag_trapeze);
  // cs->setFlagVlasov(flag_vlasov);

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
  PLOG(debug) << pmessage->message("reading general...");

  // if (debug)
  //   std::cout << "\n"
  //             << markInfo << " Read Cross Section General"
  //             << "\n\n";
  xml_node<> *nodeGeneral{p_xn_sg->first_node("general")};

  // Point2 origin{0, 0};
  // SVector3 translate{0, 0, 0};
  // std::vector<double> translate{0, 0};
  double dx{0}, dy{0}, dz{0};
  xml_node<> *nodeTranslate{nodeGeneral->first_node("translate")};
  if (nodeTranslate) {
    std::stringstream ss{nodeTranslate->value()};
    if (ss.str()[0] != '\0') {
      ss >> dy >> dz;
      // origin.setPoint(-x, -y);
      // translate[1] = x;
      // translate[2] = y;
    }
  }
  // cs->setOrigin(origin);
  // cs->setTranslate(translate);

  double sfactor{1.0};
  xml_node<> *nodeScale{nodeGeneral->first_node("scale")};
  if (nodeScale) {
    std::string sscale{nodeScale->value()};
    if (sscale[0] != '\0')
      sfactor = atof(sscale.c_str());
  }
  // cs->setScale(scale);

  double rangle{0.0};
  xml_node<> *nodeRotate{nodeGeneral->first_node("rotate")};
  if (nodeRotate) {
    std::string srotate{nodeRotate->value()};
    if (srotate[0] != '\0')
      rangle = atof(srotate.c_str());
  }
  // Matrix2 rotatem{getRotationMatrix(rotate)};
  // cs->setRotate(rotate);
  // cs->setRotateMatrix(rotatem);

  // Global transformation of all points
  // 1. Translate
  // 2. Scale
  // 3. Rotate
  // for (auto v : pmodel->vertices()) {
  //   v->translate(dx, dy, dz);
  //   v->scale(sfactor);
  //   v->rotate(rangle);
  // }

  double meshsize{0.0};
  xml_node<> *nodeMeshsize{nodeGeneral->first_node("mesh_size")};
  if (nodeMeshsize) {
    std::string smeshsize{nodeMeshsize->value()};
    if (smeshsize[0] != '\0')
      meshsize = atof(smeshsize.c_str());
  }
  // else Use default mesh size, the smallest layer thickness
  // This is set after reading in all segments
  // cs->setMeshsize(meshsize);

  int elementtype = 2;
  xml_node<> *nodeElementType{nodeGeneral->first_node("element_type")};
  if (nodeElementType) {
    if (nodeElementType->value()[0] != '\0') {
      std::string et{nodeElementType->value()};
      if (et == "linear" || et == "1") {
        elementtype = 1;
      } else if (et == "quadratic" || et == "2") {
        elementtype = 2;
      }
    }
    // elementtype = nodeElementType->value();
  }
  // cs->setElementtype(elementtype);
  // pmessage->print(9, "elementtype = " + std::to_string(elementtype));
  pmodel->setElementType(elementtype);


  double omega{1.0};
  xml_node<> *p_xn_omega{nodeGeneral->first_node("omega")};
  if (p_xn_omega) {
    std::string stol{p_xn_omega->value()};
    if (stol[0] != '\0') {
      omega = atof(stol.c_str());
    }
  }
  pmodel->setOmega(omega);


  xml_node<> *p_xn_tol{nodeGeneral->first_node("tolerance")};
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
  p_xn_itf = nodeGeneral->first_node("track_interface");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      track_interface = atoi(ss.c_str());
      // std::cout << "track_interface = " << track_interface << std::endl;
      pmodel->setInterfaceOutput(track_interface);
      // std::cout << "_itf_output = " << pmodel->interfaceOutput() << std::endl;
    }
  }


  double itf_t1d_th;
  p_xn_itf = nodeGeneral->first_node("interface_theta1_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t1d_th = atoi(ss.c_str());
      pmodel->setInterfaceTheta1DiffThreshold(itf_t1d_th);
    }
  }


  double itf_t3d_th;
  p_xn_itf = nodeGeneral->first_node("interface_theta3_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') {
      itf_t3d_th = atoi(ss.c_str());
      pmodel->setInterfaceTheta3DiffThreshold(itf_t3d_th);
    }
  }


  PLOG(debug) << pmessage->message("finished reading general.");









  // -----------------------------------------------------------------
  // Read include
  PLOG(debug) << pmessage->message("finding includings...");
  xml_node<> *nodeInclude{p_xn_sg->first_node("include")};
  xml_node<> *nodeBaselines;





  // -----------------------------------------------------------------
  // Read geometry (base points and base lines)
  PLOG(debug) << pmessage->message("reading geometry...");

  // 1: Try to read geometry from a seperated file
  if (nodeInclude) {
    xml_node<> *p_xn_include_bsl{nodeInclude->first_node("baseline")};
    if (p_xn_include_bsl) {

      // General type cross-section
      if (cs_type == "general") {
        std::string filenameBaselines{p_xn_include_bsl->value()};
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
        nodeBaselines = xmlDocBaselines.first_node("baselines");

        readBaselines(nodeBaselines, pmodel, filePath, dx, dy, dz, sfactor, rangle, pmessage);

      }

      // Airfoil type cross-section
      else if (cs_type == "airfoil") {

        xml_attribute<> *baselineType{p_xn_include_bsl->first_attribute("type")};

        // Based on topology library
        if (baselineType) {
          std::string baselineTypeString{baselineType->value()};
          if (baselineTypeString != "airfoil"){
              std::cout << markError << " Undefined baseline type: " << baselineTypeString
                        << std::endl;
              return 1;
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
            ytLE = getWebEnd(p_xn_include_bsl, filePath, xtLE);
            ybLE = getWebEnd(p_xn_include_bsl, filePath, xbLE, false);
            xmLE = (0 - ybLE) / (ytLE - ybLE) * (xtLE - xbLE) + xbLE;
            angLE = atan2(ytLE - ybLE, xtLE - xbLE) * 180 / PI;
            PDCELVertex *pvLEmid = new PDCELVertex{"Pweb_le", 0, xmLE , 0};
            addBaselineByPointAndAngle(pmodel, "web_le", pvLEmid, angLE);
            websArray.push_back(std::make_pair(xmLE, angLE));

            double ytTE,ybTE,xmTE,angTE;
            ytTE = getWebEnd(p_xn_include_bsl, filePath, xtTE);
            ybTE = getWebEnd(p_xn_include_bsl, filePath, xbTE, false);
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
              ytM = getWebEnd(p_xn_include_bsl, filePath, xtM);
              ybM = getWebEnd(p_xn_include_bsl, filePath, xbM, false);
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
              return 1;
            }
          }
          addBaselinesFromAirfoil(p_xn_include_bsl, pmodel, filePath, websArray, dx, dy, dz, sfactor, rangle, pmessage);

        }


      }

    }
  }



  // 2: Try to read geometry in the main input file
  nodeBaselines = p_xn_sg->first_node("baselines");

  if (nodeBaselines) {

    readBaselines(nodeBaselines, pmodel, filePath, dx, dy, dz, sfactor, rangle, pmessage);

  }

  if (config.debug) {
    pmessage->printBlank();
    // pmessage->print(9, "summary of base lines");
    PLOG(debug) << pmessage->message("summary of base lines (before transformation)");
    for (auto bsl : pmodel->baselines()) {
      bsl->print(pmessage);
      pmessage->printBlank();
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
    pmessage->printBlank();
    // pmessage->print(9, "summary of base lines");
    PLOG(debug) << pmessage->message("summary of base lines (after transformation)");
    for (auto bsl : pmodel->baselines()) {
      bsl->print(pmessage);
      pmessage->printBlank();
    }
  }

  PLOG(debug) << pmessage->message("finished reading geometry.");









  // -----------------------------------------------------------------
  // Read materials (global)
  PLOG(debug) << pmessage->message("reading materials...");

  std::string fn_material_global = "";
  // xml_node<> *nodeMaterials{nodeInclude->first_node("material")};
  // if (nodeMaterials) {
  //   fn_material_global = filePath + nodeMaterials->value() + ".xml";
  // } else {
// Use defualt database
// std::string s_fullpath;
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
  // std::cout << fn_material_global << std::endl;
  // }
  // if (debug) {
  //   std::cout << markInfo << " Include Material Database: " << fn_material_global
  //             << std::endl;
  // }
  // if (debug)
  // std::cout << "- reading materials" << std::endl;
  // readMaterials(fn_material_global, pmodel);
  // if (debug) {
  // std::cout << "- reading materials -- done" << std::endl;
  // for (auto m : pmodel->materials())
  //   m->printMaterial();
  // }
  // for (auto m : pmodel->materials())
  //   m->printMaterial();

  std::string fn_material_local = "";
  if (nodeInclude) {
    xml_node<> *xn_material{nodeInclude->first_node("material")};
    if (xn_material) {
      fn_material_local = filePath + xn_material->value() + ".xml";
      // readMaterials(fn_material_local, pmodel);
    }
  }

  if (fn_material_global != "") {
    readMaterialsFile(fn_material_global, pmodel, pmessage);
  }

  if (fn_material_local != "") {
    readMaterialsFile(fn_material_local, pmodel, pmessage);
  }

  xml_node<> *p_xn_materials{p_xn_sg->first_node("materials")};
  if (p_xn_materials) {
    readMaterials(p_xn_materials, pmodel, pmessage);
  }

  if (config.debug) {
    pmessage->printBlank();
    pmessage->print(9, "summary of materials");
    for (auto mtr : pmodel->materials()) {
      mtr->print(pmessage, 9);
      pmessage->printBlank();
    }
  }

  PLOG(debug) << pmessage->message("finished reading materials.");









  // -----------------------------------------------------------------
  // Read layups
  PLOG(debug) << pmessage->message("reading layups...");

  xml_node<> *nodeLayups;
  if (d_fmt == 0) {
    if (nodeInclude) {
      xml_node<> *p_xn_include_lyp{nodeInclude->first_node("layup")};
      std::string filenameLayups{p_xn_include_lyp->value()};
      filenameLayups = filePath + filenameLayups + ".xml";
      if (debug) {
        std::cout << markInfo << " Include Layups File: " << filenameLayups
                  << std::endl;
      }
      std::ifstream fileLayups;
      openFile(fileLayups, filenameLayups);
      xml_document<> xmlDocLayups;
      std::vector<char> buffer_lyp{(std::istreambuf_iterator<char>(fileLayups)),
                              std::istreambuf_iterator<char>()};
      buffer_lyp.push_back('\0');

      try {
        xmlDocLayups.parse<0>(&buffer_lyp[0]);
      } catch (parse_error &e) {
        std::cout << markError << " Unable to parse the file: " << filenameLayups
                  << std::endl;
        std::cerr << e.what() << std::endl;
      }
      nodeLayups = xmlDocLayups.first_node("layups");
      readLayups(nodeLayups, pmodel, pmessage);
      PLOG(debug) << pmessage->message("finished reading layups.");
    }
  }

  else if (d_fmt == 1) {
    nodeLayups = p_xn_sg->first_node("layups");
    if (nodeLayups) {
      readLayups(nodeLayups, pmodel, pmessage);
      PLOG(debug) << pmessage->message("finished reading layups.");
    }
  }

  // readLayups(filenameLayups, pmodel, pmessage);
  // if (config.debug) {
  //   pmessage->printBlank();
  //   pmessage->print(9, "summary of layups");
  //   for (auto lyp : pmodel->layups()) {
  //     lyp->print(pmessage, 9);
  //     pmessage->printBlank();
  //   }
  // }









  // -----------------------------------------------------------------
  // Read components
  // if (debug)
  // std::cout << "- reading components" << std::endl;
  PLOG(debug) << pmessage->message("reading components...");

  std::vector<Layup *> p_layups{};  // All layups used in this cross section
  int num_combined_layups = 0;  // Number of combined layups

  std::vector<std::vector<std::string>> tmp_dependents_all;

  for (auto xn_component = p_xn_sg->first_node("component");
       xn_component; xn_component = xn_component->next_sibling("component")) {
    // xml_node<> *nodeSegments{p_xn_sg->first_node("segments")};
    // for (auto nodeSegment = nodeSegments->first_node("segment"); nodeSegment;
    PComponent *p_component;

    p_component = readXMLElementComponent(
      xn_component, tmp_dependents_all, p_layups, num_combined_layups, cs, pmodel, pmessage
    );

    cs->addComponent(p_component);
  }
  PLOG(debug) << pmessage->message("finished reading components.");





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

  // if (config.debug) {
  //   pmessage->printBlank();
  //   pmessage->print(9, "summary of components");
  //   for (auto cmp : cs->components()) {
  //     cmp->print(pmessage, 9);
  //     pmessage->printBlank();
  //   }
  // }

  // if (debug)
  // std::cout << "- reading components -- done" << std::endl;

  // Set default mesh size as the smallest layer thickness
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
  }
  // cs->setMeshsize(meshsize);
  pmodel->setGlobalMeshSize(meshsize);

  // pmodel->crosssections.push_back(cs);
  pmodel->setCrossSection(cs);

  // if (config.debug) {
  //   pmodel->summary(pmessage);
  // }

  // i_indent--;
  pmessage->decreaseIndent();

  return 0;
}
