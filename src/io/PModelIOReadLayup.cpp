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
#include "plog.hpp"

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

int readLayups(const xml_node<> *nodeLayups, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();

  std::vector<Layup> tempLayups;

  for (xml_node<> *nodeLayup = nodeLayups->first_node("layup"); nodeLayup;
       nodeLayup = nodeLayup->next_sibling("layup")) {
    std::string layupName{};
    layupName = nodeLayup->first_attribute("name")->value();
    // std::cout << "[debug] reading layup: " << layupName << std::endl;
    PLOG(debug) << pmessage->message("reading layup: " + layupName);

    std::string layupMethod{"layer list"};
    xml_attribute<> *attrMethod = nodeLayup->first_attribute("method");
    if (attrMethod)
      layupMethod = lowerString(attrMethod->value());

    // if (debug) {
    //   std::cout << layupName << std::endl;
    //   std::cout << layupMethod << std::endl;
    // }

    Layup *layup = new Layup(layupName);

    if ((layupMethod == "layer list") || (layupMethod == "ll")) {
      for (xml_node<> *nodeLayer = nodeLayup->first_node("layer"); nodeLayer;
           nodeLayer = nodeLayer->next_sibling("layer")) {
        std::string laminaName{};
        std::string attrName{};
        attrName = nodeLayer->first_attribute()->name();
        if (attrName == "lamina") {
          laminaName = nodeLayer->first_attribute("lamina")->value();

          std::string angleStack{nodeLayer->value()};
          double angle{};
          int stack{};

          if (angleStack == "") {
            angle = 0.0;
            stack = 1;
          }
          else {
            std::vector<std::string> vAngleStack;
            vAngleStack = splitString(angleStack, ':');

            angle = atof(vAngleStack[0].c_str());
            if (vAngleStack.size() > 1)
              stack = atoi(vAngleStack[1].c_str());
            else
              stack = 1;
          }

          Lamina *lamina;
          Material *p_material;
          LayerType *p_layertype;

          lamina = pmodel->getLaminaByName(laminaName);
          if (lamina == nullptr) {
            std::cout << "[error] cannot find lamina: " << laminaName
                      << std::endl;
          }

          p_material = lamina->getMaterial();

          p_layertype = pmodel->getLayerTypeByMaterialAngle(p_material, angle);
          if (p_layertype == nullptr) {
            p_layertype = new LayerType{0, p_material, angle};
            pmodel->addLayerType(p_layertype);
          }

          if (stack > 0) {
            layup->addLayer(lamina, angle, stack, p_layertype);
          }

          for (int i = 1; i <= stack; ++i)
            layup->addPly(lamina->getMaterial(), lamina->getThickness(), angle);
        }
        else if (attrName == "layup") {
          // handle sub-layup
          // must make sure the sub-layup appears before current layup
          std::string layupName=nodeLayer->first_attribute("layup")->value();
          Layup *subLayup = pmodel->getLayupByName(layupName);

          std::vector<Layer> llayers{subLayup->getLayers()};
          std::vector<Ply> lplies{subLayup->getPlies()};
      
          for (auto it=llayers.begin(); it != llayers.end(); it++) {
            layup->addLayer(it->getLamina(), it->getAngle(), it->getStack(), it->getLayerType());
          }

          for (auto jt=lplies.begin(); jt != lplies.end(); jt++) {
            layup->addPly(jt->getMaterial(), jt->getThickness(), jt->getAngle());
          }
        }
      }
    }
    else if ((layupMethod == "stack sequence") || (layupMethod == "ss")) {
      std::string laminaName{nodeLayup->first_node("lamina")->value()};
      Lamina *lamina = pmodel->getLaminaByName(laminaName);

      LayerType *p_lt = nullptr;
      Material *p_material;
      p_material = lamina->getMaterial();

      std::string layupCode{nodeLayup->first_node("code")->value()};
      std::vector<double> anglesList{decodeStackSequence(layupCode)};
      for (auto angle : anglesList) {
        p_lt = pmodel->getLayerTypeByMaterialAngle(p_material, angle);

        if (p_lt == nullptr) {
          p_lt = new LayerType{0, p_material, angle};
          pmodel->addLayerType(p_lt);
        }

        layup->addLayer(lamina, angle, 1, p_lt);
        layup->addPly(lamina->getMaterial(), lamina->getThickness(), angle);
      }
    }
    pmodel->addLayup(layup);
  }

  pmessage->decreaseIndent();

  return 0;
}
