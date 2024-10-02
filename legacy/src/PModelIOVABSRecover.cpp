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


void PModel::recoverVABS() {
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

  printInfo(i_indent, "reading VABS input data: " + config.file_name_vsc);

  // ------------------------------
  xml_node<> *p_xn_analysis{p_xn_cs->first_node("analysis")};
  int model = 0;
  if (p_xn_analysis) {
    xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
    if (p_xn_model) {
      model = atoi(p_xn_model->value());
    }
  }

  int dehomo_nl = 2;
  xml_node<> *p_xn_dehomo{p_xn_analysis->first_node("dehomo")};
    if (p_xn_dehomo) {
      std::string ss{p_xn_dehomo->value()};
      if (ss[0] != '\0') {
        dehomo_nl = atoi(ss.c_str());
        if (dehomo_nl == 1) {
          config.dehomo_nl = true;
        }
      }
    }









  // -----------------------------------------------------------------
  // Read *_vabs.dat
  // Change the recover_flag (row 2 col 2) to 1 (nonlinear beam theory)
  // std::ifstream ifs_dat(config.file_name_vsc);
  // std::string s_line;
  // std::vector<std::vector<int>> vv_i_head;
  // std::vector<std::string> v_s_body;
  // bool head{true};
  // while (std::getline(ifs_dat, s_line)) {
  //   if (head) {
  //     std::vector<int> vi;
  //     vi = parseIntegersFromString(s_line);
  //     if (vi.size() > 0) {
  //       vv_i_head.push_back(vi);
  //     }
  //     if (vv_i_head.size() == 4)
  //       head = false;
  //   }
  //   else {
  //     v_s_body.push_back(s_line);
  //   }
  // }




  // // Set the recover flag to 2 (linear beam theory)
  // vv_i_head[1][1] = 2;

  // xml_node<> *p_xn_analysis{p_xn_cs->first_node("analysis")};
  // xml_node<> *p_xn_recover{p_xn_analysis->first_node("recover")};
  // if (p_xn_recover) {
  //   std::string ss{p_xn_recover->value()};
  //   if (ss[0] != '\0') {
  //     vv_i_head[1][1] = atoi(ss.c_str());
  //   }
  // }




  // ifs_dat.close();









  // -----------------------------------------------------------------
  // Read *.xml
  // Append global analysis results

  printInfo(i_indent, "reading structural analysis results");

  xml_node<> *p_xn_global{p_xn_cs->first_node("global")};

  std::vector<double> vd_displacement(3, 0.0);
  std::vector<double> vd_rotation(9, 0.0);
  vd_rotation[0] = 1.0;
  vd_rotation[4] = 1.0;
  vd_rotation[8] = 1.0;
  std::vector<double> vd_rotation1{3};
  std::vector<double> vd_rotation2{3};
  std::vector<double> vd_rotation3{3};
  std::vector<double> vd_load;
  std::vector<double> vd_force(3, 0.0);
  std::vector<double> vd_moment(3, 0.0);
  std::vector<double> vd_dforce0(3, 0.0);
  std::vector<double> vd_dforce1(3, 0.0);
  std::vector<double> vd_dforce2(3, 0.0);
  std::vector<double> vd_dforce3(3, 0.0);
  std::vector<double> vd_dmoment0(3, 0.0);
  std::vector<double> vd_dmoment1(3, 0.0);
  std::vector<double> vd_dmoment2(3, 0.0);
  std::vector<double> vd_dmoment3(3, 0.0);

  if (p_xn_global) {

    xml_node<> *p_xn_temp;

    p_xn_temp = p_xn_global->first_node("displacements");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      vd_displacement = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_global->first_node("rotations");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
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
      vd_load = parseNumbersFromString(p_xn_temp->value());
    }
    if (model == 0) {
      vd_force[0] = vd_load[0];
      vd_moment[0] = vd_load[1];
      vd_moment[1] = vd_load[2];
      vd_moment[2] = vd_load[3];
    }
    else if (model == 1) {
      vd_force[0] = vd_load[0];
      vd_force[1] = vd_load[1];
      vd_force[2] = vd_load[2];
      vd_moment[0] = vd_load[3];
      vd_moment[1] = vd_load[4];
      vd_moment[2] = vd_load[5];
    }

    // p_xn_temp = p_xn_global->first_node("forces");
    // if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
    //   vd_force = parseNumbersFromString(p_xn_temp->value());
    // }

    // p_xn_temp = p_xn_global->first_node("moments");
    // if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
    //   vd_moment = parseNumbersFromString(p_xn_temp->value());
    // }

    xml_node<> *p_xn_distributed{p_xn_global->first_node("distributed")};
    if (p_xn_distributed) {
      p_xn_temp = p_xn_distributed->first_node("forces");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dforce0 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d1");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dforce1 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d2");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dforce2 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d3");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dforce3 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dmoment0 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d1");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dmoment1 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d2");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dmoment2 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d3");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        vd_dmoment3 = parseNumbersFromString(p_xn_temp->value());
      }
    }
  }









  // -----------------------------------------------------------------
  // Write new *_vabs.dat file
  printInfo(i_indent, "writing VABS .glb file");
  std::ofstream ofs_dat;
  ofs_dat.open(s_file_name_dat);

  ofs_dat << std::scientific;

  // for (auto vi : vv_i_head) {
  //   writeVectorToFile(ofs_dat, vi);
  //   ofs_dat << "\n";
  // }

  // for (auto n : v_s_body) {
  //   ofs_dat << n << "\n";
  // }

  // ofs_dat << std::endl;

  writeVectorToFile(ofs_dat, vd_displacement);
  ofs_dat << "\n\n";
  writeVectorToFile(ofs_dat, vd_rotation1);
  ofs_dat << "\n";
  writeVectorToFile(ofs_dat, vd_rotation2);
  ofs_dat << "\n";
  writeVectorToFile(ofs_dat, vd_rotation3);
  ofs_dat << "\n\n";

  ofs_dat << std::setw(16) << vd_force[0];
  writeVectorToFile(ofs_dat, vd_moment);
  ofs_dat << "\n";

  if (model == 1) {
    ofs_dat << std::setw(16) << vd_force[1] << std::setw(16) << vd_force[2] << "\n\n";

    writeVectorToFile(ofs_dat, vd_dforce0);
    writeVectorToFile(ofs_dat, vd_dmoment0);
    ofs_dat << "\n";
    writeVectorToFile(ofs_dat, vd_dforce1);
    writeVectorToFile(ofs_dat, vd_dmoment1);
    ofs_dat << "\n";
    writeVectorToFile(ofs_dat, vd_dforce2);
    writeVectorToFile(ofs_dat, vd_dmoment2);
    ofs_dat << "\n";
    writeVectorToFile(ofs_dat, vd_dforce3);
    writeVectorToFile(ofs_dat, vd_dmoment3);
    ofs_dat << "\n";
  }

  ofs_dat.close();

  i_indent--;

  return;
}



