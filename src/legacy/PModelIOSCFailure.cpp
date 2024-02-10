#include "PModel.hpp"

#include "PBaseLine.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"

#include "rapidxml/rapidxml.hpp"
#include "gmsh/StringUtils.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rapidxml;


void PModel::failureSC() {
  i_indent++;

  // -----------------------------------------------------------------
  // Read *.xml
  // Append global analysis results
  xml_document<> xd_cs;
  std::ifstream ifs_cs{config.main_input};
  if (!ifs_cs.is_open()) {
    std::cout << markError << " Unable to open file: " << config.main_input
              << std::endl;
    return;
  } else {
    printInfo(i_indent, "reading main input data: " + config.main_input);
  }

  std::vector<char> buffer{(std::istreambuf_iterator<char>(ifs_cs)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xd_cs.parse<0>(&buffer[0]);
  } catch (parse_error &e) {
    std::cout << markError << " Unable to parse the file: " << config.main_input
              << std::endl;
    std::cerr << e.what() << std::endl;
  }

  // ------------------------------
  xml_node<> *p_xn_cs{xd_cs.first_node("cross_section")};
  std::string s_file_name_glb;
  s_file_name_glb = config.file_name_vsc + ".glb";

  // ------------------------------
  xml_node<> *p_xn_analysis{p_xn_cs->first_node("analysis")};
  int model = 0;
  if (p_xn_analysis) {
    xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
    if (p_xn_model) {
      model = atoi(p_xn_model->value());
    }
  }

  // ------------------------------
  xml_node<> *p_xn_recover{p_xn_cs->first_node("global")};
  std::string s_output;
  std::string s_axis1, s_axis2;
  int i_div;
  std::vector<double> v_d_loads;
  if (p_xn_recover) {
    s_output = p_xn_recover->first_attribute("measure")->value();
    if (config.fail_envelope) {
      s_axis1 = p_xn_recover->first_node("axis1")->value();
      s_axis2 = p_xn_recover->first_node("axis2")->value();
      i_div = atoi(p_xn_recover->first_node("divisions")->value());
    }
    if (config.fail_index) {
      xml_node<> *p_xn_temp;
      p_xn_temp = p_xn_recover->first_node("loads");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        v_d_loads = parseNumbersFromString(p_xn_temp->value());
      }
    }
  }

  // -----------------------------------------------------------------
  // Write new *.sg.glb file
  printInfo(i_indent, "writing SwiftComp .glb file");
  std::ofstream ofs;
  ofs.open(s_file_name_glb);

  ofs << std::scientific;

  for (auto pm : _cross_section->getUsedMaterials()) {
    ofs << std::setw(8) << pm->getFailureCriterion()
        << std::setw(8) << pm->getStrength().size() << std::endl;
    ofs << std::setw(16) << pm->getCharacteristicLength() << std::endl;
    for (auto s : pm->getStrength()) {
      ofs << std::setw(16) << s;
    }
    ofs << std::endl;
    ofs << std::endl;
  }

  if (s_output == "stress")
    ofs << std::setw(8) << 0 << std::endl;
  else if (s_output == "strain")
    ofs << std::setw(8) << 1 << std::endl;
  
  ofs << std::endl;
  
  std::vector<std::string> v_s_order;
  if (model == 0) {
    if (s_output == "strain") {
      v_s_order = {"e11", "k11", "k12", "k13"};
    } else if (s_output == "stress") {
      v_s_order = {"f1", "m1", "m2", "m3"};
    }
  } else if (model == 1) {
    if (s_output == "strain") {
      v_s_order = {"e11", "g12", "g13", "k11", "k12", "k13"};
    } else if (s_output == "stress") {
      v_s_order = {"f1", "f2", "f3", "m1", "m2", "m3"};
    }
  }
  if (config.fail_envelope) {
    for (int i = 0; i < v_s_order.size(); i++) {
      if (s_axis1 == v_s_order[i]) {
        ofs << std::setw(8) << i + 1;
        break;
      }
    }
    for (int i = 0; i < v_s_order.size(); i++) {
      if (s_axis2 == v_s_order[i]) {
        ofs << std::setw(8) << i + 1;
        break;
      }
    }
    ofs << std::setw(8) << i_div << std::endl;
  }
  if (config.fail_index) {
    for (auto load : v_d_loads) {
      ofs << std::setw(16) << load;
    }
    ofs << std::endl;
  }

  ofs << std::endl;

  ofs.close();

  i_indent--;

  return;
}
