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

PComponent *readXMLElementComponent(
  const rapidxml::xml_node<> *xn_component,
  std::vector<std::vector<std::string>> &dependents_all,
  std::vector<Layup *> &p_layups, int &num_combined_layups,
  CrossSection *cs, PModel *pmodel
  ) {


  PComponent *p_component = new PComponent();

  std::string cmp_name;
  rapidxml::xml_attribute<> *p_xa_name = xn_component->first_attribute("name");
  if (p_xa_name) {
    cmp_name = p_xa_name->value();
  } else {
    PComponent::count_tmp++;
    cmp_name = "cmp_" + std::to_string(PComponent::count_tmp);
  }
  // std::cout << "[debug] reading component: " << cmp_name << std::endl;
    PLOG(debug) << "reading component: " + cmp_name;
  p_component->setName(cmp_name);

  std::string s_cmp_type;
  ComponentType cmp_type = ComponentType::laminate;
  rapidxml::xml_attribute<> *p_xa_type = xn_component->first_attribute("type");
  if (p_xa_type) {
    s_cmp_type = p_xa_type->value();
    if (s_cmp_type == "laminate") {
      cmp_type = ComponentType::laminate;
    }
    else if (s_cmp_type == "fill") {
      cmp_type = ComponentType::fill;
    }
  }
  // std::cout << "[debug] cmp_type = " << cmp_type << std::endl;
  p_component->setType(cmp_type);

  JointStyle cmp_style = JointStyle::step;
  rapidxml::xml_attribute<> *p_xa_style = xn_component->first_attribute("style");
  if (p_xa_style) {
    std::string ss{p_xa_style->value()};
    if (ss[0] != '\0') {
      cmp_style =
          (atoi(ss.c_str()) == static_cast<int>(JointStyle::smooth))
              ? JointStyle::smooth
              : JointStyle::step;
    }
  }
  p_component->setStyle(cmp_style);

  rapidxml::xml_attribute<> *p_xa_cycle = xn_component->first_attribute("cycle");
  if (p_xa_cycle == nullptr) {
    p_xa_cycle = xn_component->first_attribute("cyclic");
  }
  if (p_xa_cycle) {
    p_component->setCycle(
      parseXmlBoolValue(p_xa_cycle->value(), "<component>@cycle/@cyclic")
    );
  }

  // Record the dependency relations between components (names only)
  std::vector<std::string> tmp_dependents_one;
  tmp_dependents_one.push_back(cmp_name);

  std::vector<std::string> depend_names;
  if (xn_component->first_attribute("depend")) {
    depend_names =
        splitString(xn_component->first_attribute("depend")->value(), ',');
    for (std::string n : depend_names) {
      tmp_dependents_one.push_back(n);
    }
  }
  dependents_all.push_back(tmp_dependents_one);

  // Read extra material orientation setting
  rapidxml::xml_node<> *p_xn_orient = xn_component->first_node("material_orientation");
  if (p_xn_orient) {
    rapidxml::xml_node<> *p_xn_moe1 = p_xn_orient->first_node("e1");
    if (p_xn_moe1) {
      p_component->setMatOrient1(p_xn_moe1->value());
    }
    rapidxml::xml_node<> *p_xn_moe2 = p_xn_orient->first_node("e2");
    if (p_xn_moe2) {
      p_component->setMatOrient2(p_xn_moe2->value());
    }
  }








  // Laminate type component
  if (cmp_type == ComponentType::laminate) {

    readXMLElementComponentLaminate(
      p_component, xn_component, dependents_all, depend_names,
      p_layups, num_combined_layups,
      cs, pmodel
    );

  }

  // Fill type component
  else if (cmp_type == ComponentType::fill) {

    readXMLElementComponentFilling(
      p_component, xn_component, cs, pmodel
    );

  }

  return p_component;

}