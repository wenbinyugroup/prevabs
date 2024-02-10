#include "PModelIO.hpp"

#include "PDCELVertex.hpp"
#include "PBaseLine.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <string>
#include <vector>
#include <stack>
#include <sstream>
#include <iostream>


int readXMLElementComponentFilling(
  PComponent *p_component, const xml_node<> *xn_component,
  CrossSection *cs, PModel *pmodel, Message *pmessage
  ) {

  // Read material
  std::string fmn = xn_component->first_node("material")->value();
  Material *p_mtr_tmp = pmodel->getMaterialByName(fmn);
  if (p_mtr_tmp == nullptr) {
    std::cout << "[error] cannot find material: " << fmn << std::endl;
  }
  // p_mtr_tmp->printMaterial();
  p_component->setFillMaterial(p_mtr_tmp);

  // Read \theta_1
  double theta1 = 0.0;
  xml_node<> *p_xn_theta1 = xn_component->first_node("theta1");
  if (p_xn_theta1) {
    std::string stheta1{p_xn_theta1->value()};
    if (stheta1[0] != '\0') {
      theta1 = atof(stheta1.c_str());
      // p_component->setFillTheta1(theta1);
    }
  }
  p_component->setFillTheta1(theta1);

  // Read \theta_3
  double theta3 = 0.0;
  xml_node<> *p_xn_theta3 = xn_component->first_node("theta3");
  if (p_xn_theta3) {
    std::string stheta3{p_xn_theta3->value()};
    if (stheta3[0] != '\0') {
      theta3 = atof(stheta3.c_str());
      // p_component->setFillTheta3(theta3);
    }
  }
  p_component->setFillTheta3(theta3);


  LayerType *p_lt_tmp = pmodel->getLayerTypeByMaterialNameAngle(fmn, theta3);
  if (p_lt_tmp == nullptr) {
    p_lt_tmp = new LayerType{0, pmodel->getMaterialByName(fmn), theta3};
    pmodel->addLayerType(p_lt_tmp);
  }
  if (p_lt_tmp->id() == 0) {
    // Created but never used layer type
    CrossSection::used_layertype_index++;
    p_lt_tmp->setId(CrossSection::used_layertype_index);
    cs->addUsedLayerType(p_lt_tmp);
  }
  // Check used material
  if (p_mtr_tmp->id() == 0) {
    CrossSection::used_material_index++;
    p_mtr_tmp->setId(CrossSection::used_material_index);
    cs->addUsedMaterial(p_mtr_tmp);
  }


  //
  if (xn_component->first_node("location")) {
    p_component->setFillLocation(pmodel->getPointByName(
        xn_component->first_node("location")->value()));
  }

  for (auto p_xn_baseline = xn_component->first_node("baseline");
        p_xn_baseline;
        p_xn_baseline = p_xn_baseline->next_sibling("baseline")) {
    std::list<Baseline *> blg;

    std::vector<std::string> bl_names =
        splitString(p_xn_baseline->value(), ',');
    for (auto name : bl_names) {
      Baseline *bl = pmodel->getBaselineByNameCopy(name);
      blg.push_back(bl);
    }
    if (p_xn_baseline->first_attribute("fillside")) {
      p_component->setFillRefBaseline(blg.front());
      std::string fs = p_xn_baseline->first_attribute("fillside")->value();
      if (fs == "left") {
        p_component->setFillSide(1);
      } else if (fs == "right") {
        p_component->setFillSide(-1);
      }
    }

    p_component->addFillBaselineGroup(blg);
  }

  // Set local mesh size
  xml_node<> *p_xn_meshsize = xn_component->first_node("mesh_size");
  if (p_xn_meshsize) {
    std::string smeshsize{p_xn_meshsize->value()};
    if (smeshsize[0] != '\0')
      p_component->setMeshSize(atof(smeshsize.c_str()));

    std::vector<std::string> point_names;
    point_names =
        splitString(p_xn_meshsize->first_attribute("at")->value(), ',');
    for (auto n : point_names) {
      p_component->addEmbeddedVertex(pmodel->getPointByName(n));
    }

  }

  return 0;

}
