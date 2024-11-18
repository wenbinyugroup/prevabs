#include "PModel.hpp"
#include "PModelIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh_mod/StringUtils.h"

#include "CLI11.hpp"

#include <ctime>
#include <iostream>
#include <stdexcept>
// #include <exception>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>


#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

int i_indent = 0;

bool debug = false;
bool scientific_format = false;
PConfig config;


// class CommandLineException : public std::exception {
// public:
//   explicit CommandLineException(const std::string& message) : msg_(message) {}
//   virtual const char* what() const noexcept override {
//     return msg_.c_str();
//   }
// private:
//   std::string msg_;
// };


void addParserArguments(CLI::App &app) {

  app.add_option("-i,--input", config.main_input, "Input file")->required();
  app.add_flag("--vabs", config.analysis_tool, "Use VABS format")->default_val(1);
  app.add_flag("--sc", config.analysis_tool, "Use SwiftComp format")->default_val(2);
  app.add_option("--ver", config.tool_ver, "Tool version");
  app.add_flag("--int", config.integrated_solver, "Use integrated solver")->default_val(false);
  app.add_flag("-e,--execute", config.execute, "Execute VABS/SwiftComp")->default_val(false);
  app.add_flag("-v,--visualize", config.plot, "Visualize meshed cross section or contour plots")->default_val(false);
  app.add_flag("-d,--debug", config.debug, "Debug mode")->default_val(false);

  app.add_flag("--hm,--homogenization", config.homo, "Homogenization")->default_val(false);
  app.add_flag("--dh,--dehomogenization", config.dehomo, "Dehomogenization")->default_val(false);
  app.add_flag("--fs,--failure-strength", config.fail_strength, "Failure strength")->default_val(false);
  app.add_flag("--fe,--failure-envelope", config.fail_envelope, "Failure envelope")->default_val(false);
  app.add_flag("--fi,--failure-index", config.fail_index, "Failure index")->default_val(false);

  // Format help message
  app.get_formatter()->column_width(50);

}


// Old CLI, will be deprecated
void printCliHelp() {
  std::cout << std::endl;
  std::cout << " Usage" << std::endl;
  std::cout << std::string(20, '-') << std::endl;
  std::cout << "         prevabs -i <file_path/file_name.xml> [options]\n";
  std::cout << std::endl;
  std::cout << " Analysis options" << std::endl;
  std::cout << std::string(20, '-') << std::endl;
  std::cout << " -h      Build cross section and generate VABS/SwiftComp input file for homogenization.\n";
  std::cout << " -d      Read 1D beam analysis results and update VABS/SwiftComp input file for dehomogenization.\n";
  std::cout << " -fi     Initial failure indices and strength ratios.\n";
  std::cout << " -f      Initial failure strength analysis (SwiftComp only).\n";
  std::cout << " -fe     Initial failure envelope (SwiftComp only).\n";
  std::cout << std::endl;
  std::cout << " Format and execution options" << std::endl;
  std::cout << std::string(20, '-') << std::endl;
  std::cout << " -vabs   Use VABS format (Default).\n";
  std::cout << " -sc     Use SwiftComp format.\n";
  std::cout << " -int    Use integrated solver.\n";
  std::cout << " -e      Execute VABS/SwiftComp.\n";
  std::cout << " -v      Visualize meshed cross section for homogenization or contour plots of stresses and strains after recovery.\n";
  std::cout << " -debug  Debug mode.\n";
  std::cout << std::endl;
}


// Old CLI, will be deprecated
void parseArguments(int argc, char** argv) {

  for (int i = 1; i < argc; ++i) {
    if (std::string{argv[i]} == "-i")
      config.main_input = std::string{argv[i + 1]};
    
    if (std::string{argv[i]} == "-vabs") {
      config.analysis_tool = 1;
    }
    if (std::string{argv[i]} == "-sc") {
      config.analysis_tool = 2;
    }

    if (std::string{argv[i]} == "-int") {
      config.integrated_solver = true;
      config.execute = true;
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
      config.fail_envelope = false;
      config.fail_index = false;
      config.fail_strength = true;
    }
    if (std::string{argv[i]} == "-fe") {
      config.fail_envelope = true;
      config.fail_index = false;
      config.fail_strength = false;
    }
    if (std::string{argv[i]} == "-fi") {
      config.fail_envelope = false;
      config.fail_index = true;
      config.fail_strength = false;
    }

    if (std::string{argv[i]} == "-v") {
      config.plot = true;
    }
    if (std::string{argv[i]} == "-e") {
      config.execute = true;
    }
    if (std::string{argv[i]} == "-debug") {
      config.debug = true;
    }
  }

  // throw exception
  if (config.main_input.empty()) {
    throw std::runtime_error("Please provide input file: -i working_directory/file_name.xml");
  }

  if (!(config.homo || config.dehomo || config.fail_strength || config.fail_envelope || config.fail_index)) {
    throw std::runtime_error("Please indicate an analysis: -h or -d or -f or -fe or -fi\n");
  }

  if ((config.analysis_tool == 1) && (config.fail_strength || config.fail_envelope)) {
    throw std::runtime_error("Failure strength and failure envelope analyses can only be carried out by SwiftComp: -sc\n");
  }

}


void processConfigVariables() {

  if (config.analysis_tool == 1) {
    config.tool_name = "VABS";
  } else if (config.analysis_tool == 2) {
    config.tool_name = "SwiftComp";
  }

  if (config.homo && !config.dehomo) {
    config.msg_analysis = "homogenization";
    config.sc_option = "H";
  }

  if (!config.homo && config.dehomo) {
    config.msg_analysis = "recover";
    config.sc_option = "LG";
  }

  if (config.fail_strength && !config.fail_index && !config.fail_envelope) {
    config.msg_analysis = "failure strength";
    config.sc_option = "F";
  }

  if (!config.fail_strength && config.fail_index && !config.fail_envelope) {
    config.msg_analysis = "failure index";
    config.vabs_option = "3";
    config.sc_option = "FI";
  }

  if (!config.fail_strength && !config.fail_index && config.fail_envelope) {
    config.msg_analysis = "failure envelope";
    config.sc_option = "FE";
  }

  if (config.debug) {
    config.log_severity_level = 1;
  }

  std::vector<std::string> v_filename{gmshSplitFileName(config.main_input)};
  config.file_directory = v_filename[0]; // ****/****/
  config.file_base_name = v_filename[1]; // ****
  config.file_extension = v_filename[2]; // .****
  config.file_name_deb = v_filename[0] + v_filename[1] + ".debug";

  config.file_name_geo = config.file_directory + config.file_base_name + ".geo_unrolled";
  config.file_name_msh = config.file_directory + config.file_base_name + ".msh";
  config.file_name_opt = config.file_directory + config.file_base_name + ".opt";
  config.file_name_vsc = config.file_directory + config.file_base_name + ".sg";
  config.file_name_log = config.file_directory + config.file_base_name + ".log";

}




std::string getCurrentDateTimeString() {
  // Get the current time
  auto now = std::chrono::system_clock::now();
  // Convert to time_t to get calendar time
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  // Convert to tm structure
  std::tm* now_tm = std::localtime(&now_time);

  // Create a string stream to format the date and time
  std::stringstream ss;
  ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");

  return ss.str();
}






// ===================================================================
int main(int argc, char** argv) {

  // CLI::App app{"PreVABS, a parametric pre-/post-processor for 2D cross-sections for VABS/SwiftComp"};
  // addParserArguments(app);

  try {
    // app.parse(argc, argv);

    // Old CLI, will be deprecated
    if (argc < 2) {
      printCliHelp();
      return 0;
    }

    parseArguments(argc, argv);

    processConfigVariables();
  }

  // catch (const CLI::ParseError &e) {
  //   // Handle CLI parsing errors
  //   return app.exit(e);
  // }

  catch (const std::exception &e) {
    // Handle other exceptions
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }




  // -----------------------------------------------------------------

  // Message *pmessage = new Message(v_filename[0] + v_filename[1] + ".txt");
  Message *pmessage = new Message(config.file_name_log);
  pmessage->openFile();

  int start_s = clock();

  pmessage->printTitle();

  PModel *pmodel = new PModel(config.file_base_name);
  pmodel->initialize();


  // Log current date and time
  std::string s_dt_start = getCurrentDateTimeString();

  PLOG(info) << pmessage->message("prevabs start (" + s_dt_start + ")");









  if (config.homo) {

    pmodel->homogenize(pmessage);

  }




  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    try {
      pmessage->printBlank();
      pmodel->dehomogenize(pmessage);
      pmessage->printBlank();
    }
    catch (std::exception &exception) {
      pmessage->print(2, exception.what());
      return 0;
    }
  }









  // =========
  // EXECUTION
  // =========

  if (config.execute) {

    pmodel->run(pmessage);

  }




  if (config.plot) {

    pmodel->plot(pmessage);

  }

  pmodel->finalize();


  std::string s_dt_finish = getCurrentDateTimeString();

  int stop_s = clock();
  double tt = (stop_s - start_s) / double(CLOCKS_PER_SEC);
  std::stringstream ss;
  ss << "total running time: " << tt << " sec";
  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();
  // pmessage->print(0, "  FINISHED");
  PLOG(info) << pmessage->message("prevabs finished (" + s_dt_finish + ")");
  // pmessage->print(0, ss);
  PLOG(info) << ss.str();
  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();

  pmessage->closeFile();

  return 0;
}

