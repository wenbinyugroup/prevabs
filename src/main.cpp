#include "PModel.hpp"
#include "PModelIO.hpp"
#include "AppConfigIO.hpp"
#include "execu.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "adaptive_thickness.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "pui.hpp"
#include "version.h"

#include "CLI11.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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

// Adds blank lines between option groups and wraps long descriptions.
class GroupedFormatter : public CLI::Formatter {
  static constexpr std::size_t kTotalWidth = 80;

  // Word-wraps text to at most `width` characters per line.
  static std::string wordWrap(const std::string &text, std::size_t width) {
    if (text.size() <= width) return text;
    std::string result;
    std::istringstream iss(text);
    std::string word;
    std::size_t line_len = 0;
    while (iss >> word) {
      if (line_len > 0) {
        if (line_len + 1 + word.size() > width) {
          result += '\n';
          line_len = 0;
        } else {
          result += ' ';
          ++line_len;
        }
      }
      result += word;
      line_len += word.size();
    }
    return result;
  }

public:
  std::string make_expanded(const CLI::App *sub) const override {
    return "\n" + CLI::Formatter::make_expanded(sub);
  }

  std::string make_option(const CLI::Option *opt, bool is_positional) const override {
    std::stringstream out;
    std::size_t col = get_column_width();
    std::size_t desc_width = kTotalWidth > col ? kTotalWidth - col : 40u;
    CLI::detail::format_help(
        out,
        make_option_name(opt, is_positional) + make_option_opts(opt),
        wordWrap(make_option_desc(opt), desc_width),
        col);
    return out.str();
  }
};

void addParserArguments(CLI::App &app) {
  app.formatter(std::make_shared<GroupedFormatter>());
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
  auto *exec_group = app.add_option_group("Execution options");
  exec_group->add_option("--ver", config.tool_ver, "Tool version (e.g. 3.0)");
  exec_group->add_flag("--integrated", config.integrated_solver,
    "Use integrated solver (implies --execute)")->default_val(false);
  exec_group->add_flag("-e,--execute", config.execute,
    "Execute VABS/SwiftComp after generating input")->default_val(false);

  // Output / visualisation options
  auto *out_group = app.add_option_group("Output options");
  out_group->add_flag("-v,--visualize", config.plot,
    "Visualize meshed cross section or contour plots")->default_val(false);
  out_group->add_flag("--nopopup", config.no_popup,
    "Do not open the Gmsh GUI window (same as gmsh -nopopup); "
    "geo/msh files are still written when -v is given")->default_val(false);
  out_group->add_option("--gmsh-verbosity", config.app.gmsh_verbosity,
    "Gmsh log verbosity (0=silent, 1=errors, 2=warnings, 3=info, 5=debug)"
  )->default_val(2)->check(CLI::IsMember({0, 1, 2, 3, 5}));

  // Developer options
  auto *dev_group = app.add_option_group("Developer options");
  dev_group->add_option("-d,--debug", debug_level_str,
                 "Debug verbosity: summary|phase|join|geo|all; "
                 "plain -d/--debug defaults to 'join'")
     ->expected(0, 1)
     ->default_str("join")  // value used when --debug given with no argument
     ->check(CLI::IsMember({"", "summary", "phase", "join", "geo", "all"}));
  dev_group->add_option(
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
  app.get_formatter()->column_width(30);
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

void emitRunCompletion(
    const std::chrono::steady_clock::time_point &start_s) {
  std::string s_dt_finish = getCurrentDateTimeString();
  auto stop_s = std::chrono::steady_clock::now();
  double tt = std::chrono::duration<double>(stop_s - start_s).count();
  std::ostringstream ss;
  ss << "total running time: " << tt << " sec";

  pui::success("prevabs finished (" + s_dt_finish + ")");
  pui::info(ss.str());
}

namespace {

bool containsAny(
    const std::string &line,
    const std::vector<std::string> &needles) {
  for (const auto &needle : needles) {
    if (line.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> collectFailureEvidence(
    const std::string &path,
    std::size_t max_lines) {
  std::vector<std::string> lines;
  if (path.empty()) return lines;

  std::ifstream in(path.c_str());
  if (!in) return lines;

  const std::vector<std::string> needles{
      "Unable to recover the edge",
      "recover the edge",
      "fatal exception",
      "unhandled exception",
      "offset (multi-vertex",
      "only ",
      "skin dropped",
      "dropped range",
      "dropped ranges",
      "Stage E pre-trim",
      "splitFace",
      "splitEdge",
      "repeated",
      "gmsh"};

  std::string line;
  while (std::getline(in, line)) {
    if (!containsAny(line, needles)) continue;
    lines.push_back(line);
    if (lines.size() >= max_lines) break;
  }
  return lines;
}

bool fileExists(const std::string &path) {
  if (path.empty()) return false;
  std::ifstream in(path.c_str());
  return static_cast<bool>(in);
}

struct AdaptiveThicknessSuggestion {
  bool found = false;
  std::string segment_name;
  int pretrim_lo = -1;
  int pretrim_hi = -1;
  int dropped_lo = -1;
  int dropped_hi = -1;
  int first_thin_base = -1;
  double offset_distance = 0.0;
  double design_half_thickness = 0.0;
  int offset_vertices = -1;
  int base_vertices = -1;
  double ratio = 0.0;
};

std::string joinStrings(
    const std::vector<std::string> &values,
    const std::string &sep) {
  std::ostringstream ss;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) ss << sep;
    ss << values[i];
  }
  return ss.str();
}

bool parseFirstMatch(
    const std::string &line,
    const std::regex &pattern,
    std::smatch &match) {
  return std::regex_search(line, match, pattern);
}

AdaptiveThicknessSuggestion collectAdaptiveThicknessSuggestion(
    const std::string &path) {
  AdaptiveThicknessSuggestion suggestion;
  if (path.empty()) return suggestion;

  std::ifstream in(path.c_str());
  if (!in) return suggestion;

  const std::regex segment_re(
      "offsetting the base curve of segment: ([^ ]+)");
  const std::regex thin_re(
      "local half-thickness < \\|dist\\| = ([0-9eE+\\.-]+).*"
      "first at base\\[([0-9]+)\\]");
  const std::regex pretrim_re(
      "Stage E pre-trim removed base\\[([0-9]+)\\.\\.([0-9]+)\\]");
  const std::regex dropped_re(
      "skin dropped over base indices \\[([0-9]+)\\.\\.([0-9]+)\\]");
  const std::regex ratio_re(
      "only ([0-9]+) offset verts produced for ([0-9]+) base verts "
      "\\(M/N=([0-9eE+\\.-]+)\\).*layup half-thickness "
      "([0-9eE+\\.-]+)");

  std::string active_segment;
  std::string line;
  while (std::getline(in, line)) {
    std::smatch match;
    if (parseFirstMatch(line, segment_re, match)) {
      active_segment = match[1].str();
      continue;
    }
    if (parseFirstMatch(line, thin_re, match)) {
      suggestion.found = true;
      suggestion.segment_name = active_segment;
      suggestion.offset_distance = std::stod(match[1].str());
      suggestion.first_thin_base = std::stoi(match[2].str());
      continue;
    }
    if (parseFirstMatch(line, pretrim_re, match)) {
      suggestion.found = true;
      suggestion.segment_name = active_segment;
      suggestion.pretrim_lo = std::stoi(match[1].str());
      suggestion.pretrim_hi = std::stoi(match[2].str());
      continue;
    }
    if (parseFirstMatch(line, dropped_re, match)) {
      suggestion.found = true;
      suggestion.segment_name = active_segment;
      suggestion.dropped_lo = std::stoi(match[1].str());
      suggestion.dropped_hi = std::stoi(match[2].str());
      continue;
    }
    if (parseFirstMatch(line, ratio_re, match)) {
      suggestion.found = true;
      suggestion.segment_name = active_segment;
      suggestion.offset_vertices = std::stoi(match[1].str());
      suggestion.base_vertices = std::stoi(match[2].str());
      suggestion.ratio = std::stod(match[3].str());
      suggestion.design_half_thickness = std::stod(match[4].str());
      continue;
    }
  }
  return suggestion;
}

void writeAdaptiveThicknessSuggestion(std::ostream &out) {
  const auto suggestion =
      collectAdaptiveThicknessSuggestion(config.file_name_log_dev);
  if (!suggestion.found && !config.adaptive_thickness.enabled) return;

  const int repair_lo =
      (suggestion.dropped_lo >= 0) ? suggestion.dropped_lo
                                  : suggestion.pretrim_lo;
  const int repair_hi =
      (suggestion.dropped_hi >= 0) ? suggestion.dropped_hi
                                  : suggestion.pretrim_hi;
  const double safety = config.adaptive_thickness.enabled
      ? config.adaptive_thickness.safety : 0.90;
  const int transition_base_count = config.adaptive_thickness.enabled
      ? config.adaptive_thickness.transition_base_count : 2;
  const int repair_base_padding = config.adaptive_thickness.enabled
      ? config.adaptive_thickness.repair_base_padding : 0;
  const double design_half_thickness =
      (suggestion.design_half_thickness > 0.0)
      ? suggestion.design_half_thickness : suggestion.offset_distance;

  prevabs::geo::LinearAdaptiveThicknessInput plan_input;
  plan_input.segment_name = suggestion.segment_name;
  plan_input.base_count = suggestion.base_vertices;
  plan_input.repair_lo = repair_lo;
  plan_input.repair_hi = repair_hi;
  plan_input.design_half_thickness = design_half_thickness;
  plan_input.safety = safety;
  plan_input.repair_base_padding = repair_base_padding;
  plan_input.transition_base_count = transition_base_count;
  plan_input.min_half_thickness =
      config.adaptive_thickness.min_half_thickness;
  const auto plan =
      prevabs::geo::buildLinearAdaptiveThicknessPlan(plan_input);

  out << "## Adaptive thickness suggestion\n\n";
  if (config.adaptive_thickness.enabled) {
    if (config.adaptive_thickness.report_only) {
      out << "Adaptive thickness is configured in report-only mode, so the "
             "model was not modified.\n\n";
    } else {
      out << "Adaptive thickness is configured for matching open segments. "
             "This report was written because the build still failed.\n\n";
    }
    out << "- Configured mode: `" << config.adaptive_thickness.mode << "`\n";
    out << "- Configured report_only: "
        << (config.adaptive_thickness.report_only ? "true" : "false")
        << "\n";
    out << "- Configured safety: " << config.adaptive_thickness.safety
        << "\n";
    out << "- Configured repair_base_padding: "
        << config.adaptive_thickness.repair_base_padding << "\n";
    out << "- Configured transition_base_count: "
        << config.adaptive_thickness.transition_base_count << "\n";
    out << "- Configured min_half_thickness: "
        << config.adaptive_thickness.min_half_thickness << "\n";
    if (!config.adaptive_thickness.target_segments.empty()) {
      out << "- Configured target_segments: `"
          << joinStrings(config.adaptive_thickness.target_segments, ", ")
          << "`\n";
    }
    if (!suggestion.found) {
      out << "\nNo offset-thickness diagnostic was found in the debug log, "
             "so no repair range could be suggested.\n\n";
      return;
    }
  } else {
    out << "This is a dry-run suggestion only. The model was not modified.\n\n";
    out << "- Suggested mode: `linear`\n";
  }
  if (!suggestion.segment_name.empty()) {
    out << "- Segment: `" << suggestion.segment_name << "`\n";
  }
  if (!plan.ok) {
    out << "- Adaptive thickness plan: unavailable (" << plan.error
        << ")\n";
  } else {
    out << "- Repair range: base[" << plan.range.repair_lo << ".."
        << plan.range.repair_hi << "]\n";
    out << "- Suggested taper range: base[" << plan.range.taper_lo << ".."
        << plan.range.taper_hi << "]\n";
  }
  if (suggestion.pretrim_lo >= 0 && suggestion.pretrim_hi >= 0) {
    out << "- Stage E pre-trim range: base[" << suggestion.pretrim_lo
        << ".." << suggestion.pretrim_hi << "]\n";
  }
  if (suggestion.first_thin_base >= 0) {
    out << "- First thin base index: base[" << suggestion.first_thin_base
        << "]\n";
  }
  if (suggestion.design_half_thickness > 0.0) {
    out << "- Design half-thickness: " << suggestion.design_half_thickness
        << "\n";
  }
  if (suggestion.offset_distance > 0.0) {
    out << "- Current offset distance: " << suggestion.offset_distance
        << "\n";
  }
  if (plan.ok && plan.range.repaired_half_thickness > 0.0) {
    out << "- Initial trial local half-thickness `t'`: "
        << plan.range.repaired_half_thickness
        << " (safety = " << safety << ")\n";
  }
  if (suggestion.offset_vertices >= 0 && suggestion.base_vertices > 0) {
    out << "- Offset/base vertex ratio: " << suggestion.offset_vertices
        << "/" << suggestion.base_vertices << " = "
        << suggestion.ratio << "\n";
  }
  out << "\n";
  out << "Use this as the audit trail for the opt-in adaptive thickness "
         "repair. A stricter automatic value still requires logging the "
         "minimum local available half-thickness in the failed range.\n\n";
}

std::string classifyFailure(const std::string &fatal_msg) {
  if (fatal_msg.find("Unable to recover the edge") != std::string::npos ||
      fatal_msg.find("recover the edge") != std::string::npos) {
    return "Gmsh could not recover one generated mesh edge. This usually "
           "means the generated laminate face is too thin, self-intersecting, "
           "or topologically inconsistent near a local offset/join region.";
  }
  if (fatal_msg.find("SEH exception") != std::string::npos) {
    return "PreVABS hit a low-level runtime exception while building the "
           "cross-section geometry.";
  }
  return "PreVABS stopped while building the cross-section. See the fatal "
         "message and nearby diagnostic log lines below.";
}

std::string writeFailureReport(const std::string &fatal_msg) {
  if (config.file_directory.empty() || config.file_base_name.empty()) {
    return "";
  }

  const std::string report_path =
      config.file_directory + config.file_base_name + ".failure.md";
  const std::string dump_path =
      config.file_directory + config.file_base_name + ".dcel_dump.txt";
  const std::string progress = formatProgressContext();

  std::ofstream out(report_path.c_str(), std::ios::out | std::ios::trunc);
  if (!out) return "";

  out << "# PreVABS build failure report\n\n";
  out << "- Input: `" << config.main_input << "`\n";
  out << "- Case: `" << config.file_base_name << "`\n";
  out << "- Time: " << getCurrentDateTimeString() << "\n";
  out << "- Failure: " << fatal_msg << "\n";
  if (!progress.empty()) {
    out << "- Catch context: " << progress << "\n";
  }
  out << "\n";

  out << "## What failed\n\n";
  out << classifyFailure(fatal_msg) << "\n\n";

  out << "## Diagnostic evidence\n\n";
  bool wrote_evidence = false;
  const std::vector<std::pair<std::string, std::string> > logs{
      std::make_pair("user log", config.file_name_log),
      std::make_pair("debug log", config.file_name_log_dev)};
  for (const auto &log : logs) {
    const auto lines = collectFailureEvidence(log.second, 10);
    if (lines.empty()) continue;
    wrote_evidence = true;
    out << "From `" << log.second << "`:\n\n";
    for (const auto &line : lines) {
      out << "- " << line << "\n";
    }
    out << "\n";
  }
  if (!wrote_evidence) {
    out << "No matching diagnostic lines were found in the logs. Check the "
           "full log files listed below.\n\n";
  }

  writeAdaptiveThicknessSuggestion(out);

  out << "## Related files\n\n";
  out << "- Log: `" << config.file_name_log << "`\n";
  out << "- Debug log: `" << config.file_name_log_dev << "`\n";
  if (fileExists(dump_path)) {
    out << "- DCEL dump: `" << dump_path << "`\n";
  }
  out << "\n";

  out << "## User action\n\n";
  out << "The build was stopped instead of silently dropping local faces or "
         "elements. Inspect the reported segment/offset region and adjust the "
         "cross-section geometry or layup so the local laminate faces remain "
         "meshable and connected.\n";

  return report_path;
}

void writeFailureReportBestEffort(const std::string &fatal_msg) {
  try {
    const std::string report_path = writeFailureReport(fatal_msg);
    if (!report_path.empty()) {
      pui::warn("failure report written: " + report_path);
    }
  } catch (...) {}
}

} // namespace

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
  if (!hasPrevabsLogger()) {
    const std::string progress = formatProgressContext();
    std::cerr << message << std::endl;
    if (!progress.empty()) {
      std::cerr << "progress context: " << progress << std::endl;
    }
    return;
  }

  PLogStream(level, __FILE__, __LINE__, __func__) << message;
  flushPrevabsLoggers();
}

} // namespace
#else
namespace {

void installStructuredExceptionTranslator() {}

void logFatalWithProgress(
    spdlog::level::level_enum level, const std::string &message) {
  if (!hasPrevabsLogger()) {
    const std::string progress = formatProgressContext();
    std::cerr << message << std::endl;
    if (!progress.empty()) {
      std::cerr << "progress context: " << progress << std::endl;
    }
    return;
  }

  PLogStream(level, __FILE__, __LINE__, __func__) << message;
  flushPrevabsLoggers();
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
    pui::init(config.file_name_log);
    installStructuredExceptionTranslator();
    // pui::title(std::string("PreVABS ") + VERSION_STRING +
    //            " (VABS " + vabs_version + ", SwiftComp " + sc_version + ")");
    pui::title(std::string("PreVABS ") + VERSION_STRING);
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
  bool completion_reported = false;
  clearProgressContext();

  // Single catch-all: any unhandled exception in any pipeline stage is
  // reported here before cleanup, preventing silent exit or second-crash
  // from downstream stages using a partially-built model.
  try {
    PLogContext initialize_context("initialize model");
    pmodel_uptr->initialize();
    initialized = true;

    std::string s_dt_start = getCurrentDateTimeString();
    pui::info("prevabs start (" + s_dt_start + ")");

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
      pmodel_uptr->plot([&]() {
        emitRunCompletion(start_s);
        completion_reported = true;
        pui::info("launching Gmsh GUI; close the window to let prevabs exit");
      });
    }

    PLogContext finalize_context("finalize model");
    pmodel_uptr->finalize();
    initialized = false;

    if (!completion_reported) {
      emitRunCompletion(start_s);
    }
    clearProgressContext();
    return 0;
  }
  catch (const std::exception &e) {
    const std::string fatal_msg = std::string("fatal exception: ") + e.what();
    pui::error(fatal_msg);
    logFatalWithProgress(spdlog::level::err, fatal_msg);
    if (pmodel_uptr && pmodel_uptr->dcel()) {
      try {
        auto dump_path = config.file_directory + config.file_base_name
                         + ".dcel_dump.txt";
        pmodel_uptr->dcel()->dumpToFile(dump_path);
      } catch (...) {}
    }
    writeFailureReportBestEffort(fatal_msg);
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    clearProgressContext();
    return 1;
  }
  catch (...) {
    const std::string fatal_msg = "unhandled exception";
    pui::error(fatal_msg);
    logFatalWithProgress(
        spdlog::level::critical, fatal_msg);
    if (pmodel_uptr && pmodel_uptr->dcel()) {
      try {
        auto dump_path = config.file_directory + config.file_base_name
                         + ".dcel_dump.txt";
        pmodel_uptr->dcel()->dumpToFile(dump_path);
      } catch (...) {}
    }
    writeFailureReportBestEffort(fatal_msg);
    if (initialized) {
      try { pmodel_uptr->finalize(); } catch (...) {}
    }
    clearProgressContext();
    return 1;
  }
}
