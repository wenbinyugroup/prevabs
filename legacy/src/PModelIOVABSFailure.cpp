#include "PModel.hpp"

#include "PBaseLine.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

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


void PModel::failureVABS(Message *pmessage) {
  i_indent++;
  pmessage->increaseIndent();

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
  printInfo(i_indent, "reading structural analysis results");

  xml_node<> *p_xn_global{p_xn_cs->first_node("global")};

  LoadCase loadcase;

  // std::vector<double> vd_displacement(3, 0.0);
  std::vector<double> vd_rotation(9, 0.0);
  // vd_rotation[0] = 1.0;
  // vd_rotation[4] = 1.0;
  // vd_rotation[8] = 1.0;
  // std::vector<double> vd_rotation1{3};
  // std::vector<double> vd_rotation2{3};
  // std::vector<double> vd_rotation3{3};
  std::vector<double> vd_load;
  // std::vector<double> vd_force(3, 0.0);
  // std::vector<double> vd_moment(3, 0.0);
  // std::vector<double> vd_dforce0(3, 0.0);
  // std::vector<double> vd_dforce1(3, 0.0);
  // std::vector<double> vd_dforce2(3, 0.0);
  // std::vector<double> vd_dforce3(3, 0.0);
  // std::vector<double> vd_dmoment0(3, 0.0);
  // std::vector<double> vd_dmoment1(3, 0.0);
  // std::vector<double> vd_dmoment2(3, 0.0);
  // std::vector<double> vd_dmoment3(3, 0.0);

  if (p_xn_global) {

    xml_node<> *p_xn_temp;

    p_xn_temp = p_xn_global->first_node("displacements");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      // vd_displacement = parseNumbersFromString(p_xn_temp->value());
      loadcase.displacement = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_global->first_node("rotations");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      vd_rotation = parseNumbersFromString(p_xn_temp->value());
    }
    // vd_rotation1 =
    //     std::vector<double>(vd_rotation.begin(), vd_rotation.begin() + 3);
    // vd_rotation2 =
    //     std::vector<double>(vd_rotation.begin() + 3, vd_rotation.begin() + 6);
    // vd_rotation3 =
    //     std::vector<double>(vd_rotation.begin() + 6, vd_rotation.begin() + 9);
    loadcase.ratation[0] =
        std::vector<double>(vd_rotation.begin(), vd_rotation.begin() + 3);
    loadcase.ratation[1] =
        std::vector<double>(vd_rotation.begin() + 3, vd_rotation.begin() + 6);
    loadcase.ratation[2] =
        std::vector<double>(vd_rotation.begin() + 6, vd_rotation.begin() + 9);


    p_xn_temp = p_xn_global->first_node("loads");
    if (p_xn_temp) {
      vd_load = parseNumbersFromString(p_xn_temp->value());
    }
    if (model == 0) {
      // vd_force[0] = vd_load[0];
      // vd_moment[0] = vd_load[1];
      // vd_moment[1] = vd_load[2];
      // vd_moment[2] = vd_load[3];
      loadcase.force[0] = vd_load[0];
      loadcase.moment[0] = vd_load[1];
      loadcase.moment[1] = vd_load[2];
      loadcase.moment[2] = vd_load[3];
    }
    else if (model == 1) {
      // vd_force[0] = vd_load[0];
      // vd_force[1] = vd_load[1];
      // vd_force[2] = vd_load[2];
      // vd_moment[0] = vd_load[3];
      // vd_moment[1] = vd_load[4];
      // vd_moment[2] = vd_load[5];
      loadcase.force[0] = vd_load[0];
      loadcase.force[1] = vd_load[1];
      loadcase.force[2] = vd_load[2];
      loadcase.moment[0] = vd_load[3];
      loadcase.moment[1] = vd_load[4];
      loadcase.moment[2] = vd_load[5];
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
        // vd_dforce0 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_force = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d1");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dforce1 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_force_d1 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d2");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dforce2 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_force_d2 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("forces_d3");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dforce3 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_force_d3 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dmoment0 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_moment = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d1");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dmoment1 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_moment_d1 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d2");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dmoment2 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_moment_d2 = parseNumbersFromString(p_xn_temp->value());
      }

      p_xn_temp = p_xn_distributed->first_node("moments_d3");
      if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
        // vd_dmoment3 = parseNumbersFromString(p_xn_temp->value());
        loadcase.distr_moment_d3 = parseNumbersFromString(p_xn_temp->value());
      }
    }
  }

  _load_cases.append(loadcase);




  // -----------------------------------------------------------------
  // Write new *.sg.glb file
  writeGLB(s_file_name_glb, pmessage);

  // // printInfo(i_indent, "writing .glb file");
  // std::ofstream ofs;
  // ofs.open(s_file_name_glb);

  // ofs << std::scientific;

  // // Write material strength properties
  // for (auto pm : _cross_section->getUsedMaterials()) {
  //   ofs << std::setw(8) << pm->getFailureCriterion()
  //       << std::setw(8) << pm->getStrength().size() << std::endl;

  //   for (auto s : pm->getStrength()) {
  //     ofs << std::setw(16) << s;
  //   }
  //   ofs << std::endl;
  //   ofs << std::endl;
  // }

  // // Write global displacements, rotations and loads
  // writeVectorToFile(ofs, vd_displacement);
  // ofs << "\n\n";
  // writeVectorToFile(ofs, vd_rotation1);
  // ofs << "\n";
  // writeVectorToFile(ofs, vd_rotation2);
  // ofs << "\n";
  // writeVectorToFile(ofs, vd_rotation3);
  // ofs << "\n\n";

  // ofs << std::setw(16) << vd_force[0];
  // writeVectorToFile(ofs, vd_moment);
  // ofs << "\n";

  // if (model == 1) {
  //   ofs << std::setw(16) << vd_force[1] << std::setw(16) << vd_force[2] << "\n\n";

  //   writeVectorToFile(ofs, vd_dforce0);
  //   writeVectorToFile(ofs, vd_dmoment0);
  //   ofs << "\n";
  //   writeVectorToFile(ofs, vd_dforce1);
  //   writeVectorToFile(ofs, vd_dmoment1);
  //   ofs << "\n";
  //   writeVectorToFile(ofs, vd_dforce2);
  //   writeVectorToFile(ofs, vd_dmoment2);
  //   ofs << "\n";
  //   writeVectorToFile(ofs, vd_dforce3);
  //   writeVectorToFile(ofs, vd_dmoment3);
  //   ofs << "\n";
  // }

  // ofs.close();

  i_indent--;
  pmessage->decreaseIndent();

  return;
}
