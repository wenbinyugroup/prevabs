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






// ===================================================================
int main(int argc, char** argv) {

  CLI::App app{"PreVABS, a parametric pre-/post-processor for 2D cross-sections for VABS/SwiftComp"};
  addParserArguments(app);

  // parseArguments(argc, argv);
  try {
    app.parse(argc, argv);

    processConfigVariables();
  }

  catch (const CLI::ParseError &e) {
    // Handle CLI parsing errors
    return app.exit(e);
    // std::cerr << "Error: " << e.what() << std::endl;
    // return e.get_exit_code();
    // return CLI::Exit(e);
  }

  catch (const std::exception &e) {
    // Handle other exceptions
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }




  // -----------------------------------------------------------------

  PLOG(info) << "prevabs start";

  // Message *pmessage = new Message(v_filename[0] + v_filename[1] + ".txt");
  Message *pmessage = new Message(config.file_name_log);
  pmessage->openFile();

  int start_s = clock();

  pmessage->printTitle();

  PModel *pmodel = new PModel(config.file_base_name);
  pmodel->initialize();










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




  // else if (config.dehomo) {
  //   try {
  //     pmessage->printBlank();
  //     // pmessage->print(1, "reading and writing inputs for dehomogenization");
  //     PLOG(info) << pmessage->message("reading and writing inputs for dehomogenization");

  //     if (config.analysis_tool == 1) {
  //       pmodel->recoverVABS();
  //     }
  //     else if (config.analysis_tool == 2) {
  //       pmodel->dehomoSC();
  //     }

  //     // pmessage->print(1, "reading and writing inputs for dehomogenization -- done");
  //     PLOG(info) << pmessage->message("reading and writing inputs for dehomogenization -- done");
  //     pmessage->printBlank();
  //   }
  //   catch (std::exception &exception) {
  //     pmessage->print(2, exception.what());
  //     return 0;
  //   }
  // }




  // else if (config.fail_strength || config.fail_index || config.fail_envelope) {
  //   try{
  //     pmessage->printBlank();
  //     // pmessage->print(1, "reading and writing inputs for failure analysis");
  //     PLOG(info) << pmessage->message("reading and writing inputs for failure analysis");

  //     readInputMain(config.main_input, config.file_directory, pmodel, pmessage);
  //     if (config.analysis_tool == 1) {
  //       pmodel->failureVABS();
  //     }
  //     else if (config.analysis_tool == 2) {
  //       pmodel->failureSC();
  //     }

  //     // pmessage->print(1, "reading and writing inputs for failure analysis -- done");
  //     PLOG(info) << pmessage->message("reading and writing inputs for failure analysis -- done");
  //     pmessage->printBlank();

  //   }
  //   catch (std::exception &exception) {
  //     pmessage->print(2, exception.what());
  //     return 0;
  //   }
  // }











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



  int stop_s = clock();
  double tt = (stop_s - start_s) / double(CLOCKS_PER_SEC);
  std::stringstream ss;
  ss << "total running time: " << tt << " sec";
  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();
  // pmessage->print(0, "  FINISHED");
  PLOG(info) << "prevabs finished";
  // pmessage->print(0, ss);
  PLOG(info) << ss.str();
  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();

  pmessage->closeFile();

  return 0;
}

