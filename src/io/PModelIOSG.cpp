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
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"
#include "gmsh_mod/StringUtils.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

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

int PModel::writeSG(std::string fn, int fmt, Message *pmessage) {

  pmessage->increaseIndent();

  PLOG(info) << pmessage->message("writing sg file: " + fn);

  FILE *fsg;
  fsg = fopen(fn.c_str(), "w");

  std::vector<int> inums;
  std::vector<double> dnums;

  // ------------------------------
  // Write the analysis settings
  if (config.analysis_tool == 1) {
    // VABS
    writeSettingsVABS(fsg, this);
  } else if (config.analysis_tool == 2) {
    // SwiftComp
    writeSettingsSC(fsg, this);
  }

  // ------------------------------
  // Write nodes and elements
  // writeNodes(fsg, this);
  writeNodes(fsg, pmessage);

  if (config.analysis_tool == 1) {
    // writeElementsVABS(fsg, this);
    writeElementsVABS(fsg, pmessage);
  } else if (config.analysis_tool == 2) {
    // writeElementsSC(fsg, this);
    writeElementsSC(fsg, pmessage);
  }

  // ------------------------------
  // Write layer types and materials
  if (config.analysis_tool == 1) {
    writeMaterialsVABS(fsg, this);
  } else if (config.analysis_tool == 2) {
    writeMaterialsSC(fsg, this);
  }

  // ------------------------------
  // Omega for SwiftComp only
  if (config.analysis_tool == 2) {
    fprintf(fsg, "%16e\n", _omega);
  }

  fclose(fsg);


  // ------------------------------
  // Write a supplementary file
  // to store the mapping between the material id and name
  fn_mid2name = fn + ".mat";
  PLOG(info) << pmessage->message("writing material id-name file: " + fn_mid2name);

  FILE *fsg_mat;
  fsg_mat = fopen(fn_mid2name.c_str(), "w");

  for (auto m : this->cs()->getUsedMaterials()) {
    fprintf(fsg_mat, "%4d    %s\n", m->id(), m->getName().c_str());
  }
  fprintf(fsg_mat, "\n");

  fclose(fsg_mat);


  pmessage->decreaseIndent();

  return 1;
}









int readSG(const std::string &fn, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();


  PMesh *mesh = new PMesh();

  std::ifstream ifs;
  ifs.open(config.file_name_vsc);
  if (ifs.fail())
    throw std::runtime_error("Unable to find the file: " + config.file_name_vsc);
  // printInfo(i_indent, "reading VABS input data: " + config.file_name_vsc);
  
  if (config.analysis_tool == 1)
    PLOG(info) << pmessage->message("reading VABS input data: " + config.file_name_vsc);
  else if (config.analysis_tool == 2)
    PLOG(info) << pmessage->message("reading SwiftComp input data: " + config.file_name_vsc);


  int nnode{0}, nelem{0}, nsg;
  int ln(1); // line counter
  int num_head_lines;
  if (config.analysis_tool == 1) num_head_lines = 4;
  else if (config.analysis_tool == 2) num_head_lines = 5;

  while (ifs) {
    std::string line;
    getline(ifs, line);

    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);
    if (line.empty())
      continue;


    if (ln == num_head_lines) {
      // read out number of nodes and number of elements
      std::stringstream ss;
      ss << line;
      if (config.analysis_tool == 1) ss >> nnode >> nelem;
      else if (config.analysis_tool == 2) ss >> nsg >> nnode >> nelem;
    }

    // read the node block
    else if (ln > num_head_lines && ln <= (num_head_lines + nnode)) {
      std::stringstream ss;
      int nid;
      double nx2, nx3;

      ss << line;
      ss >> nid >> nx2 >> nx3;

      PNode *node = new PNode(nid, 0, nx2, nx3);
      mesh->addNode(node);
      // nodesid.push_back(nid);
      // std::vector<double> ncoords{0.0, nx2, nx3};
      // nodescoords.push_back(ncoords);
    }

    // read the element block
    else if (ln > (num_head_lines + nnode) && ln < (num_head_lines + nnode + nelem + 1)) {
      std::stringstream ss;
      std::vector<int> econnt{};
      int ei;

      ss << line;
      ss >> ei; // extract the element label
      PElement *element = new PElement(ei);
      // element->setTypeId(2);

      if (config.analysis_tool == 2) {
        // For swiftcomp, read the layer type id after the element id
        int prop_id;
        ss >> prop_id;
        element->setPropId(prop_id);
      }

      // econnt.push_back(ei);
      while (!ss.eof()) {
        // extract all nodes belonging to this element
        int n;
        ss >> n;
        if (n != 0) {
          element->addNode(n);
        }
          // econnt.push_back(n);
      }

      // std::vector<int>::iterator it{econnt.begin()};
      // econnt.insert(it + 1, econnt.size() - 1);

      // elements.push_back(econnt);
      mesh->addElement(element);
    }


    ln++;
    // if (doonce)
    //   break;
  }

  ifs.close();


  pmodel->setMesh(mesh);


  pmessage->decreaseIndent();

  return 0;
}









// Write in VABS format
// ===================================================================

void writeSettingsVABS(FILE *file, PModel *model) {
  std::vector<unsigned int> inums;

  // inums = {1, model->cs()->getNumOfUsedLayerTypes()};
  inums.push_back(1);
  inums.push_back(model->cs()->getNumOfUsedLayerTypes());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");

  inums.clear();
  unsigned int recover = config.dehomo ? 1 : 0;
  // inums = {model->analysisModel(), recover, model->analysisThermal()};
  inums.push_back(model->analysisModel());
  // inums.push_back(recover);
  inums.push_back(model->analysisDamping());
  inums.push_back(model->analysisThermal());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");

  inums.clear();
  // inums = {model->analysisCurvature(), model->analysisOblique(),
  //          model->analysisTrapeze(), model->analysisVlasov()};
  inums.push_back(model->analysisCurvature());
  inums.push_back(model->analysisOblique());
  inums.push_back(model->analysisTrapeze());
  inums.push_back(model->analysisVlasov());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");

  if (model->analysisCurvature()) {
    writeNumbers(file, "%16e", model->curvatures());
    fprintf(file, "\n");
  }

  if (model->analysisOblique()) {
    writeNumbers(file, "%16e", model->obliques());
    fprintf(file, "\n");
  }

  inums.clear();
  // inums = {model->gmodel()->indexMeshVertices(true, 0),
  //          model->indexGmshElements(), model->cs()->getNumOfUsedMaterials()};
  // inums.push_back(model->gmodel()->indexMeshVertices(true, 0));
  inums.push_back(model->getNumOfNodes());
  // inums.push_back(model->indexGmshElements());
  inums.push_back(model->getNumOfElements());
  inums.push_back(model->cs()->getNumOfUsedMaterials());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");
}









// void writeNodes(FILE *file, PModel *pmodel) {
//   std::vector<GEntity *> gentities;
//   pmodel->gmodel()->getEntities(gentities);
//   for (unsigned int i = 0; i < gentities.size(); ++i) {

//     for (unsigned int j = 0; j < gentities[i]->mesh_vertices.size(); ++j) {

//       if (gentities[i]->mesh_vertices[j]->getIndex() > 0) {
//         fprintf(file, "%8d%16e%16e\n",
//                 gentities[i]->mesh_vertices[j]->getIndex(),
//                 gentities[i]->mesh_vertices[j]->y(),
//                 gentities[i]->mesh_vertices[j]->z());

//         if (pmodel->interfaceOutput()) {
//           std::vector<int> eids;
//           // Add an empty vector as a placeholder
//           pmodel->node_elements.push_back(eids);
//         }

//       }

//       else {
//         continue;
//       }

//     }
//   }
//   fprintf(file, "\n");
// }









// template <class T> void writeElementVABS(FILE *file, PModel *pmodel, T *elem) {
//   std::vector<int> inums(9, 0);

//   int eid = pmodel->gmodel()->getMeshElementIndex(elem);
//   fprintf(file, "%8d", eid);

//   for (int i = 0; i < elem->getNumVertices(); ++i) {
//     int nid = elem->getVertex(i)->getIndex();

//     if (i < 3) {
//       inums[i] = nid;
//     } else {
//       inums[i + 1] = nid;
//     }

//     if (pmodel->interfaceOutput()) {
//       // pmodel->addNodeElement(nid, eid);
//       pmodel->node_elements[nid-1].push_back(eid);
//     }

//   }

//   writeNumbers(file, "%8d", inums);

// }









// void writeElementsVABS(FILE *file, PModel *pmodel) {
//   // Write connectivity for each element
//   // for (auto fit = model->firstFace(); fit != model->lastFace(); ++fit) {
//   for (auto f : pmodel->dcel()->faces()) {
//     if (f->gface() != nullptr) {
//       for (auto elem : f->gface()->triangles) {
//         writeElementVABS(file, pmodel, elem);
//       }
//     }
//   }
//   fprintf(file, "\n");

//   // Wirte layer type and theta_1 for each element
//   // for (auto fit = model->firstFace(); fit != model->lastFace(); ++fit) {
//   for (auto f : pmodel->dcel()->faces()) {
//     if (f->gface() != nullptr) {
//       for (auto elem : f->gface()->triangles) {
//         fprintf(file, "%8d%8d%16e\n",
//                 pmodel->gmodel()->getMeshElementIndex(elem),
//                 // f->gface()->physicals[0],
//                 f->layertype()->id(), f->theta1());
//       }
//     }
//   }
//   fprintf(file, "\n");
// }









void writeMaterialVABS(FILE *file, Material *m) {
  // std::cout << "writing material " << m->getName() << std::endl;
  // m->printMaterial();
  fprintf(file, "%8d", m->id());

  if (m->getType() == "isotropic") {
    fprintf(file, "%8d\n", 0);
    fprintf(file, "%16e%16e\n", m->getElastic()[0], m->getElastic()[1]);
  }
  else if (m->getType() == "orthotropic" || m->getType() == "engineering") {
    fprintf(file, "%8d\n", 1);
    int index = 0;
    for (int row = 1; row <= 3; ++row) {
      for (int col = 1; col <= 3; ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }
  }
  else if (m->getType() == "anisotropic") {
    fprintf(file, "%8d\n", 2);
    int index = 0;
    for (int row = 1; row <= 6; ++row) {
      for (int col = 1; col <= (6 - row + 1); ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }
  }
  fprintf(file, "%16e\n", m->getDensity());
}









void writeMaterialStrength(FILE *file, Material *m) {
  Strength sp = m->getStrength();
  std::string type = sp._type;
  int fc = m->getFailureCriterion();

  if (type == "lamina") {
    m->completeStrengthProperties();
    type = "orthotropic";
  }

  fprintf(file, "%8d", fc);

  if (type == "isotropic") {
    if ((fc == 1) || (fc == 2)) {
      fprintf(file, "%8d\n", 2);
      fprintf(file, "%16e%16e\n", sp.t1, sp.c1);
    }
    else if ((fc == 3) || (fc == 4)) {
      fprintf(file, "%8d\n", 1);
      fprintf(file, "%16e\n", sp.s12);
    }
    else if (fc == 5) {
      fprintf(file, "%8d\n", 1);
      fprintf(file, "%16e\n", sp.t1);
    }
  }
  else if ((type == "orthotropic") || (type == "engineering") || (type == "anisotropic")) {
    if ((fc == 1) || (fc == 2) || (fc == 4)) {
      fprintf(file, "%8d\n", 9);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e%16e%16e%16e\n",
        sp.t1, sp.t2, sp.t3, sp.c1, sp.c2, sp.c3, sp.s23, sp.s13, sp.s12);
    }
    else if (fc == 3) {
      fprintf(file, "%8d\n", 6);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e\n",
        sp.t1, sp.t2, sp.t3, sp.s23, sp.s13, sp.s12);
    }
    else if (fc == 5) {
      fprintf(file, "%8d\n", 6);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e\n",
        sp.t1, sp.t2, sp.c1, sp.c2, sp.s23, sp.s12);
    }
  }
}









void writeMaterialsVABS(FILE *file, PModel *model) {
  for (auto lt : model->cs()->getUsedLayerTypes()) {
    fprintf(file, "%8d%8d%16e\n", lt->id(), lt->material()->id(), lt->angle());
  }
  fprintf(file, "\n");

  for (auto m : model->cs()->getUsedMaterials()) {
    writeMaterialVABS(file, m);
  }
  fprintf(file, "\n");
}









// Write in SwiftComp format
// ===================================================================

void writeSettingsSC(FILE *file, PModel *model) {
  std::vector<unsigned int> inums;

  if (model->analysisModelDim() == 1) {
    // Beam model
    // BM Line 1: submodel
    unsigned int imodel = model->analysisModel();
    if (model->analysisVlasov() == 1) {
      imodel = 2;
    } else if (model->analysisTrapeze() == 1) {
      imodel = 3;
    }
    fprintf(file, "%8d\n\n", imodel);

    // BM Line 2: curvatures
    writeNumbers(file, "%16e", model->curvatures());
    fprintf(file, "\n");

    // BM Line 3: obliques
    writeNumbers(file, "%16e", model->obliques());
    fprintf(file, "\n");
  }

  else if (model->analysisModelDim() == 2) {
    // Plate/shell model
  }


  // Line 1
  // unsigned int ianalysis = 0;
  // if (model->analysisThermal() == 1) {
  //   ianalysis = 1;
  // }
  unsigned int ianalysis = model->analysisPhysics();
  unsigned int ielem_flag = 0;
  unsigned int itrans_flag = 1;
  unsigned int itemp_flag = 0;
  unsigned int iforce_flag = 0;
  unsigned int isteer_flag = 0;
  inums = {ianalysis, ielem_flag, itrans_flag, itemp_flag};
  if (config.tool_ver == "2.2") {
    inums.push_back(iforce_flag);
    inums.push_back(isteer_flag);
  }
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");

  // Line 2
  inums.clear();
  unsigned int insg = 2;
  unsigned int inslave = 0;
  // inums = {
  //   insg, // nsg
  //   model->gmodel()->indexMeshVertices(true, 0), // nnode
  //   model->indexGmshElements(), // nelem
  //   model->cs()->getNumOfUsedMaterials(), // nmate
  //   inslave, // nslave
  //   model->cs()->getNumOfUsedLayerTypes() // nlayer
  // };
  inums.push_back(insg);
  // inums.push_back(model->gmodel()->indexMeshVertices(true, 0));
  inums.push_back(model->getNumOfNodes());
  // inums.push_back(model->indexGmshElements());
  inums.push_back(model->getNumOfElements());
  inums.push_back(model->cs()->getNumOfUsedMaterials());
  inums.push_back(inslave);
  inums.push_back(model->cs()->getNumOfUsedLayerTypes());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");
}









// template <class T> void writeElementSC(FILE *file, PModel *model, T *elem, int mid) {
//   std::vector<int> inums(9, 0);
//   fprintf(file, "%8d%8d", model->gmodel()->getMeshElementIndex(elem), mid);
//   for (int i = 0; i < elem->getNumVertices(); ++i) {
//     if (i < 3) {
//       inums[i] = elem->getVertex(i)->getIndex();
//     } else {
//       inums[i + 1] = elem->getVertex(i)->getIndex();
//     }
//   }
//   writeNumbers(file, "%8d", inums);
// }









// void writeElementsSC(FILE *file, PModel *model) {
//   // Write connectivity for each element
//   for (auto f : model->dcel()->faces()) {
//     if (f->gface() != nullptr) {
//       for (auto elem : f->gface()->triangles) {
//         writeElementSC(file, model, elem, f->layertype()->id());
//       }
//     }
//   }
//   fprintf(file, "\n");

//   // Wirte local coordinate for each element
//   std::vector<double> dnums;
//   for (auto f : model->dcel()->faces()) {
//     if (f->gface() != nullptr) {
//       for (auto elem : f->gface()->triangles) {
//         fprintf(file, "%8d", model->gmodel()->getMeshElementIndex(elem));
//         dnums = {
//           // 1.0, 0.0, 0.0, 
//           f->localy1()[0], f->localy1()[1], f->localy1()[2], 
//           f->localy2()[0], f->localy2()[1], f->localy2()[2], 
//           0.0, 0.0, 0.0
//         };
//         writeNumbers(file, "%16e", dnums);
//       }
//     }
//   }
//   fprintf(file, "\n");
// }









void writeMaterialSC(FILE *file, Material *m, unsigned int physics) {
  int intemp = 1;

  fprintf(file, "%8d", m->id());

  if (m->getType() == "isotropic") {
    fprintf(file, "%8d%8d\n", 0, intemp);
    fprintf(file, "%8d%16e\n", 1, m->getDensity());
    fprintf(file, "%16e%16e\n", m->getElastic()[0], m->getElastic()[1]);

    if (physics == 1 || physics == 4 || physics == 6) {
      fprintf(file, "%16e%16e\n", m->getCte()[0], m->getSpecificHeat());
    }
  }

  else if (m->getType() == "orthotropic" || m->getType() == "engineering") {
    fprintf(file, "%8d%8d\n", 1, intemp);
    fprintf(file, "%8d%16e\n", 1, m->getDensity());
    int index = 0;
    for (int row = 1; row <= 3; ++row) {
      for (int col = 1; col <= 3; ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }

    if (physics == 1 || physics == 4 || physics == 6) {
      fprintf(
        file,
        "%16e%16e%16e%16e\n",
        m->getCte()[0], m->getCte()[1], m->getCte()[2], m->getSpecificHeat()
      );
    }
  }

  else if (m->getType() == "anisotropic") {
    fprintf(file, "%8d%8d\n", 2, intemp);
    fprintf(file, "%8d%16e\n", 1, m->getDensity());
    int index = 0;
    for (int row = 1; row <= 6; ++row) {
      for (int col = 1; col <= (6 - row + 1); ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }

    if (physics == 1 || physics == 4 || physics == 6) {
      fprintf(
        file,
        "%16e%16e%16e%16e%16e%16e%16e\n",
        m->getCte()[0], m->getCte()[1], m->getCte()[2],
        m->getCte()[3], m->getCte()[4], m->getCte()[5], m->getSpecificHeat()
      );
    }
  }

}









void writeMaterialsSC(FILE *file, PModel *model) {
  for (auto lt : model->cs()->getUsedLayerTypes()) {
    fprintf(file, "%8d%8d%16e\n", lt->id(), lt->material()->id(), lt->angle());
  }
  fprintf(file, "\n");

  for (auto m : model->cs()->getUsedMaterials()) {
    writeMaterialSC(file, m, model->analysisPhysics());
  }
  fprintf(file, "\n");
}

