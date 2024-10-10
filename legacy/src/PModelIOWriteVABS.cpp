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
  inums.push_back(model->gmodel()->indexMeshVertices(true, 0));
  inums.push_back(model->indexGmshElements());
  inums.push_back(model->cs()->getNumOfUsedMaterials());
  writeNumbers(file, "%8d", inums);
  fprintf(file, "\n");
}









void writeNodes(FILE *file, PModel *model) {
  std::vector<GEntity *> gentities;
  model->gmodel()->getEntities(gentities);
  for (unsigned int i = 0; i < gentities.size(); ++i) {
    for (unsigned int j = 0; j < gentities[i]->mesh_vertices.size(); ++j) {
      if (gentities[i]->mesh_vertices[j]->getIndex() > 0) {
        fprintf(file, "%8d%16e%16e\n",
                gentities[i]->mesh_vertices[j]->getIndex(),
                gentities[i]->mesh_vertices[j]->y(),
                gentities[i]->mesh_vertices[j]->z());
      } else {
        continue;
      }
    }
  }
  fprintf(file, "\n");
}









template <class T> void writeElementVABS(FILE *file, PModel *model, T *elem) {
  std::vector<int> inums(9, 0);
  fprintf(file, "%8d", model->gmodel()->getMeshElementIndex(elem));
  for (int i = 0; i < elem->getNumVertices(); ++i) {
    if (i < 3) {
      inums[i] = elem->getVertex(i)->getIndex();
    } else {
      inums[i + 1] = elem->getVertex(i)->getIndex();
    }
  }
  writeNumbers(file, "%8d", inums);
}









void writeElementsVABS(FILE *file, PModel *model) {
  // Write connectivity for each element
  // for (auto fit = model->firstFace(); fit != model->lastFace(); ++fit) {
  for (auto f : model->dcel()->faces()) {
    if (f->gface() != nullptr) {
      for (auto elem : f->gface()->triangles) {
        writeElementVABS(file, model, elem);
      }
    }
  }
  fprintf(file, "\n");

  // Wirte layer type and theta_1 for each element
  // for (auto fit = model->firstFace(); fit != model->lastFace(); ++fit) {
  for (auto f : model->dcel()->faces()) {
    if (f->gface() != nullptr) {
      for (auto elem : f->gface()->triangles) {
        fprintf(file, "%8d%8d%16e\n",
                model->gmodel()->getMeshElementIndex(elem),
                // f->gface()->physicals[0],
                f->layertype()->id(), f->theta1());
      }
    }
  }
  fprintf(file, "\n");
}









void writeMaterialVABS(FILE *file, Material *m) {
  fprintf(file, "%8d", m->id());

  if (m->getType() == "isotropic") {
    fprintf(file, "%8d\n", 0);
    fprintf(file, "%16e%16e\n", m->getElastic()[0], m->getElastic()[1]);
  } else if (m->getType() == "orthotropic") {
    fprintf(file, "%8d\n", 1);
    int index = 0;
    for (int row = 1; row <= 3; ++row) {
      for (int col = 1; col <= 3; ++col) {
        fprintf(file, "%16e", m->getElastic()[index]);
        index++;
      }
      fprintf(file, "\n");
    }
  } else if (m->getType() == "anisotropic") {
    fprintf(file, "%8d\n", 1);
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
  else if ((type == "orthotropic") || (type == "anisotropic")) {
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








