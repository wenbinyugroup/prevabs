#include "PModel.hpp"
#include "PModelIO.hpp"
#include "AppConfigIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

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


// ---------------------------------------------------------------------------
// Global definitions (declared extern in globalVariables.hpp)
// ---------------------------------------------------------------------------
int i_indent = 0;

bool debug = false;
bool scientific_format = false;
PConfig config;
RuntimeState runtime;
Message* g_msg = nullptr;


// ---------------------------------------------------------------------------
// CLI setup
// ---------------------------------------------------------------------------

void addParserArguments(CLI::App &app) {

  // Required input
  app.add_option("-i,--input", config.main_input, "Input file")->required();

  // Analysis tool — mutually exclusive; default is VABS
  auto *tool_group = app.add_option_group(
    "Analysis tool", "Select the target solver format (default: VABS)");
  tool_group->add_flag("--vabs", "Use VABS format (default)");
  tool_group->add_flag("--sc",   "Use SwiftComp format");
  tool_group->require_option(0, 1);  // zero or one; zero => VABS default

  // Analysis mode — exactly one must be chosen.
  // CLI flags are collected into temporary booleans; the callback maps them
  // to the AnalysisMode enum stored in config.mode.
  static bool f_homo = false, f_dehomo = false,
              f_fs   = false, f_fe     = false, f_fi = false;

  auto *mode_group = app.add_option_group(
    "Analysis mode", "Select the analysis type (exactly one required)");
  mode_group->add_flag("--hm,--homogenization",   f_homo,   "Homogenization");
  mode_group->add_flag("--dh,--dehomogenization",  f_dehomo, "Dehomogenization (recovery)");
  mode_group->add_flag("--fs,--failure-strength",  f_fs,     "Initial failure strength (SwiftComp only)");
  mode_group->add_flag("--fe,--failure-envelope",  f_fe,     "Initial failure envelope (SwiftComp only)");
  mode_group->add_flag("--fi,--failure-index",     f_fi,     "Initial failure index");
  mode_group->require_option(1);  // exactly one mode must be given

  // Solver / execution options
  app.add_option("--ver", config.tool_ver, "Tool version (e.g. 3.0)");
  app.add_flag("--integrated", config.integrated_solver,
    "Use integrated solver (implies --execute)")->default_val(false);
  app.add_flag("-e,--execute", config.execute,
    "Execute VABS/SwiftComp after generating input")->default_val(false);

  // Output / visualisation options
  app.add_flag("-v,--visualize", config.plot,
    "Visualize meshed cross section or contour plots")->default_val(false);
  app.add_option("--gmsh-verbosity", config.app.gmsh_verbosity,
    "Gmsh log verbosity (0=silent, 1=errors, 2=warnings, 3=info, 5=debug)"
  )->default_val(2)->check(CLI::IsMember({0, 1, 2, 3, 5}));

  // Developer options
  app.add_flag("-d,--debug", config.debug, "Debug mode")->default_val(false);

  // Post-parse validation callback
  app.callback([&]() {
    // Resolve tool enum from option group flags
    config.tool = app["--sc"]->count() ? AnalysisTool::SwiftComp : AnalysisTool::VABS;

    // Map mode flags to AnalysisMode enum
    if      (f_homo)   config.mode = AnalysisMode::Homogenization;
    else if (f_dehomo) config.mode = AnalysisMode::Dehomogenization;
    else if (f_fs)     config.mode = AnalysisMode::FailureStrength;
    else if (f_fe)     config.mode = AnalysisMode::FailureEnvelope;
    else if (f_fi)     config.mode = AnalysisMode::FailureIndex;

    // --integrated implies --execute
    if (config.integrated_solver) {
      config.execute = true;
    }

    // Failure strength / envelope are SwiftComp-only
    if (config.isVABS() && (config.isFailStrength() || config.isFailEnvelope())) {
      throw CLI::ValidationError(
        "--fs/--failure-strength and --fe/--failure-envelope "
        "are SwiftComp-only; add --sc");
    }
  });

  // Format help columns
  app.get_formatter()->column_width(50);
}


// ---------------------------------------------------------------------------
// Config post-processing
// ---------------------------------------------------------------------------

void processConfigVariables() {

  // Derive display strings from enums
  config.tool_name = config.isVABS() ? "VABS" : "SwiftComp";

  switch (config.mode) {
    case AnalysisMode::Homogenization:
      config.msg_analysis = "homogenization";
      config.sc_option    = "H";
      break;
    case AnalysisMode::Dehomogenization:
    case AnalysisMode::DehomogenizationNL:
      config.msg_analysis = "recover";
      config.sc_option    = "LG";
      break;
    case AnalysisMode::FailureStrength:
      config.msg_analysis = "failure strength";
      config.sc_option    = "F";
      break;
    case AnalysisMode::FailureIndex:
      config.msg_analysis = "failure index";
      config.vabs_option  = "3";
      config.sc_option    = "FI";
      break;
    case AnalysisMode::FailureEnvelope:
      config.msg_analysis = "failure envelope";
      config.sc_option    = "FE";
      break;
  }

  if (config.debug) {
    config.app.log_level = LOG_LEVEL_DEBUG;
  }

  // Resolve derived file paths
  std::vector<std::string> v_filename{splitFilePath(config.main_input)};
  config.file_directory = v_filename[0];  // ****/****/
  config.file_base_name = v_filename[1];  // ****
  config.file_extension = v_filename[2];  // .****
  config.file_name_deb  = v_filename[0] + v_filename[1] + ".debug";

  config.file_name_geo = config.file_directory + config.file_base_name + ".geo_unrolled";
  config.file_name_msh = config.file_directory + config.file_base_name + ".msh";
  config.file_name_opt = config.file_directory + config.file_base_name + ".opt";
  config.file_name_vsc = config.file_directory + config.file_base_name + ".sg";
  config.file_name_log = config.file_directory + config.file_base_name + ".log";
}


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string getCurrentDateTimeString() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm now_tm{};
#ifdef _WIN32
  localtime_s(&now_tm, &now_time);
#else
  localtime_r(&now_time, &now_tm);
#endif
  std::ostringstream ss;
  ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
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

    // Load multi-level JSON config (system → user → project).
    // This runs after path derivation so project_dir is available,
    // and before initLog() so log_level is applied correctly.
    // CLI --gmsh-verbosity/--debug already wrote into config.app, so
    // we re-apply CLI overrides after loading files.
    AppConfig cli_overrides = config.app;  // snapshot the CLI values
    prevabs::loadAppConfig(config.app, config.file_directory);
    // Re-apply CLI flags that were explicitly set (they take highest priority)
    if (app["--gmsh-verbosity"]->count())
      config.app.gmsh_verbosity = cli_overrides.gmsh_verbosity;
    if (config.debug)
      config.app.log_level = LOG_LEVEL_DEBUG;

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
  g_msg = pmessage.get();

  auto start_s = std::chrono::steady_clock::now();
  pmessage->printTitle();

  auto pmodel_uptr = std::make_unique<PModel>(config.file_base_name);
  bool initialized = false;

  // Single catch-all: any unhandled exception in any pipeline stage is
  // reported here before cleanup, preventing silent exit or second-crash
  // from downstream stages using a partially-built model.
  try {
    pmodel_uptr->initialize();
    initialized = true;

    std::string s_dt_start = getCurrentDateTimeString();
    PLOG(info) << pmessage->message("prevabs start (" + s_dt_start + ")");

    if (config.isHomo()) {
      pmodel_uptr->homogenize();
    } else if (config.isRecovery()) {
      pmessage->printBlank();
      pmodel_uptr->dehomogenize();
      pmessage->printBlank();
    }

    if (config.execute) {
      pmodel_uptr->run();
    }

    if (config.plot) {
      pmodel_uptr->plot();
    }

    pmodel_uptr->finalize();
    initialized = false;

    std::string s_dt_finish = getCurrentDateTimeString();
    auto stop_s = std::chrono::steady_clock::now();
    double tt = std::chrono::duration<double>(stop_s - start_s).count();
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
  catch (const std::exception &e) {
    const std::string msg = e.what();
    if (g_msg) { g_msg->error(msg); }
    PLOG(error) << msg;
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    pmessage->closeFile();
    return 1;
  }
  catch (...) {
    const std::string msg = "unknown fatal error";
    if (g_msg) { g_msg->error(msg); }
    PLOG(error) << msg;
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    pmessage->closeFile();
    return 1;
  }
}
