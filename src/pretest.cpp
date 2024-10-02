// #include "GeneralCS.h"
// #include "generalClasses.hpp"
// #include "generalFunctions.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "internalClasses.hpp"
// #include "postProcess.hpp"
// #include "readInputs.hpp"
// #include "recover.h"
#include "utilities.hpp"

#include "gmsh_mod/StringUtils.h"

// #include <cmath>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

int i_indent = 1;

bool debug = false;
bool scientific_format = false;
PConfig config;

// ===================================================================
int main(int argc, char *argv[]) {
  int start_s = clock();

  // Print title
  std::cout << std::endl;
  std::cout << std::string(40, '=') << std::endl;
  std::cout << std::endl;
  std::cout << " PreVABS version 0.7" << std::endl;
  std::cout << std::endl;
  std::cout << std::string(40, '=') << std::endl;

  // Print short documentation
  if (argc < 2) {
    std::cout << std::endl;
    std::cout << " Usage" << std::endl;
    std::cout << std::string(20, '-') << std::endl;
    std::cout << "        prevabs -i <file_path/file_name.xml> [options]"
              << std::endl;
    std::cout << std::endl;
    std::cout << " Options" << std::endl;
    std::cout << std::string(20, '-') << std::endl;
    std::cout
        << " -h     Build cross section and generate VABS/SwiftComp input file"
        << std::endl;
    std::cout << "        for homogenization" << std::endl;
    std::cout
        << " -d     Read 1D beam analysis results and update VABS/SwiftComp"
        << std::endl;
    std::cout << "        input file for recovery" << std::endl;
    std::cout << " -v     Visualize meshed cross section for homogenization or"
              << std::endl;
    std::cout << "        contour plots of stresses and strains after recovery"
              << std::endl;
    std::cout << " -e     Execute VABS/SwiftComp" << std::endl;
    std::cout << std::endl;
    std::cout << " -vabs  Use VABS (Default)" << std::endl;
    std::cout << " -sc    Use SwiftComp" << std::endl;
    std::cout << std::endl;
    std::cout << "        Below are SwiftComp only" << std::endl;
    std::cout << " -f     Initial failure strength analysis" << std::endl;
    std::cout << " -fe    Initial failure envelope" << std::endl;
    std::cout << " -fi    Initial failure indices and strength ratios"
              << std::endl;
    std::cout << std::endl;
    return 0;
  }

  // Read arguments
  for (int i = 1; i < argc; ++i) {
    if (std::string{argv[i]} == "-i")
      config.main_input = std::string{argv[i + 1]};
    
    if (std::string{argv[i]} == "-vabs") {
      config.analysis_tool = 1;
    }
    if (std::string{argv[i]} == "-sc") {
      config.analysis_tool = 2;
    }
    
    if (std::string{argv[i]} == "-h") {
      config.homo = true;
      config.dehomo = false;
    }
    if (std::string{argv[i]} == "-d") {
      config.homo = false;
      config.dehomo = true;
    }

    if (std::string{argv[i]} == "-f") {
      config.fail_strength = true;
    }
    if (std::string{argv[i]} == "-fe") {
      config.fail_envelope = true;
    }
    if (std::string{argv[i]} == "-fi") {
      config.fail_index = true;
    }

    if (std::string{argv[i]} == "-v") {
      config.plot = true;
    }
    if (std::string{argv[i]} == "-e") {
      config.execute = true;
    }
    if (std::string{argv[i]} == "-debug")
      config.debug = true;
  }

  if (config.main_input.empty()) {
    std::cout
        << "Please provide input file: -i working_directory/file_name.xml\n";
    return 0;
  }

  std::vector<std::string> v_filename{SplitFileName(config.main_input)};
  config.file_directory = v_filename[0]; // ****/****/
  config.file_base_name = v_filename[1]; // ****
  config.file_extension = v_filename[2]; // .****
  config.file_name_deb = v_filename[0] + v_filename[1] + ".debug";


  PModel *pmodel = new PModel("test");
  pmodel->initialize();

  // ================
  // READ INPUT FILES
  // ================

  try {
    std::cout << "\n\n\n";
    printInfo(i_indent, "Reading Inputs");
    // read(s_file_name, s_file_path, p_pvm);
    readInputMain(config.main_input, config.file_directory, pmodel);
  } catch (std::exception &exception) {
    std::cerr << "ERROR: " << exception.what() << std::endl;
    return 0;
  }

  // ==============
  // BUILD GEOMETRY
  // ==============

  try {
    std::cout << "\n\n\n";
    printInfo(i_indent, "Building");
    pmodel->build();
    // printInfo(i_indent, "Build Model End");
  } catch (std::exception &exception) {
    std::cerr << "ERROR: " << exception.what() << std::endl;
    return 0;
  }

  // ================
  // MODELING IN GMSH
  // ================

  try {
    std::cout << "\n\n\n";
    printInfo(i_indent, "Modeling in Gmsh");
    pmodel->buildGmsh();
    // printInfo(i_indent, "Build Model End");
  } catch (std::exception &exception) {
    std::cerr << "ERROR: " << exception.what() << std::endl;
    return 0;
  }

  // ===================
  // WRITE SG INPUT FILE
  // ===================

  try {
    std::cout << "\n\n\n";
    printInfo(i_indent, "- writing outputs");
    if (config.plot) {
      pmodel->writeGmsh(config.file_directory + config.file_base_name);
    }
    pmodel->writeSG(config.file_directory + config.file_base_name, config.analysis_tool);
    printInfo(i_indent, "- writing outputs -- done");
  } catch (std::exception &exception) {
    std::cerr << "ERROR: " << exception.what() << std::endl;
    return 0;
  }

  pmodel->finalize();

  // =========
  // EXECUTION
  // =========

  if (config.execute) {
    if (config.analysis_tool == 1) {
      if (config.homo) {
        std::cout << "- running VABS for homogenization" << std::endl;
        runVABS(config.file_name_vsc);
        std::cout << "- running VABS for homogenization -- done" << std::endl;
      }
    } else if (config.analysis_tool == 2) {
      if (config.homo) {
        std::cout << "- running SwiftComp for homogenization" << std::endl;
        runSC(config.file_name_vsc, "H");
        std::cout << "- running SwiftComp for homogenization -- done" << std::endl;
      }
    }
  }

  if (config.plot) {
    std::cout << "- running Gmsh for visualization" << std::endl;
    runGmsh(config.file_name_geo, config.file_name_msh, config.file_name_opt);
  }

  int stop_s = clock();
  std::cout << singleLine << std::endl;
  std::cout << " Total running time: "
            << (stop_s - start_s) / double(CLOCKS_PER_SEC) << " sec"
            << std::endl;
  std::cout << singleLine << std::endl;
  std::cout << std::endl;

  return 0;
}

