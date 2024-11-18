#pragma once

#include <cstdio>
#include <string>
// #include <vector>
// #include <CGAL/Cartesian.h>
// #include "internalClasses.hpp"

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

  int gmsh_views = 0;

  double tol = 1e-12;
  double geo_tol = 1e-9;
};

extern PConfig config;



extern std::string log_severity_level;
