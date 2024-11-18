#include "PModel.hpp"

#include "globalVariables.hpp"
#include "utilities.hpp"

#include "gmsh/StringUtils.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <vector>


int PModel::postSCDehomo() {
  i_indent++;

  std::vector<std::string> v_inputsc{SplitFileName(config.file_name_vsc)};
  std::string outputsn{config.file_name_vsc + ".sn"};
  std::string outputsnm{config.file_name_vsc + ".snm"};
  config.file_name_geo = outputsn + ".geo";
  config.file_name_msh = outputsn + ".msh";

  std::vector<int> v_i_nodeid{};
  std::vector<std::vector<double>> vv_d_nodecoord{};
  std::vector<std::vector<int>> vv_i_elemconnt{};

  // -----------------------------------------------------------------
  // Read SwiftComp input .sc
  std::ifstream ifs;
  ifs.open(config.file_name_vsc);
  if (ifs.fail())
    throw std::runtime_error("Unable to find the file: " + config.file_name_vsc);
  printInfo(i_indent, "reading SwiftComp input data: " + config.file_name_vsc);
  int nnode, nelem, nsg;
  int ln{1}; // line counter
  while (ifs) {
    std::string line;
    getline(ifs, line);
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);
    if (line.empty())
      continue;
    if (ln == 5) {
      // read out number of nodes and number of elements
      std::stringstream ss;
      ss << line;
      ss >> nsg >> nnode >> nelem;
    } else if (ln > 5 && ln <= (5 + nnode)) {
      // read the node block
      std::stringstream ss;
      int nid;
      double nx2, nx3;
      ss << line;
      ss >> nid >> nx2 >> nx3;
      v_i_nodeid.push_back(nid);
      std::vector<double> ncoords{0.0, nx2, nx3};
      vv_d_nodecoord.push_back(ncoords);
    } else if (ln > (5 + nnode) && ln <= (5 + nnode + nelem)) {
      // read the element block
      std::stringstream ss;
      std::vector<int> econnt{};
      int ei, lt;
      ss << line;
      ss >> ei >> lt; // extract the element label and layer type id
      econnt.push_back(ei);
      while (!ss.eof()) {
        // extract all nodes belonging to this element
        int n;
        ss >> n;
        if (n != 0)
          econnt.push_back(n);
      }
      std::vector<int>::iterator it{econnt.begin()};
      econnt.insert(it + 1, econnt.size() - 1);

      vv_i_elemconnt.push_back(econnt);
    }
    ln++;
  }
  ifs.close();

  // -----------------------------------------------------------------
  // Read SwiftComp dehomogenization output .sn

  int sid{1};  // 1 to 12 (6 strain components and 6 stress components)
  std::vector<std::vector<std::string>> vv_s_results;

  std::ifstream ifs_sn;
  ifs_sn.open(outputsn);
  if (ifs_sn.fail()) {
    std::cout << " -- not found" << std::endl;
    throw std::runtime_error("\n[ERROR] Unable to fine the file" + outputsn + "\n");
  } else {
    printInfo(i_indent, "reading SwiftComp dehomogenization results: " + outputsn);
    std::string line;
    std::vector<std::string> v_s_results{};
    while (getline(ifs_sn, line)) {
      if (line.empty()) {
        vv_s_results.push_back(v_s_results);
        v_s_results = {};
        sid++;
        if (sid > 12) break;
        continue;
      }
      if (!line.empty() && line[line.size()-1] == '\n')
        line.erase(line.size()-1);
      v_s_results.push_back(line);
    }
  }
  ifs_sn.close();

  // =================================================================
  // Write Gmsh input .geo
  printInfo(i_indent, "writing Gmsh .geo file: " + config.file_name_geo);
  std::ofstream ofs_geo(config.file_name_geo);
  ofs_geo.close();

  // -----------------------------------------------------------------
  // Write Gmsh input .msh
  printInfo(i_indent, "writing Gmsh .msh file: " + config.file_name_msh);
  std::ofstream ofs_msh(config.file_name_msh);
  ofs_msh << "$MeshFormat" << std::endl;
  ofs_msh << 2.2 << " " << 0 << " " << 8 << std::endl;
  ofs_msh << "$EndMeshFormat" << std::endl;
  // -----------------------------
  ofs_msh << "$Nodes" << std::endl;
  ofs_msh << nnode << std::endl;
  for (int i{0}; i < v_i_nodeid.size(); ++i) {
    ofs_msh.setf(std::fstream::scientific);
    ofs_msh.width(8);
    ofs_msh << v_i_nodeid[i];
    ofs_msh.width(16);
    ofs_msh << vv_d_nodecoord[i][0];
    ofs_msh.width(16);
    ofs_msh << vv_d_nodecoord[i][1];
    ofs_msh.width(16);
    ofs_msh << vv_d_nodecoord[i][2] << std::endl;
  }
  ofs_msh << "$EndNodes" << std::endl;
  // -----------------------------
  ofs_msh << "$Elements" << std::endl;
  ofs_msh << nelem << std::endl;
  for (int i{0}; i < vv_i_elemconnt.size(); ++i) {
    ofs_msh.width(8);
    ofs_msh << vv_i_elemconnt[i][0];
    ofs_msh << " 2 2 1 1";
    for (int j{2}; j < vv_i_elemconnt[i].size(); ++j) {
      ofs_msh.width(8);
      ofs_msh << vv_i_elemconnt[i][j];
    }
    ofs_msh << std::endl;
  }
  ofs_msh << "$EndElements" << std::endl;
  // -----------------------------
  std::vector<std::string> v_s_label{
    "EN11", "EN22", "EN33", "2EN23", "2EN13", "2EN12",
    "SN11", "SN22", "SN33", "SN23", "SN13", "SN12"
  };
  for (int i = 0; i < 12; ++i) {
    // 6 strain components and 6 stress components
    ofs_msh << "$ElementNodeData" << std::endl;
    ofs_msh << 1 << std::endl;
    ofs_msh << "\"" << v_s_label[i] << "\"" << std::endl;
    ofs_msh << 1 << std::endl;
    ofs_msh << 0.0 << std::endl;
    ofs_msh << 3 << std::endl;
    ofs_msh << 0 << std::endl;  // time step
    ofs_msh << 1 << std::endl;  // 1-component (scalar) field
    ofs_msh << nelem << std::endl;
    for (int j = 0; j < vv_s_results[i].size(); ++j) {
      ofs_msh << vv_s_results[i][j];
      ofs_msh << std::endl;
    }
    ofs_msh << "$EndElementNodeData" << std::endl;
  }
  ofs_msh.close();

  // =================================================================
  // Write Gmsh input .opt
  printInfo(i_indent, "writing Gmsh .opt file: " + config.file_name_opt);
  std::fstream fopt;
  fopt.open(config.file_name_opt, std::ios::out);
  fopt << "General.Axes = " << 3 << ";\n";
  fopt << "General.Orthographic = " << 1 << ";\n";
  fopt << "General.RotationX = " << 270 << ";\n";
  fopt << "General.RotationY = " << 360 << ";\n";
  fopt << "General.RotationZ = " << 270 << ";\n";
  fopt << "General.ScaleX = " << 2 << ";\n";
  fopt << "General.ScaleY = " << 2 << ";\n";
  fopt << "General.ScaleZ = " << 2 << ";\n";
  fopt << "General.Trackball = " << 0 << ";\n";
  fopt << "Geometry.LineWidth = " << 1 << ";\n";
  fopt << "Mesh.Points = " << 0 << ";\n";
  fopt << "Mesh.SurfaceEdges = " << 0 << ";\n";
  fopt << "Mesh.SurfaceFaces = " << 0 << ";\n";
  fopt << "Mesh.VolumeEdges = " << 0 << ";\n";
  fopt << "Mesh.RemeshParametrization = " << 0 << ";\n";
  fopt << "View[0].Visible = " << 1 << ";\n";
  for (int i = 1; i < 12; ++i)
    fopt << "View[" << i << "].Visible = " << 0 << ";\n";
  fopt.close();

  i_indent--;

  return 1;
}

