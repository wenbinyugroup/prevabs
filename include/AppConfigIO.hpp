#pragma once

// AppConfigIO — multi-level JSON configuration file loading for prevabs.
//
// Load priority (lowest → highest, later values win):
//   1. Built-in defaults     (AppConfig struct member initializers)
//   2. Executable-dir config <exe-dir>/prevabs.json
//   3. User config           ~/.prevabs.json                   (home directory)
//   4. Project config        <input-file-dir>/.prevabs.json
//   5. --config <path>       explicit file (appended as highest file layer)
//   6. CLI flags             --gmsh-verbosity, etc.            (applied last in main.cpp)
//
// Levels 2–4 are loaded by loadAppConfig; level 5 (--config) and level 6
// (CLI flags) are applied afterwards in main.cpp.
//
// Only AppConfig fields (numerics/output/tools/paths/gmsh_opt sections) are
// read from files. Session options (mode, tool, execute, plot, debug) are
// always CLI-only.
//
// JSON schema example:
//   {
//     "numerics": { "tol": 1e-12, "geo_tol": 1e-9 },
//     "output":   { "log_level": 2, "gmsh_verbosity": 2 },
//     "tools":    { "vabs": "VABS", "swiftcomp": "SwiftComp", "gmsh": "gmsh" },
//     "paths":    { "material_db": "" },
//     "gmsh_opt": { "template_file": "" }
//   }

#include "globalVariables.hpp"
#include "nlohmann/json.hpp"

#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/types.h>
#  include <pwd.h>
#  include <limits.h>
#  ifdef __APPLE__
#    include <mach-o/dyld.h>
#  endif
#endif

namespace prevabs {

// Apply one JSON object to an AppConfig, leaving unmentioned fields unchanged.
inline void applyJson(AppConfig &cfg, const nlohmann::json &j) {
  if (j.contains("numerics")) {
    const auto &n = j.at("numerics");
    if (n.contains("tol"))     cfg.tol     = n.at("tol").get<double>();
    if (n.contains("geo_tol")) cfg.geo_tol = n.at("geo_tol").get<double>();
  }
  if (j.contains("output")) {
    const auto &o = j.at("output");
    if (o.contains("log_level"))      cfg.log_level      = o.at("log_level").get<int>();
    if (o.contains("gmsh_verbosity")) cfg.gmsh_verbosity = o.at("gmsh_verbosity").get<int>();
  }
  if (j.contains("tools")) {
    const auto &t = j.at("tools");
    if (t.contains("vabs"))      cfg.tools.vabs      = t.at("vabs").get<std::string>();
    if (t.contains("swiftcomp")) cfg.tools.swiftcomp = t.at("swiftcomp").get<std::string>();
    if (t.contains("gmsh"))      cfg.tools.gmsh      = t.at("gmsh").get<std::string>();
  }
  if (j.contains("paths")) {
    const auto &p = j.at("paths");
    if (p.contains("material_db"))
      cfg.paths.material_db = p.at("material_db").get<std::string>();
  }
  // "gmsh" section: raw Gmsh .opt options grouped into sub-sections
  // ("general" / "homogenization" / "recovery"). Each option is merged per-key
  // onto lower config levels; the value is stored as its JSON text (numbers ->
  // "0"/"1.5", strings -> "\"text\"") so it can be written verbatim into the
  // .opt file. PreVABS does not validate the option names or values.
  if (j.contains("gmsh")) {
    const auto &g = j.at("gmsh");
    for (auto sit = g.begin(); sit != g.end(); ++sit) {
      if (!sit.value().is_object()) continue;   // skip non-section entries
      auto &sec = cfg.gmsh[sit.key()];
      for (auto it = sit.value().begin(); it != sit.value().end(); ++it)
        sec[it.key()] = it.value().dump();
    }
  }
}

// Outcome of attempting to load one config file.
enum class ConfigFileStatus {
  NotFound,    // file does not exist / cannot be opened
  Loaded,      // opened and parsed successfully
  ParseError   // file exists but is not valid JSON
};

// Try to load and apply a JSON config file.
// A missing file is not an error (NotFound); a malformed file is reported as
// ParseError so the caller can warn the user (config values are unchanged).
inline ConfigFileStatus tryLoadConfigFile(AppConfig &cfg, const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) return ConfigFileStatus::NotFound;
  try {
    nlohmann::json j;
    f >> j;
    applyJson(cfg, j);
    return ConfigFileStatus::Loaded;
  } catch (...) {
    return ConfigFileStatus::ParseError;
  }
}

// Directory containing the running executable, with a trailing separator.
// Returns "" if it cannot be determined.
inline std::string executableDir() {
#ifdef _WIN32
  char buf[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return "";
  std::string p(buf, n);
  std::size_t slash = p.find_last_of("\\/");
  return (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
#elif defined(__APPLE__)
  char buf[PATH_MAX] = {};
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) != 0) return "";
  std::string p(buf);
  std::size_t slash = p.find_last_of('/');
  return (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
#else
  char buf[PATH_MAX] = {};
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return "";
  buf[n] = '\0';
  std::string p(buf);
  std::size_t slash = p.find_last_of('/');
  return (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
#endif
}

// Level 2: <exe-dir>/prevabs.json  (empty if exe dir is unknown).
inline std::string exeDirConfigPath() {
  std::string dir = executableDir();
  return dir.empty() ? "" : dir + "prevabs.json";
}

// Level 3: user home config  ~/.prevabs.json  (empty if home is unknown).
inline std::string userConfigPath() {
#ifdef _WIN32
  char buf[MAX_PATH] = {};
  DWORD n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return "";
  return std::string(buf) + "\\.prevabs.json";
#else
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) home = pw->pw_dir;
  }
  if (!home) return "";
  return std::string(home) + "/.prevabs.json";
#endif
}

// Load the auto-discovered config levels (2–4) into cfg, in priority order.
// project_dir is the directory containing the input file (may be empty).
// Returns the paths of files that existed but failed to parse, so the caller
// can warn once logging is initialised. (The --config file, level 5, is
// applied separately in main.cpp.)
inline std::vector<std::string> loadAppConfig(
    AppConfig &cfg, const std::string &project_dir) {
  std::vector<std::string> parse_errors;
  auto tryLevel = [&](const std::string &path) {
    if (path.empty()) return;
    if (tryLoadConfigFile(cfg, path) == ConfigFileStatus::ParseError)
      parse_errors.push_back(path);
  };

  tryLevel(exeDirConfigPath());                 // Level 2: executable dir
  tryLevel(userConfigPath());                   // Level 3: user home
  if (!project_dir.empty())                     // Level 4: input-file dir
    tryLevel(project_dir + ".prevabs.json");

  return parse_errors;
}

// Serialize AppConfig to JSON (useful for --print-config or generating a template).
inline nlohmann::json toJson(const AppConfig &cfg) {
  nlohmann::json j = {
    {"numerics", {
      {"tol",     cfg.tol},
      {"geo_tol", cfg.geo_tol}
    }},
    {"output", {
      {"log_level",      cfg.log_level},
      {"gmsh_verbosity", cfg.gmsh_verbosity}
    }},
    {"tools", {
      {"vabs",      cfg.tools.vabs},
      {"swiftcomp", cfg.tools.swiftcomp},
      {"gmsh",      cfg.tools.gmsh}
    }},
    {"paths", {
      {"material_db", cfg.paths.material_db}
    }}
  };
  // Rebuild the gmsh sections, parsing each stored value back to JSON.
  nlohmann::json gmsh = nlohmann::json::object();
  for (const auto &section : cfg.gmsh) {
    nlohmann::json sec = nlohmann::json::object();
    for (const auto &kv : section.second)
      sec[kv.first] = nlohmann::json::parse(kv.second);
    gmsh[section.first] = sec;
  }
  j["gmsh"] = gmsh;
  return j;
}

} // namespace prevabs
