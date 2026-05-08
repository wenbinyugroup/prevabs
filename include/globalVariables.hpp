#pragma once

#include "globalConstants.hpp"

#include <cstdio>
#include <string>

class PDCEL;
class LayerType;
class Material;
class PModel;

extern bool scientific_format;

// ---------------------------------------------------------------------------
// AppConfig — persistent, user-tunable settings.
// These are the only fields that belong in a prevabs.json config file.
// ---------------------------------------------------------------------------
struct AppConfig {
  double tol            = 1e-12;          // intersection parameter tolerance
  double geo_tol        = 1e-9;           // geometric edge-length tolerance
  int    log_level      = LOG_LEVEL_INFO;  // 0=trace…5=fatal (globalConstants.hpp)
  int    gmsh_verbosity = 2;              // 0=silent,1=errors,2=warnings,3=info,5=debug
  // External solver timeout in seconds. 0 = no timeout (default).
  // Set via prevabs.json to enable; CLI is unchanged.
  int    solver_timeout_s = 0;
};

// ---------------------------------------------------------------------------
// PConfig — full session configuration (CLI options + derived paths + AppConfig).
// Set once during startup; treated as read-only after processConfigVariables().
// ---------------------------------------------------------------------------
struct PConfig {
  // --- Input ---
  std::string main_input = "";   // full path as given on CLI

  // --- Derived file paths (set by processConfigVariables, never user-settable) ---
  std::string file_directory    = "";
  std::string file_base_name    = "";
  std::string file_extension    = "";
  std::string file_name_geo     = "";
  std::string file_name_msh     = "";
  std::string file_name_opt     = "";
  std::string file_name_vsc     = "";
  std::string file_name_glb     = "";
  std::string file_name_lss_geo = "";
  std::string file_name_lss_msh = "";
  std::string file_name_lss_opt = "";
  std::string file_name_log     = "";
  std::string file_name_log_dev = "";
  std::string file_name_deb     = "";

  // --- Tool selection ---
  AnalysisTool tool = AnalysisTool::VABS;
  bool integrated_solver = false;
  std::string tool_ver = "0";   // version string passed to the solver

  // --- Analysis mode ---
  AnalysisMode mode = AnalysisMode::Homogenization;

  // --- Execution / output ---
  bool execute   = false;
  bool plot      = false;
  bool no_popup  = false;   // suppress Gmsh FLTK window when -v is used
  DebugLevel debug_level = DebugLevel::off;
  SnapshotMode snapshot_mode = SnapshotMode::never;

  // --- Persistent numeric/output settings (may be overridden by config file) ---
  AppConfig app;

  // --- Derived display/option strings (set by processConfigVariables) ---
  std::string tool_name    = "VABS";
  std::string vabs_name    = "VABS";
  std::string sc_name      = "SwiftComp";
  std::string msg_analysis = "";
  std::string vabs_option  = "";
  std::string sc_option    = "";

  // --- Convenience accessors (replace the old per-mode bool fields) ---
  bool isHomo()         const { return mode == AnalysisMode::Homogenization; }
  bool isDehomo()       const { return mode == AnalysisMode::Dehomogenization ||
                                       mode == AnalysisMode::DehomogenizationNL; }
  bool isFailStrength() const { return mode == AnalysisMode::FailureStrength; }
  bool isFailEnvelope() const { return mode == AnalysisMode::FailureEnvelope; }
  bool isFailIndex()    const { return mode == AnalysisMode::FailureIndex; }
  bool isRecovery()     const { return isRecoveryMode(mode); }
  bool isFailure()      const { return isFailureMode(mode); }
  bool isVABS()         const { return tool == AnalysisTool::VABS; }
  bool isSC()           const { return tool == AnalysisTool::SwiftComp; }
};

extern PConfig config;

// ---------------------------------------------------------------------------
// RuntimeState — mutable counters and handles that are NOT configuration.
// Managed by PModel; not serialized or set via CLI.
// ---------------------------------------------------------------------------
struct RuntimeState {
  int   gmsh_views = 0;
  FILE* fdeb       = nullptr;
};

extern RuntimeState runtime;

// ---------------------------------------------------------------------------
// Sub-structs passed explicitly to subsystems (no global dependency needed)
// ---------------------------------------------------------------------------

struct WriterConfig {
  AnalysisTool tool;
  bool         dehomo;        // recovery analysis flag
  std::string  tool_ver;      // tool version string (e.g. "2.2")
  std::string  file_name_vsc; // path to the .sg file
};

// Interface for material lookup used by domain objects during build.
struct IMaterialLookup {
  virtual LayerType* getLayerTypeByMaterialAngle(Material*, double) = 0;
  virtual ~IMaterialLookup() = default;
};

struct BuilderConfig {
  DebugLevel   debug_level = DebugLevel::off;
  AnalysisTool tool;
  double       tol;
  double       geo_tol;
  PDCEL*           dcel      = nullptr;
  IMaterialLookup* materials = nullptr;
  PModel*          model     = nullptr;

  BuilderConfig() = default;
  BuilderConfig(DebugLevel d, AnalysisTool t, double to, double gt,
                PDCEL* dcel_ptr, PModel* m)
      : debug_level(d), tool(t), tol(to), geo_tol(gt),
        dcel(dcel_ptr), model(m) {}
};
