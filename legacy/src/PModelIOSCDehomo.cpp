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


void PModel::dehomoSC() {
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
  std::string s_file_name_dat;
  s_file_name_dat = config.file_name_vsc + ".glb";

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
  xml_node<> *p_xn_global{p_xn_cs->first_node("global")};

  int measure = 0;  // 0: stress; 1: strain
  std::vector<double> vd_displacement(3, 0.0);
  std::vector<double> vd_rotation(9, 0.0);
  vd_rotation[0] = 1.0;
  vd_rotation[4] = 1.0;
  vd_rotation[8] = 1.0;
  std::vector<double> vd_rotation1{3};
  std::vector<double> vd_rotation2{3};
  std::vector<double> vd_rotation3{3};
  std::vector<double> vd_load;

  if (p_xn_global) {

    xml_attribute<> *p_xa_temp;
    xml_node<> *p_xn_temp;

    p_xa_temp = p_xn_global->first_attribute("measure");
    if (p_xa_temp) {
      std::string s_measure{p_xa_temp->value()};
      if (s_measure == "stress") measure = 0;
      else if (s_measure == "strain") measure = 1;
    }

    p_xn_temp = p_xn_global->first_node("displacements");
    if (p_xn_temp) {
      // std::stringstream ss{p_xn_temp->value()};
      // for (int i = 0; i < 3; ++i) {
      //   double u;
      //   ss >> u;
      //   vd_displacement[i] = u;
      // }
      vd_displacement = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_global->first_node("rotations");
    if (p_xn_temp) {
      // std::stringstream ss{p_xn_temp->value()};
      // for (int i = 0; i < 6; ++i) {
      //   double r;
      //   ss >> r;
      //   vd_rotation[i] = r;
      // }
      vd_rotation = parseNumbersFromString(p_xn_temp->value());
    }
    vd_rotation1 =
        std::vector<double>(vd_rotation.begin(), vd_rotation.begin() + 3);
    vd_rotation2 =
        std::vector<double>(vd_rotation.begin() + 3, vd_rotation.begin() + 6);
    vd_rotation3 =
        std::vector<double>(vd_rotation.begin() + 6, vd_rotation.begin() + 9);

    p_xn_temp = p_xn_global->first_node("loads");
    if (p_xn_temp) {
      // std::stringstream ss{p_xn_temp->value()};
      // int n;
      // if (model == 0) n = 4;
      // else if (model == 1) n = 6;
      // for (int i = 0; i < n; ++i) {
      //   double s;
      //   ss >> s;
      //   vd_loads.push_back(s);
      // }
      vd_load = parseNumbersFromString(p_xn_temp->value());
    }
  }









  // -----------------------------------------------------------------
  // Write new *_vabs.dat file
  printInfo(i_indent, "writing SwiftComp .glb file");
  std::ofstream ofs_dat;
  ofs_dat.open(s_file_name_dat);

  ofs_dat << std::scientific;

  writeVectorToFile(ofs_dat, vd_displacement);
  ofs_dat << "\n\n";
  writeVectorToFile(ofs_dat, vd_rotation1);
  ofs_dat << "\n";
  writeVectorToFile(ofs_dat, vd_rotation2);
  ofs_dat << "\n";
  writeVectorToFile(ofs_dat, vd_rotation3);
  ofs_dat << "\n\n";

  ofs_dat << "    " << measure << "\n\n";

  writeVectorToFile(ofs_dat, vd_load);
  ofs_dat << "\n";

  ofs_dat.close();

  i_indent--;

  return;
}
