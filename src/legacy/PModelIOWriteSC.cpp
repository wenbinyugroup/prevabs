#include "PModelIO.hpp"

#include "CrossSection.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"

#include "gmsh/GModel.h"
#include "gmsh/MTriangle.h"
#include "gmsh/MVertex.h"
#include "gmsh/SPoint3.h"
#include "gmsh/SVector3.h"
#include "gmsh/StringUtils.h"
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

void writeSettingsSC(FILE *file, PModel *model) {
  std::vector<unsigned int> inums;

  // Line 1
  unsigned int imodel = model->analysisModel();
  if (model->analysisVlasov() == 1) {
    imodel = 2;
  } else if (model->analysisTrapeze() == 1) {
    imodel = 3;
  }
  fprintf(file, "%8d\n\n", imodel);

  // Line 2
  writeNumbers(file, "%16e", model->curvatures());
  fprintf(file, "\n");

  // Line 3
  writeNumbers(file, "%16e", model->obliques());
  fprintf(file, "\n");

  // Line 4
  unsigned int ianalysis = 0;
  if (model->analysisThermal() == 1) {
    ianalysis = 1;
  }
  unsigned int ielem_flag = 0;
  unsigned int itrans_flag = 1;
  unsigned int itemp_flag = 0;
  inums = {ianalysis, ielem_flag, itrans_flag, itemp_flag};
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");

  // Line 5
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
  inums.push_back(model->gmodel()->indexMeshVertices(true, 0));
  inums.push_back(model->indexGmshElements());
  inums.push_back(model->cs()->getNumOfUsedMaterials());
  inums.push_back(inslave);
  inums.push_back(model->cs()->getNumOfUsedLayerTypes());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");
}

template <class T> void writeElementSC(FILE *file, PModel *model, T *elem, int mid) {
  std::vector<int> inums(9, 0);
  fprintf(file, "%8d%8d", model->gmodel()->getMeshElementIndex(elem), mid);
  for (int i = 0; i < elem->getNumVertices(); ++i) {
    if (i < 3) {
      inums[i] = elem->getVertex(i)->getIndex();
    } else {
      inums[i + 1] = elem->getVertex(i)->getIndex();
    }
  }
  writeNumbers(file, "%8d", inums);
}

void writeElementsSC(FILE *file, PModel *model) {
  // Write connectivity for each element
  for (auto f : model->dcel()->faces()) {
    if (f->gface() != nullptr) {
      for (auto elem : f->gface()->triangles) {
        writeElementSC(file, model, elem, f->layertype()->id());
      }
    }
  }
  fprintf(file, "\n");

  // Wirte local coordinate for each element
  std::vector<double> dnums;
  for (auto f : model->dcel()->faces()) {
    if (f->gface() != nullptr) {
      for (auto elem : f->gface()->triangles) {
        fprintf(file, "%8d", model->gmodel()->getMeshElementIndex(elem));
        dnums = {
          1.0, 0.0, 0.0, 
          f->localy2()[0], f->localy2()[1], f->localy2()[2], 
          0.0, 0.0, 0.0
        };
        writeNumbers(file, "%16e", dnums);
      }
    }
  }
  fprintf(file, "\n");
}

void writeMaterialSC(FILE *file, Material *m) {
  int intemp = 1;

  fprintf(file, "%8d", m->id());

  if (m->getType() == "isotropic") {
    fprintf(file, "%8d%8d\n", 0, intemp);
    fprintf(file, "%8d%16e\n", 1, m->getDensity());
    fprintf(file, "%16e%16e\n", m->getElastic()[0], m->getElastic()[1]);
  } else if (m->getType() == "orthotropic") {
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
  } else if (m->getType() == "anisotropic") {
    fprintf(file, "%8d%8d\n", 1, intemp);
    fprintf(file, "%8d%16e\n", 1, m->getDensity());
    int index = 0;
    for (int row = 1; row <= 6; ++row) {
      for (int col = 1; col <= (6 - row + 1); ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }
  }
  
}

void writeMaterialsSC(FILE *file, PModel *model) {
  for (auto lt : model->cs()->getUsedLayerTypes()) {
    fprintf(file, "%8d%8d%16e\n", lt->id(), lt->material()->id(), lt->angle());
  }
  fprintf(file, "\n");

  for (auto m : model->cs()->getUsedMaterials()) {
    writeMaterialSC(file, m);
  }
  fprintf(file, "\n");
}
