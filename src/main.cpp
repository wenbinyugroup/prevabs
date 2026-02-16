#include "PModel.hpp"
#include "PModelIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh_mod/StringUtils.h"

#include "CLI11.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif


int i_indent = 0;

bool debug = false;
bool scientific_format = false;
PConfig config;


void addParserArguments(CLI::App &app) {

  // ---------------------------------------------------------------------------
  // Required input
  // ---------------------------------------------------------------------------
  app.add_option("-i,--input", config.main_input, "Input file")->required();

  // ---------------------------------------------------------------------------
  // Analysis tool — mutually exclusive; default is VABS
  // ---------------------------------------------------------------------------
  auto *tool_group = app.add_option_group(
    "Analysis tool", "Select the target solver format (default: VABS)");
  tool_group->add_flag("--vabs", "Use VABS format (default)");
  tool_group->add_flag("--sc",   "Use SwiftComp format");
  tool_group->require_option(0, 1);  // zero or one; zero => VABS default

  // ---------------------------------------------------------------------------
  // Analysis mode — exactly one must be chosen
  // ---------------------------------------------------------------------------
  auto *mode_group = app.add_option_group(
    "Analysis mode", "Select the analysis type (exactly one required)");
  mode_group->add_flag("--hm,--homogenization",    config.homo,          "Homogenization");
  mode_group->add_flag("--dh,--dehomogenization",  config.dehomo,        "Dehomogenization (recovery)");
  mode_group->add_flag("--fs,--failure-strength",  config.fail_strength, "Initial failure strength (SwiftComp only)");
  mode_group->add_flag("--fe,--failure-envelope",  config.fail_envelope, "Initial failure envelope (SwiftComp only)");
  mode_group->add_flag("--fi,--failure-index",     config.fail_index,    "Initial failure index");
  mode_group->require_option(1);  // exactly one mode must be given

  // ---------------------------------------------------------------------------
  // Solver / execution options
  // ---------------------------------------------------------------------------
  app.add_option("--ver", config.tool_ver, "Tool version (e.g. 3.0)");
  app.add_flag("--integrated", config.integrated_solver,
    "Use integrated solver (implies --execute)")->default_val(false);
  app.add_flag("-e,--execute", config.execute,
    "Execute VABS/SwiftComp after generating input")->default_val(false);

  // ---------------------------------------------------------------------------
  // Output / visualisation options
  // ---------------------------------------------------------------------------
  app.add_flag("-v,--visualize", config.plot,
    "Visualize meshed cross section or contour plots")->default_val(false);
  app.add_option("--gmsh-verbosity", config.gmsh_verbosity,
    "Gmsh log verbosity (0=silent, 1=errors, 2=warnings, 3=info, 5=debug)"
  )->default_val(2)->check(CLI::IsMember({0, 1, 2, 3, 5}));

  // ---------------------------------------------------------------------------
  // Developer options
  // ---------------------------------------------------------------------------
  app.add_flag("-d,--debug", config.debug, "Debug mode")->default_val(false);

  // ---------------------------------------------------------------------------
  // Post-parse validation callback
  // ---------------------------------------------------------------------------
  app.callback([&]() {
    // Resolve tool selection from option group flags
    if (app["--sc"]->count()) {
      config.analysis_tool = 2;
    } else {
      config.analysis_tool = 1;  // default: VABS
    }

    // --integrated implies --execute
    if (config.integrated_solver) {
      config.execute = true;
    }

    // Failure strength / envelope analyses are SwiftComp-only
    if (config.analysis_tool == 1 && (config.fail_strength || config.fail_envelope)) {
      throw CLI::ValidationError(
        "--fs/--failure-strength and --fe/--failure-envelope "
        "are SwiftComp-only; add --sc");
    }
  });

  // Format help columns
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
  config.file_name_deb  = v_filename[0] + v_filename[1] + ".debug";

  config.file_name_geo = config.file_directory + config.file_base_name + ".geo_unrolled";
  config.file_name_msh = config.file_directory + config.file_base_name + ".msh";
  config.file_name_opt = config.file_directory + config.file_base_name + ".opt";
  config.file_name_vsc = config.file_directory + config.file_base_name + ".sg";
  config.file_name_log = config.file_directory + config.file_base_name + ".log";
}




std::string getCurrentDateTimeString() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm *now_tm = std::localtime(&now_time);
  std::ostringstream ss;
  ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}




// ===================================================================
int main(int argc, char **argv) {

  CLI::App app{
    "PreVABS: parametric pre-/post-processor for 2D cross-sections "
    "(VABS / SwiftComp)"};

  addParserArguments(app);

  try {
    app.parse(argc, argv);
    processConfigVariables();
    initLog();
  }
  catch (const CLI::ParseError &e) {
    return app.exit(e);
  }
  catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }


  // -----------------------------------------------------------------

  auto pmessage = std::make_unique<Message>(config.file_name_log);
  pmessage->openFile();

  clock_t start_s = clock();

  pmessage->printTitle();

  auto pmodel_uptr = std::make_unique<PModel>(config.file_base_name);
  pmodel_uptr->initialize();

  std::string s_dt_start = getCurrentDateTimeString();
  PLOG(info) << pmessage->message("prevabs start (" + s_dt_start + ")");


  if (config.homo) {

    pmodel_uptr->homogenize(pmessage.get());

  }
  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    try {
      pmessage->printBlank();
      pmodel_uptr->dehomogenize(pmessage.get());
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
    pmodel_uptr->run(pmessage.get());
  }

  if (config.plot) {
    pmodel_uptr->plot(pmessage.get());
  }

  pmodel_uptr->finalize();


  std::string s_dt_finish = getCurrentDateTimeString();

  clock_t stop_s = clock();
  double tt = static_cast<double>(stop_s - start_s) / CLOCKS_PER_SEC;
  std::ostringstream ss;
  ss << "total running time: " << tt << " sec";

  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();
  PLOG(info) << pmessage->message("prevabs finished (" + s_dt_finish + ")");
  PLOG(info) << ss.str();
  pmessage->printBlank();
  pmessage->printDivider(40, '=');
  pmessage->printBlank();

  pmessage->closeFile();

  return 0;
}
