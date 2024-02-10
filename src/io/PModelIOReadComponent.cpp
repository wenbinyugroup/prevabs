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

using namespace rapidxml;


PComponent *readXMLElementComponent(
  const xml_node<> *xn_component, std::vector<std::vector<std::string>> &dependents_all,
  std::vector<Layup *> &p_layups, int &num_combined_layups,
  CrossSection *cs, PModel *pmodel, Message *pmessage
  ) {

  PComponent *p_component = new PComponent(pmodel);

  std::string cmp_name;
  xml_attribute<> *p_xa_name = xn_component->first_attribute("name");
  if (p_xa_name) {
    cmp_name = p_xa_name->value();
  } else {
    PComponent::count_tmp++;
    cmp_name = "cmp_" + std::to_string(PComponent::count_tmp);
  }
  // std::cout << "[debug] reading component: " << cmp_name << std::endl;
  PLOG(debug) << pmessage->message("reading component: " + cmp_name);
  p_component->setName(cmp_name);

  std::string s_cmp_type;
  int cmp_type = 1;
  xml_attribute<> *p_xa_type = xn_component->first_attribute("type");
  if (p_xa_type) {
    s_cmp_type = p_xa_type->value();
    if (s_cmp_type == "laminate") {
      cmp_type = 1;
    }
    else if (s_cmp_type == "fill") {
      cmp_type = 2;
    }
  }
  // std::cout << "[debug] cmp_type = " << cmp_type << std::endl;
  p_component->setType(cmp_type);

  int cmp_style = 1;
  xml_attribute<> *p_xa_style = xn_component->first_attribute("style");
  if (p_xa_style) {
    std::string ss{p_xa_style->value()};
    if (ss[0] != '\0') {
      cmp_style = atoi(ss.c_str());
    }
  }
  p_component->setStyle(cmp_style);

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








  // Laminate type component
  if (cmp_type == 1) {

    readXMLElementComponentLaminate(
      p_component, xn_component, dependents_all, depend_names,
      p_layups, num_combined_layups,
      cs, pmodel, pmessage
    );

  }

  // Fill type component
  else if (cmp_type == 2) {

    readXMLElementComponentFilling(
      p_component, xn_component, cs, pmodel, pmessage
    );

  }

  return p_component;

}
