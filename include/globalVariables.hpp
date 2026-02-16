#pragma once

#include <cstdio>
#include <functional>
#include <string>
// #include <vector>
// #include <CGAL/Cartesian.h>
// #include "internalClasses.hpp"

class PDCEL;
class LayerType;
class Material;
class Message;

extern bool debug;
extern int i_indent;
extern bool scientific_format;

struct PConfig {
  std::string main_input = ""; // ****/****.****
  std::string file_directory = ""; // ****/
  std::string file_base_name = ""; // ****
  std::string file_extension = ""; // .****

  std::string file_name_geo = "";
  std::string file_name_msh = "";
  std::string file_name_opt = "";
  std::string file_name_vsc = "";
  std::string file_name_glb = "";
  std::string file_name_lss_geo = "";
  std::string file_name_lss_msh = "";
  std::string file_name_lss_opt = "";

  std::string file_name_log = "";

  std::string file_name_deb = "";
  FILE *fdeb;

  int analysis_tool = 1; // 1: VABS, 2: SC
  std::string tool_name = "VABS";
  std::string vabs_name = "VABS";
  std::string sc_name = "SwiftComp";
  bool integrated_solver = false;
  std::string tool_ver = "0";  // Version of VABS/SC

  bool homo = false;
  bool dehomo = false;
  bool dehomo_nl = false;
  bool fail_strength = false;
  bool fail_envelope = false;
  bool fail_index = false;
  std::string msg_analysis = "";
  std::string vabs_option = "";
  std::string sc_option = "";

  bool plot = false;
  bool execute = false;

  bool debug = false;
  int log_severity_level = 2;
  int gmsh_verbosity = 2;  // 0=silent,1=errors,2=warnings,3=info,5=debug

  int gmsh_views = 0;

  double tol = 1e-12;
  double geo_tol = 1e-9;
};

extern PConfig config;

// Sub-structs that expose only the config fields each subsystem needs.
// Constructed once from global `config` at the pipeline entry points
// and passed explicitly, so callers can see the actual dependencies.

struct WriterConfig {
  int analysis_tool;         // 1: VABS, 2: SwiftComp
  bool dehomo;               // recovery analysis flag
  std::string tool_ver;      // tool version string (e.g. "2.2")
  std::string file_name_vsc; // path to the .sg file (read-back path)
};

// Interface for material lookup used by domain objects during build.
// PModel implements this; passed via BuilderConfig so domain objects
// have no direct dependency on PModel.
struct IMaterialLookup {
  virtual LayerType* getLayerTypeByMaterialAngle(Material*, double) = 0;
  virtual ~IMaterialLookup() = default;
};

struct BuilderConfig {
  bool debug;                // emit debug geometry plots
  int analysis_tool;         // 1: VABS, 2: SwiftComp
  double tol;                // intersection parameter tolerance
  double geo_tol;            // geometric edge-length tolerance
  PDCEL* dcel = nullptr;     // shared DCEL; replaces _pmodel->dcel()
  IMaterialLookup* materials = nullptr;  // replaces _pmodel->getLayerTypeByMaterialAngle()
  std::function<void(Message*)> plotDebug; // replaces _pmodel->plotGeoDebug()
};

