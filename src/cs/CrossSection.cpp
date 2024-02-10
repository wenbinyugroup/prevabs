#include "CrossSection.hpp"

#include "PComponent.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh/SPoint3.h"
#include "gmsh/STensor3.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

CrossSection::CrossSection(std::string name) {
  csname = name;
  // cstype = "general";
  csorigin = SPoint3{0.0, 0.0, 0.0};
  // csscale = 1.0;
  // csrotate = 0.0;
  // csrotatem = STensor3{1.0};
  // csmeshsize = 1.0;
  // cselementtype = "linear";
  cssegments = {};
  // csconnections = {};
  // csfillings = {};
}




















CrossSection::CrossSection(std::string name, PModel *pmodel) {
  _pmodel = pmodel;
  csname = name;
  // cstype = "general";
  csorigin = SPoint3{0.0, 0.0, 0.0};
  // csscale = 1.0;
  // csrotate = 0.0;
  // csrotatem = STensor3{1.0};
  // csmeshsize = 1.0;
  // cselementtype = "linear";
  cssegments = {};
  // csconnections = {};
  // csfillings = {};
}




















void CrossSection::printCrossSection() {
  std::cout << doubleLine80 << std::endl;
  std::cout << std::setw(16) << "CROSS SECTION" << std::setw(16) << csname
            << std::endl;
  // std::cout << std::setw(16) << "Type" << std::setw(16) << cstype << std::endl;
  // std::cout << std::setw(16) << "New Origin" << std::setw(16) << csorigin
  //           << std::endl;
  // std::cout << std::setw(16) << "Translate" << std::setw(16) << cstranslate[0]
  //           << std::setw(16) << cstranslate[1] << std::endl;
  // std::cout << std::setw(16) << "Scale" << std::setw(16) << csscale
  //           << std::endl;
  // std::cout << std::setw(16) << "Rotate" << std::setw(16) << csrotate
  //           << std::endl;
  // std::cout << std::setw(16) << "Rotate Matrix" << std::setw(16)
  //           << csrotatem[0, 0] << std::setw(16) << csrotatem[0, 1]
  //           << std::setw(16) << csrotatem[0, 2] << std::endl;
  // std::cout << std::setw(16) << " " << std::setw(16) << csrotatem[1, 0]
  //           << std::setw(16) << csrotatem[1, 1] << std::setw(16)
  //           << csrotatem[1, 2] << std::endl;
  // std::cout << std::setw(16) << " " << std::setw(16) << csrotatem[2, 0]
  //           << std::setw(16) << csrotatem[2, 1] << std::setw(16)
  //           << csrotatem[2, 2] << std::endl;
  // std::cout << std::setw(16) << "Mesh Size" << std::setw(16) << csmeshsize
  //           << std::endl;
  // std::cout << std::setw(16) << "Element Type" << std::setw(16) << cselementtype
  //           << std::endl;

  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "Materials" << std::setw(16)
            << csusedmaterials.size() << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Name" << std::endl;
  for (auto pm : csusedmaterials)
    std::cout << std::setw(16) << pm->id() << std::setw(16) << pm->getName()
              << std::endl;

  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "Layer Types" << std::setw(16)
            << csusedlayertypes.size() << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Material ID"
            << std::setw(16) << "Angle" << std::setw(16) << "Material Name"
            << std::endl;
  for (auto plt : csusedlayertypes)
    std::cout << plt << std::endl;

  std::cout << singleLine80 << std::endl;
  for (auto c : _components) {
    std::cout << std::setw(16) << "Component";
    if (c->isCyclic()) {
      std::cout << std::setw(16) << "cyclic" << std::endl;
    } else {
      std::cout << std::setw(16) << "acyclic" << std::endl;
    }
    std::cout << std::setw(16) << "Segments" << std::setw(16)
              << c->segments().size() << std::endl;
    std::cout << std::setw(16) << "Name" << std::setw(16) << "Baseline"
              << std::setw(32) << "Layup" << std::setw(16) << "Layup side"
              << std::setw(8) << "Level" << std::endl;
    // std::cout << singleLine80 << std::endl;
    for (auto s : c->segments())
      std::cout << s << std::endl;
  }
}




















LayerType *CrossSection::getUsedLayerTypeByMaterialAngle(Material *m, double angle) {
  for (auto lt : csusedlayertypes) {
    if (lt->material() == m && lt->angle() == angle) {
      return lt;
    }
  }

  return nullptr;
}



















LayerType *CrossSection::getUsedLayerTypeByMaterialNameAngle(std::string name, double angle) {
  for (auto lt : csusedlayertypes) {
    if (lt->material()->getName() == name && lt->angle() == angle) {
      return lt;
    }
  }

  return nullptr;
}




















void CrossSection::setOrigin(SPoint3 origin) { csorigin = origin; }

// void CrossSection::setTranslate(std::vector<double> translate) {
//   cstranslate = translate;
// }

// void CrossSection::setScale(double scale) { csscale = scale; }

// void CrossSection::setRotate(double rotate) { csrotate = rotate; }

// void CrossSection::setRotateMatrix(STensor3 rotatem) { csrotatem = rotatem; }

// void CrossSection::setMeshsize(double meshsize) { csmeshsize = meshsize; }

// void CrossSection::setElementtype(std::string elementtype) {
//   cselementtype = elementtype;
// }

void CrossSection::addUsedMaterial(Material *p_material) {
  csusedmaterials.push_back(p_material);
}

void CrossSection::addUsedLayerType(LayerType *p_layertype) {
  csusedlayertypes.push_back(p_layertype);
}

void CrossSection::addSegment(Segment segment) {
  cssegments.push_back(segment);
}

// void CrossSection::addConnection(Connection connection) {
//   csconnections.push_back(connection);
// }

// void CrossSection::addFilling(Filling filling) {
//   csfillings.push_back(filling);
// }

void CrossSection::addComponent(PComponent *component) {
  _components.push_back(component);
}

void CrossSection::sortComponents() { _components.sort(compareOrder); }




















void CrossSection::build(Message *pmessage) {
  i_indent++;
  pmessage->increaseIndent();

  // Build the overall shape of the cross section
  // Do not consider details inside each component/segment (layers)
  PLOG(info) << pmessage->message("building the cross section, step 1");


  for (auto cmp : _components) {

    cmp->build(pmessage);

    // _pmodel->dcel()->print_dcel();
    // for (auto sgm : cmp->segments()) {
    //   sgm->curveBase()->print(pmessage, 9);
    // }

    // Remove all half edge loops, excluding segments face boundaries
    _pmodel->dcel()->removeTempLoops();

    // Create new half edge loops, excluding segments face boundaries
    _pmodel->dcel()->createTempLoops();

    // For each inner loop, find and connect to the nearest loop
    // _pmodel->dcel()->linkHalfEdgeLoops();
  }

  if (config.debug) {
    // Print DCEL
    // _pmodel->dcel()->print_dcel();


    // Create Gmsh model and write Gmsh files for debugging

    _pmodel->plotGeoDebug(pmessage);
  }

  // for (auto cmp : _components) {
  //   for (auto sgm : cmp->segments()) {
  //     sgm->curveBase()->print(pmessage, 9);
  //   }
  // }

  // pmessage->printBlank();
  // pmessage->print(9, "current dcel");
  // _pmodel->dcel()->print_dcel();

  // Build details (mainly slice layers for each segment)
  pmessage->printBlank();
  PLOG(info) << pmessage->message("building the cross section, step 2");


  for (auto cmp : _components) {
    cmp->buildDetails(pmessage);
  }


  // _pmodel->dcel()->print_dcel();
  i_indent--;
  pmessage->decreaseIndent();

}
