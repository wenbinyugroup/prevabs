#include "PModel.hpp"
#include "PModelIO.hpp"
#include "AppConfigIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "version.h"

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
#include <eh.h>
#include <windows.h>
#endif


// ---------------------------------------------------------------------------
// Global definitions (declared extern in globalVariables.hpp)
// ---------------------------------------------------------------------------
bool scientific_format = false;
PConfig config;
RuntimeState runtime;


// ---------------------------------------------------------------------------
// CLI setup
// ---------------------------------------------------------------------------

void addParserArguments(CLI::App &app) {
  static std::string snapshot_on = "never";
  static std::string debug_level_str = "";

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
  app.add_flag("--nopopup", config.no_popup,
    "Do not open the Gmsh GUI window (same as gmsh -nopopup); "
    "geo/msh files are still written when -v is given")->default_val(false);
  app.add_option("--gmsh-verbosity", config.app.gmsh_verbosity,
    "Gmsh log verbosity (0=silent, 1=errors, 2=warnings, 3=info, 5=debug)"
  )->default_val(2)->check(CLI::IsMember({0, 1, 2, 3, 5}));

  // Developer options
  app.add_option("-d,--debug", debug_level_str,
                 "Debug verbosity: summary|phase|join|geo|all; "
                 "plain -d/--debug defaults to 'join'")
     ->expected(0, 1)
     ->default_str("join")  // value used when --debug given with no argument
     ->check(CLI::IsMember({"", "summary", "phase", "join", "geo", "all"}));
  app.add_option(
      "--snapshot-on", snapshot_on,
      "Write debug geometry snapshots: never, phase, component, all")
      ->default_val("never")
      ->check(CLI::IsMember({"never", "phase", "component", "all"}));

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

    if (snapshot_on == "never") {
      config.snapshot_mode = SnapshotMode::never;
    } else if (snapshot_on == "phase") {
      config.snapshot_mode = SnapshotMode::phase;
    } else if (snapshot_on == "component") {
      config.snapshot_mode = SnapshotMode::component;
    } else if (snapshot_on == "all") {
      config.snapshot_mode = SnapshotMode::all;
    }

    if (debug_level_str == "summary")
      config.debug_level = DebugLevel::summary;
    else if (debug_level_str == "phase")
      config.debug_level = DebugLevel::phase;
    else if (debug_level_str == "join")
      config.debug_level = DebugLevel::join;
    else if (debug_level_str == "geo")
      config.debug_level = DebugLevel::geo;
    else if (debug_level_str == "all")
      config.debug_level = DebugLevel::all;
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

  if (config.debug_level >= DebugLevel::phase) {
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
  config.file_name_log     = config.file_directory + config.file_base_name + ".log";
  config.file_name_log_dev = config.file_directory + config.file_base_name + ".debug.log";
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

#ifdef _WIN32
namespace {

const char *sehCodeLabel(unsigned int code) {
  switch (code) {
  case EXCEPTION_ACCESS_VIOLATION:
    return "access violation";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    return "array bounds exceeded";
  case EXCEPTION_BREAKPOINT:
    return "breakpoint";
  case EXCEPTION_DATATYPE_MISALIGNMENT:
    return "datatype misalignment";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    return "floating-point divide by zero";
  case EXCEPTION_FLT_INVALID_OPERATION:
    return "floating-point invalid operation";
  case EXCEPTION_ILLEGAL_INSTRUCTION:
    return "illegal instruction";
  case EXCEPTION_IN_PAGE_ERROR:
    return "in-page error";
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    return "integer divide by zero";
  case EXCEPTION_PRIV_INSTRUCTION:
    return "privileged instruction";
  case EXCEPTION_STACK_OVERFLOW:
    return "stack overflow";
  default:
    return "structured exception";
  }
}

void translateStructuredException(
    unsigned int code, _EXCEPTION_POINTERS *info) {
  std::ostringstream oss;
  oss << "SEH exception 0x" << std::hex << std::uppercase << code
      << " (" << sehCodeLabel(code) << ")";

  if (info != nullptr && info->ExceptionRecord != nullptr) {
    const EXCEPTION_RECORD *record = info->ExceptionRecord;
    oss << " at " << record->ExceptionAddress;

    if (code == EXCEPTION_ACCESS_VIOLATION &&
        record->NumberParameters >= 2) {
      const ULONG_PTR mode = record->ExceptionInformation[0];
      const ULONG_PTR address = record->ExceptionInformation[1];
      oss << " while "
          << ((mode == 0) ? "reading" : (mode == 1) ? "writing"
                                                  : "executing")
          << " address 0x" << std::hex << std::uppercase << address;
    }
  }

  throw std::runtime_error(oss.str());
}

void installStructuredExceptionTranslator() {
  _set_se_translator(translateStructuredException);
}

void logFatalWithProgress(
    spdlog::level::level_enum level, const std::string &message) {
  auto logger = spdlog::get("prevabs");
  if (logger == nullptr) {
    const std::string progress = formatProgressContext();
    std::cerr << message << std::endl;
    if (!progress.empty()) {
      std::cerr << "progress context: " << progress << std::endl;
    }
    return;
  }

  PLogStream(level, __FILE__, __LINE__, __func__) << message;
  logger->flush();
}

} // namespace
#else
namespace {

void installStructuredExceptionTranslator() {}

void logFatalWithProgress(
    spdlog::level::level_enum level, const std::string &message) {
  auto logger = spdlog::get("prevabs");
  if (logger == nullptr) {
    const std::string progress = formatProgressContext();
    std::cerr << message << std::endl;
    if (!progress.empty()) {
      std::cerr << "progress context: " << progress << std::endl;
    }
    return;
  }

  PLogStream(level, __FILE__, __LINE__, __func__) << message;
  logger->flush();
}

} // namespace
#endif


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
    if (config.debug_level >= DebugLevel::phase)
      config.app.log_level = LOG_LEVEL_DEBUG;

    initLog();
    installStructuredExceptionTranslator();
    PLOG(info) << "PreVABS " VERSION_STRING
                  " (VABS " + vabs_version + ", SwiftComp " + sc_version + ")";
  }
  catch (const CLI::ParseError &e) {
    return app.exit(e);
  }
  catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }


  // -----------------------------------------------------------------

  auto start_s = std::chrono::steady_clock::now();

  auto pmodel_uptr = std::make_unique<PModel>(config.file_base_name);
  bool initialized = false;
  clearProgressContext();

  // Single catch-all: any unhandled exception in any pipeline stage is
  // reported here before cleanup, preventing silent exit or second-crash
  // from downstream stages using a partially-built model.
  try {
    PLogContext initialize_context("initialize model");
    pmodel_uptr->initialize();
    initialized = true;

    std::string s_dt_start = getCurrentDateTimeString();
    PLOG(info) << "prevabs start (" + s_dt_start + ")";

    if (config.isHomo()) {
      PLogContext homogenize_context("homogenization pipeline");
      pmodel_uptr->homogenize();
    } else if (config.isRecovery()) {
      PLogContext dehomogenize_context("dehomogenization pipeline");
      pmodel_uptr->dehomogenize();
    }

    if (config.execute) {
      PLogContext execute_context("execute solver");
      pmodel_uptr->run();
    }

    if (config.plot) {
      PLogContext plot_context("plot outputs");
      pmodel_uptr->plot();
    }

    PLogContext finalize_context("finalize model");
    pmodel_uptr->finalize();
    initialized = false;

    std::string s_dt_finish = getCurrentDateTimeString();
    auto stop_s = std::chrono::steady_clock::now();
    double tt = std::chrono::duration<double>(stop_s - start_s).count();
    std::ostringstream ss;
    ss << "total running time: " << tt << " sec";

    PLOG(info) << "prevabs finished (" + s_dt_finish + ")";
    PLOG(info) << ss.str();
    clearProgressContext();
    return 0;
  }
  catch (const std::exception &e) {
    logFatalWithProgress(
        spdlog::level::err,
        std::string("fatal exception: ") + e.what());
    if (pmodel_uptr && pmodel_uptr->dcel()) {
      try {
        auto dump_path = config.file_directory + config.file_base_name
                         + ".dcel_dump.txt";
        pmodel_uptr->dcel()->dumpToFile(dump_path);
      } catch (...) {}
    }
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    clearProgressContext();
    return 1;
  }
  catch (...) {
    logFatalWithProgress(
        spdlog::level::critical, "unhandled exception");
    if (pmodel_uptr && pmodel_uptr->dcel()) {
      try {
        auto dump_path = config.file_directory + config.file_base_name
                         + ".dcel_dump.txt";
        pmodel_uptr->dcel()->dumpToFile(dump_path);
      } catch (...) {}
    }
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    clearProgressContext();
    return 1;
  }
}
