#include "PModel.hpp"

#include "PModelIO.hpp"
#include "PDataClasses.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh/StringUtils.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <vector>


// std::vector<std::vector<double>> readVABSRecoveredData(
//   const std::string &filename,
//   const int &nelem, const std::vector<std::vector<int>> &elements) {

//   std::vector<std::vector<double>> data;

//   std::ifstream ifen;
//   ifen.open(filename);
//   if (ifen.fail()) {
//     printWarning(i_indent + 1, "[WARNING] Unable to find the file: " + filename);
//   }
//   else {
//     printInfo(i_indent, "reading VABS recovered data: " + filename);
//     int ei = 1;
//     int ni = 1;
//     std::vector<double> enei{};
//     while (ifen) {
//       // every nnodes lines belong to one element
//       int nnodes{elements[ei - 1][1]}; // number of nodes
//       std::string line;
//       int nitemp;
//       double x2, x3;
//       double comp1, comp2, comp3, comp4, comp5, comp6;
//       getline(ifen, line);
//       if (!line.empty() && line[line.size() - 1] == '\r')
//         line.erase(line.size() - 1);
//       if (line.empty())
//         continue;
//       std::stringstream ss;
//       ss << line;
//       ss >> nitemp >> x2 >> x3 >> comp1 >> comp2 >> comp3 >> comp4 >> comp5 >> comp6;
//       // std::cout << "element " << ei << " node " << nitemp << std::endl;
//       enei.push_back(comp1);
//       enei.push_back(comp2);
//       enei.push_back(comp3);
//       enei.push_back(comp4);
//       enei.push_back(comp5);
//       enei.push_back(comp6);
//       if (ni == nnodes) {
//         data.push_back(enei);
//         ni = 1;
//         if (ei != nelem)
//           ei++; // next element
//         enei.clear();
//       } else
//         ni++;
//     }
//   }
//   ifen.close();

//   return data;
// }









// void writeGmshElementNodeData(
//   std::ofstream &ofs_msh, const std::vector<std::string> &labels,
//   const int &nelem, const std::vector<std::vector<int>> &elements,
//   const std::vector<std::vector<double>> &data) {

//   for (int comp{0}; comp < 6; ++comp) {
//     // For each strain component
//     ofs_msh << "$ElementNodeData" << std::endl;
//     ofs_msh << 1 << std::endl;
//     ofs_msh << "\"" << labels[comp] << "\"" << std::endl;
//     ofs_msh << "1\n0\n3\n0\n1" << std::endl;
//     ofs_msh << nelem << std::endl;
//     for (int i{0}; i < nelem; ++i) {
//       ofs_msh.width(8);
//       ofs_msh << elements[i][0];
//       ofs_msh.width(4);
//       ofs_msh << elements[i][1];
//       for (int j{comp}; j < data[i].size(); j += 6) {
//         ofs_msh.setf(std::fstream::scientific);
//         ofs_msh.width(16);
//         ofs_msh << data[i][j];
//       }
//       ofs_msh << std::endl;
//     }
//     ofs_msh << "$EndElementNodeData" << std::endl;
//   }
// }









int PModel::postVABS(Message *pmessage) {
  // i_indent++;
  pmessage->increaseIndent();

  bool doonce{false};

  std::vector<std::string> v_inputvabs{SplitFileName(config.file_name_vsc)};

  // std::string outputu{config.file_name_vsc + ".U"};
  // std::string outputen{config.file_name_vsc + ".EN"};
  // std::string outputsn{config.file_name_vsc + ".SN"};
  // std::string outputemn{config.file_name_vsc + ".EMN"};
  // std::string outputsmn{config.file_name_vsc + ".SMN"};
  std::string outputele{config.file_name_vsc + ".ELE"};

  config.file_name_geo = v_inputvabs[0] + v_inputvabs[1] + "_lss.geo";
  config.file_name_msh = v_inputvabs[0] + v_inputvabs[1] + "_lss.msh";
  config.file_name_opt = v_inputvabs[0] + v_inputvabs[1] + "_lss.opt";

  int nview{0};

  std::vector<int> nodesid{};
  std::vector<std::vector<double>> nodescoords{};
  std::vector<std::vector<int>> elements{};

  std::vector<std::string> eslabel{
    "E11", "2E12", "2E13", "E22", "2E23", "E33",
    "S11", "S12", "S13", "S22", "S23", "S33",
    "EM11", "2EM12", "2EM13", "EM22", "2EM23", "EMN3",
    "SM11", "SM12", "SM13", "SM22", "SM23", "SM33"
  };
  // std::vector<std::string> enlabel{"EN11", "2EN12", "2EN13",
  //                                  "EN22", "2EN23", "EN33"};
  // std::vector<std::string> snlabel{"SN11", "SN12", "SN13",
  //                                  "SN22", "SN23", "SN33"};
  // std::vector<std::string> emnlabel{"EMN11", "2EMN12", "2EMN13",
  //                                  "EMN22", "2EMN23", "EMN33"};
  // std::vector<std::string> smnlabel{"SMN11", "SMN12", "SMN13",
  //                                  "SMN22", "SMN23", "SMN33"};
  // std::vector<std::vector<double>> en{};
  // std::vector<std::vector<double>> sn{};
  // std::vector<std::vector<double>> emn{};
  // std::vector<std::vector<double>> smn{};


  // =================================================================
  // Read VABS input .dat
  std::ifstream ifs;
  ifs.open(config.file_name_vsc);
  if (ifs.fail())
    throw std::runtime_error("Unable to find the file: " + config.file_name_vsc);
  // printInfo(i_indent, "reading VABS input data: " + config.file_name_vsc);
  PLOG(info) << pmessage->message("reading VABS input data: " + config.file_name_vsc);


  int nnode{0}, nelem{0};
  int ln(1); // line counter
  while (ifs) {
    std::string line;
    getline(ifs, line);
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);
    if (line.empty())
      continue;
    if (ln == 4) {
      // read out number of nodes and number of elements
      std::stringstream ss;
      ss << line;
      ss >> nnode >> nelem;
    } else if (ln > 4 && ln <= (4 + nnode)) {
      // read the node block
      std::stringstream ss;
      int nid;
      double nx2, nx3;
      ss << line;
      ss >> nid >> nx2 >> nx3;
      nodesid.push_back(nid);
      std::vector<double> ncoords{0.0, nx2, nx3};
      nodescoords.push_back(ncoords);
    } else if (ln > (4 + nnode) && ln < (4 + nnode + nelem + 1)) {
      // read the element block
      std::stringstream ss;
      std::vector<int> econnt{};
      int ei;
      ss << line;
      ss >> ei; // extract the element label
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

      elements.push_back(econnt);
    }
    ln++;
    if (doonce)
      break;
  }

  ifs.close();


  // =================================================================
  // Read VABS output .u, .en, .sn
  // int ei, ni;

  // // Strain, Global, Nodal (.en)
  // en = readVABSRecoveredData(outputen, nelem, elements);

  // // Stress, Global, Nodal (.sn)
  // sn = readVABSRecoveredData(outputsn, nelem, elements);

  // // Strain, Material, Nodal (.emn)
  // emn = readVABSRecoveredData(outputemn, nelem, elements);

  // // Stress, Material, Nodal (.smn)
  // smn = readVABSRecoveredData(outputsmn, nelem, elements);

  std::vector<PElementNodeData> elmdata;
  elmdata = readOutputDehomoElementVABS(outputele, pmessage);



  // =================================================================
  // Write Gmsh input .geo
  // printInfo(i_indent, "writing Gmsh .geo file: " + config.file_name_geo);
  PLOG(info) << pmessage->message("writing gmsh .geo file: " + config.file_name_geo);
  std::ofstream ofs_geo(config.file_name_geo);
  ofs_geo.close();

  // =================================================================
  // Write Gmsh input .msh
  // printInfo(i_indent, "writing Gmsh .msh data: " + config.file_name_msh);
  PLOG(info) << pmessage->message("writing gmsh .msh file: " + config.file_name_msh);
  std::ofstream ofs_msh(config.file_name_msh);
  ofs_msh << "$MeshFormat" << std::endl;
  ofs_msh << 2.2 << " " << 0 << " " << 8 << std::endl;
  ofs_msh << "$EndMeshFormat" << std::endl;
  // -----------------------------
  ofs_msh << "$Nodes" << std::endl;
  ofs_msh << nnode << std::endl;
  for (int i{0}; i < nodesid.size(); ++i) {
    ofs_msh.setf(std::fstream::scientific);
    ofs_msh.width(8);
    ofs_msh << nodesid[i];
    ofs_msh.width(16);
    ofs_msh << nodescoords[i][0];
    ofs_msh.width(16);
    ofs_msh << nodescoords[i][1];
    ofs_msh.width(16);
    ofs_msh << nodescoords[i][2] << std::endl;
  }
  ofs_msh << "$EndNodes" << std::endl;
  // -----------------------------
  ofs_msh << "$Elements" << std::endl;
  ofs_msh << nelem << std::endl;
  for (int i{0}; i < elements.size(); ++i) {
    ofs_msh.width(8);
    ofs_msh << elements[i][0];
    ofs_msh << " 2 2 1 1";
    for (int j{2}; j < elements[i].size(); ++j) {
      ofs_msh.width(8);
      ofs_msh << elements[i][j];
    }
    ofs_msh << std::endl;
  }
  ofs_msh << "$EndElements" << std::endl;

  writeGmshElementData(ofs_msh, elmdata, eslabel, pmessage);
  nview += 24;

  // if (en.size() > 0) {
  //   writeGmshElementNodeData(ofs_msh, enlabel, nelem, elements, en);
  //   nview += 6;
  // }

  // if (sn.size() > 0) {
  //   writeGmshElementNodeData(ofs_msh, snlabel, nelem, elements, sn);
  //   nview += 6;
  // }

  // if (emn.size() > 0) {
  //   writeGmshElementNodeData(ofs_msh, emnlabel, nelem, elements, emn);
  //   nview += 6;
  // }

  // if (smn.size() > 0) {
  //   writeGmshElementNodeData(ofs_msh, smnlabel, nelem, elements, smn);
  //   nview += 6;
  // }

  ofs_msh.close();

  // =================================================================
  // Write Gmsh input .opt
  // printInfo(i_indent, "writing Gmsh .opt file: " + config.file_name_opt);
  PLOG(info) << pmessage->message("writing gmsh .opt file: " + config.file_name_opt);
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
  for (int i{0}; i < nview; ++i) {
    fopt << "View[" << i << "].Visible=";
    if (i == 0) {
      fopt << "1;";
    }
    else {
      fopt << "0;";
    }
    // if (nview == 6) {
    //   if (i == 0)
    //     fopt << "1;";
    //   else
    //     fopt << "0;";
    // } else if (nview == 12) {
    //   if (i == 6)
    //     fopt << "1;";
    //   else
    //     fopt << "0;";
    // }
    fopt << std::endl;
  }
  fopt.close();

  // i_indent--;
  pmessage->decreaseIndent();

  return 1;
}
