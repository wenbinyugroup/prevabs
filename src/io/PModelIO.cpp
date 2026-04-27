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

int readInputMain(const std::string &filenameCrossSection,
                  const std::string &filePath, PModel *pmodel) {


  readCrossSection(filenameCrossSection, filePath, pmodel);
  if (config.isRecovery()) {
    readInputDehomo(filenameCrossSection, filePath, pmodel);
  }

  return 0;
}

int writeFace(FILE *file, PDCELFace *face) {
  // name field migrated to PDCELFaceData; pass bcfg.model->faceData(f).name
  // to callers when this function is re-activated.

  PDCELHalfEdge *he;
  // Write the outer boundary
  if (face->outer() == nullptr) {
    fprintf(file, "unbounded face\n");
  } else {
    he = face->outer();
    do {
      fprintf(file, "%16e %16e\n", he->source()->y(), he->source()->z());
      he = he->next();
    } while (he != face->outer());
    fprintf(file, "\n");

    for (auto _inner : face->inners()) {
      fprintf(file, "inner boundary:\n");
      he = _inner;
      do {
        fprintf(file, "%16e %16e\n", he->source()->y(), he->source()->z());
        he = he->next();
      } while (he != _inner);
      fprintf(file, "\n");
    }
  }
  return 0;
}
