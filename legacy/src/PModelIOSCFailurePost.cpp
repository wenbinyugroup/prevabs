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


int PModel::postSCFailure() {
  i_indent++;
  
  std::vector<std::string> v_inputsc{SplitFileName(config.file_name_vsc)};
  std::string outputfi{config.file_name_vsc + ".fi"};
  config.file_name_geo = outputfi + ".geo";
  config.file_name_msh = outputfi + ".msh";

  int nview{0};

  std::vector<int> v_i_nodeid{};
  std::vector<std::vector<double>> vv_d_nodecoord{};
  std::vector<std::vector<int>> vv_i_elemconnt{};
  std::vector<double> v_d_fi{};  // failure indices
  std::vector<double> v_d_sr{};  // strength ratios

  // -----------------------------------------------------------------
  // Read SwiftComp input .sc
  printInfo(i_indent, "Reading SwiftComp input data.");
  std::ifstream ifs;
  ifs.open(config.file_name_vsc);
  if (ifs.fail())
    throw std::runtime_error("Unable to find the file: " + config.file_name_vsc);
  printInfo(i_indent + 1, "Found SwiftComp input data: " + config.file_name_vsc);
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
      std::vector<double> ncoords{nx2, nx3, 0.0};
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
  // Read SwiftComp failure analysis output .fi

  printInfo(i_indent, "Reading SwiftComp failure analysis results");
  int eid;
  double fi, sr;

  std::ifstream ifs_fi;
  std::cout << std::string(i_indent, ' ') << ">> SwiftComp failure analysis results file";
  ifs_fi.open(outputfi);
  if (ifs_fi.fail()) {
    std::cout << " -- not found" << std::endl;
    throw std::runtime_error("\n[ERROR] Unable to fine the file" + outputfi + "\n");
  } else {
    std::cout << " -- found" << std::endl;
    while (ifs_fi) {
      std::string line;
      getline(ifs_fi, line);
      if (line.empty()) continue;
      if (!line.empty() && line[line.size()-1] == '\n')
        line.erase(line.size()-1);
      std::stringstream ss;
      ss << line;
      ss >> eid >> fi >> sr;
      v_d_fi.push_back(fi);
      v_d_sr.push_back(sr);
    }
  }
  ifs_fi.close();

  // -----------------------------------------------------------------
  // Write Gmsh input .msh
  std::cout << std::string(i_indent, ' ') << ">> Writing Gmsh .msh file";
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
  ofs_msh << "$ElementData" << std::endl;
  ofs_msh << 1 << std::endl;
  ofs_msh << "\"" << "Failure index" << "\"" << std::endl;
  ofs_msh << 1 << std::endl;
  ofs_msh << 0.0 << std::endl;
  ofs_msh << 3 << std::endl;
  ofs_msh << 0 << std::endl;  // time step
  ofs_msh << 1 << std::endl;  // 1-component (scalar) field
  ofs_msh << nelem << std::endl;  // number of element values
  for (int ei = 0; ei < vv_i_elemconnt.size(); ei++) {
    ofs_msh.width(8);
    ofs_msh << vv_i_elemconnt[ei][0];
    ofs_msh.setf(std::fstream::scientific);
    ofs_msh.width(16);
    ofs_msh << v_d_fi[ei];
    ofs_msh << std::endl;
  }
  ofs_msh << "$EndElementData" << std::endl;
  // -----------------------------
  ofs_msh << "$ElementData" << std::endl;
  ofs_msh << 1 << std::endl;
  ofs_msh << "\"" << "Strength ratio" << "\"" << std::endl;
  ofs_msh << 1 << std::endl;
  ofs_msh << 0.0 << std::endl;
  ofs_msh << 3 << std::endl;
  ofs_msh << 0 << std::endl;  // time step
  ofs_msh << 1 << std::endl;  // 1-component (scalar) field
  ofs_msh << nelem << std::endl;  // number of element values
  for (int ei = 0; ei < vv_i_elemconnt.size(); ei++) {
    ofs_msh.width(8);
    ofs_msh << vv_i_elemconnt[ei][0];
    ofs_msh.setf(std::fstream::scientific);
    ofs_msh.width(16);
    ofs_msh << v_d_sr[ei];
    ofs_msh << std::endl;
  }
  ofs_msh << "$EndElementData" << std::endl;

  ofs_msh.close();
  std::cout << " -- finished" << std::endl;

  // -----------------------------------------------------------------
  // Write Gmsh input .geo
  std::cout << std::string(i_indent, ' ') << ">> Writing Gmsh .geo file";
  std::ofstream ofs_geo(config.file_name_geo);
  ofs_geo << "Merge \"" << GetFileNameWithoutPath(config.file_name_msh) << "\";"
        << std::endl;
  ofs_geo << "Mesh.SurfaceFaces=0;" << std::endl;
  ofs_geo << "Mesh.Points=0;" << std::endl;
  ofs_geo << "Mesh.SurfaceEdges=0;" << std::endl;
  ofs_geo << "Mesh.VolumeEdges=0;" << std::endl;
  ofs_geo << "View[0].Visible=1;" << std::endl;
  ofs_geo << "View[1].Visible=0;" << std::endl;
  ofs_geo.close();
  std::cout << " -- finished" << std::endl;

  i_indent--;

  return 1;
}
