#pragma once

// AppConfigIO — multi-level JSON configuration file loading for prevabs.
//
// Load priority (lowest → highest, later values win):
//   1. Built-in defaults     (AppConfig struct member initializers)
//   2. System config         /etc/prevabs/prevabs.json          (Linux/macOS only)
//   3. User config           ~/.config/prevabs/prevabs.json  (Linux/macOS)
//                            %APPDATA%/prevabs/prevabs.json  (Windows)
//   4. Project config        <input-file-dir>/.prevabs.json
//   5. CLI flags             --gmsh-verbosity, etc.           (applied last in main.cpp)
//
// Only AppConfig fields (tol, geo_tol, log_level, gmsh_verbosity) are
// read from files. Session options (mode, tool, execute, plot, debug)
// are always CLI-only.
//
// JSON schema example:
//   {
//     "numerics": {
//       "tol":     1e-12,
//       "geo_tol": 1e-9
//     },
//     "output": {
//       "log_level":       2,
//       "gmsh_verbosity":  2
//     }
//   }

#include "globalVariables.hpp"
#include "nlohmann/json.hpp"

#include <fstream>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/types.h>
#  include <pwd.h>
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
}

// Try to load and apply a JSON config file.
// Returns true if the file was found and parsed successfully; false otherwise.
// Parse errors are silently ignored (malformed file = treated as absent).
inline bool tryLoadConfigFile(AppConfig &cfg, const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) return false;
  try {
    nlohmann::json j;
    f >> j;
    applyJson(cfg, j);
    return true;
  } catch (...) {
    return false;
  }
}

// Return the user-level config directory:
//   Windows : %APPDATA%\prevabs\prevabs.json
//   POSIX   : ~/.config/prevabs/prevabs.json
inline std::string userConfigPath() {
#ifdef _WIN32
  char buf[MAX_PATH] = {};
  DWORD n = GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return "";
  return std::string(buf) + "\prevabs\prevabs.json";
#else
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) home = pw->pw_dir;
  }
  if (!home) return "";
  return std::string(home) + "/.config/prevabs/prevabs.json";
#endif
}

// Load all config levels into cfg, in priority order.
// project_dir should be the directory containing the input file (may be empty).
inline void loadAppConfig(AppConfig &cfg, const std::string &project_dir) {
  // Level 2: system-wide (POSIX only)
#ifndef _WIN32
  tryLoadConfigFile(cfg, "/etc/prevabs/prevabs.json");
#endif

  // Level 3: user-level
  std::string upath = userConfigPath();
  if (!upath.empty()) tryLoadConfigFile(cfg, upath);

  // Level 4: project-level (directory of the input file)
  if (!project_dir.empty()) {
    std::string ppath = project_dir + ".prevabs.json";
    tryLoadConfigFile(cfg, ppath);
  }
}

// Serialize AppConfig to JSON (useful for --print-config or generating a template).
inline nlohmann::json toJson(const AppConfig &cfg) {
  return {
    {"numerics", {
      {"tol",     cfg.tol},
      {"geo_tol", cfg.geo_tol}
    }},
    {"output", {
      {"log_level",      cfg.log_level},
      {"gmsh_verbosity", cfg.gmsh_verbosity}
    }}
  };
}

} // namespace prevabs
