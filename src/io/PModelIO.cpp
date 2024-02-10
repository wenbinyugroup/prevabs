#include "PModelIO.hpp"

#include "CrossSection.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PMeshClasses.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

// #include "gmsh/GModel.h"
// #include "gmsh/MTriangle.h"
// #include "gmsh/MVertex.h"
// #include "gmsh/SPoint3.h"
// #include "gmsh/SVector3.h"
// #include "gmsh/StringUtils.h"
// #include "rapidxml/rapidxml.hpp"
// #include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __linux__
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#elif _WIN32
#include <tchar.h>
#include <windows.h>
#endif

using namespace rapidxml;

int readInputMain(const std::string &filenameCrossSection,
                  const std::string &filePath, PModel *pmodel, Message *pmessage) {

  pmessage->increaseIndent();

  // if (config.homo) {
  readCrossSection(filenameCrossSection, filePath, pmodel, pmessage);
  // }
  if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    readInputDehomo(filenameCrossSection, filePath, pmodel, pmessage);
  }

  pmessage->decreaseIndent();

  return 0;
}









// // TODO: check swiftcomp format
// int readSG(const std::string &fn, PModel *pmodel, Message *pmessage) {
//   pmessage->increaseIndent();


//   PMesh *mesh = new PMesh();

//   std::ifstream ifs;
//   ifs.open(config.file_name_vsc);
//   if (ifs.fail())
//     throw std::runtime_error("Unable to find the file: " + config.file_name_vsc);
//   // printInfo(i_indent, "reading VABS input data: " + config.file_name_vsc);
  
//   if (config.analysis_tool == 1)
//     PLOG(info) << pmessage->message("reading VABS input data: " + config.file_name_vsc);
//   else if (config.analysis_tool == 2)
//     PLOG(info) << pmessage->message("reading SwiftComp input data: " + config.file_name_vsc);


//   int nnode{0}, nelem{0};
//   int ln(1); // line counter
//   while (ifs) {
//     std::string line;
//     getline(ifs, line);

//     if (!line.empty() && line[line.size() - 1] == '\r')
//       line.erase(line.size() - 1);
//     if (line.empty())
//       continue;


//     if (ln == 4) {
//       // read out number of nodes and number of elements
//       std::stringstream ss;
//       ss << line;
//       ss >> nnode >> nelem;
//     }

//     // read the node block
//     else if (ln > 4 && ln <= (4 + nnode)) {
//       std::stringstream ss;
//       int nid;
//       double nx2, nx3;

//       ss << line;
//       ss >> nid >> nx2 >> nx3;

//       PNode *node = new PNode(nid, 0, nx2, nx3);
//       mesh->addNode(node);
//       // nodesid.push_back(nid);
//       // std::vector<double> ncoords{0.0, nx2, nx3};
//       // nodescoords.push_back(ncoords);
//     }

//     // read the element block
//     else if (ln > (4 + nnode) && ln < (4 + nnode + nelem + 1)) {
//       std::stringstream ss;
//       std::vector<int> econnt{};
//       int ei;

//       ss << line;
//       ss >> ei; // extract the element label
//       PElement *element = new PElement(ei);
//       // element->setTypeId(2);

//       if (config.analysis_tool == 2) {
//         // For swiftcomp, read the layer type id after the element id
//         int prop_id;
//         ss >> prop_id;
//         element->setPropId(prop_id);
//       }

//       // econnt.push_back(ei);
//       while (!ss.eof()) {
//         // extract all nodes belonging to this element
//         int n;
//         ss >> n;
//         if (n != 0) {
//           element->addNode(n);
//         }
//           // econnt.push_back(n);
//       }

//       // std::vector<int>::iterator it{econnt.begin()};
//       // econnt.insert(it + 1, econnt.size() - 1);

//       // elements.push_back(econnt);
//       mesh->addElement(element);
//     }


//     ln++;
//     // if (doonce)
//     //   break;
//   }

//   ifs.close();


//   pmodel->setMesh(mesh);


//   pmessage->decreaseIndent();

//   return 0;
// }










int writeFace(FILE *file, PDCELFace *face) {
  fprintf(file, "name: %s\n", face->name().c_str());

  // if (face->layertype() != nullptr) {
  //   fprintf(file, "layertype: %");
  // }

  PDCELHalfEdge *he;
  // Write the outer boundary
  if (face->outer() == nullptr) {
    // std::cout << "unbounded face." << std::endl;
    fprintf(file, "unbounded face\n");
  } else {
    // std::cout << "outer boundary: " << std::endl;
    he = face->outer();
    // std::cout << he->source();
    do {
      // std::cout << "edge: " << he << std::endl;
      // he->print2();
      fprintf(file, "%16e %16e\n", he->source()->y(), he->source()->z());
      // std::cout << " -> " << he->target();
      he = he->next();
    } while (he != face->outer());
    // std::cout << std::endl;
    fprintf(file, "\n");

    for (auto _inner : face->inners()) {
      // std::cout << "inner boundary: " << std::endl;
      fprintf(file, "inner boundary:\n");
      he = _inner;
      do {
        // std::cout << "edge: " << he << std::endl;
        // he->print2();
        fprintf(file, "%16e %16e\n", he->source()->y(), he->source()->z());
        he = he->next();
      } while (he != _inner);
      // std::cout << std::endl;
      fprintf(file, "\n");
    }
  }
  return 0;
}









// int PModel::writeSG(std::string fn, int fmt, Message *pmessage) {
//   // i_indent++;
//   pmessage->increaseIndent();

//   // printInfo(i_indent, "writing sg file: " + fn);
//   // pmessage->print(1, "writing sg file: " + fn);
//   PLOG(info) << pmessage->message("writing sg file: " + fn);

//   FILE *fsg;
//   fsg = fopen(fn.c_str(), "w");

//   std::vector<int> inums;
//   std::vector<double> dnums;

//   // Write the analysis settings
//   if (config.analysis_tool == 1) {
//     // VABS
//     writeSettingsVABS(fsg, this);
//   } else if (config.analysis_tool == 2) {
//     // SwiftComp
//     writeSettingsSC(fsg, this);
//   }

//   // Write nodes and elements
//   writeNodes(fsg, this);

//   if (config.analysis_tool == 1) {
//     writeElementsVABS(fsg, this);
//   } else if (config.analysis_tool == 2) {
//     writeElementsSC(fsg, this);
//   }

//   // Write layer types and materials
//   if (config.analysis_tool == 1) {
//     writeMaterialsVABS(fsg, this);
//   } else if (config.analysis_tool == 2) {
//     writeMaterialsSC(fsg, this);
//   }

//   // Omega for SwiftComp only
//   if (config.analysis_tool == 2) {
//     fprintf(fsg, "%16e\n", 1.0);
//   }

//   fclose(fsg);

//   // config.file_name_vsc = fn + ".sg";

//   // i_indent--;
//   pmessage->decreaseIndent();

//   return 1;
// }

