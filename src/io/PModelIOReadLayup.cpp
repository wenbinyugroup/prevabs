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

#include "geo_types.hpp"

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

int readLayups(const xml_node<> *nodeLayups, PModel *pmodel) {
  MESSAGE_SCOPE(g_msg);

  PLOG(debug) << "in function: readLayups";

  for (xml_node<> *nodeLayup = nodeLayups->first_node("layup"); nodeLayup;
       nodeLayup = nodeLayup->next_sibling("layup")) {
    std::string layupName{};
    layupName = requireAttr(nodeLayup, "name", "<layup>")->value();
    PLOG(debug) << "reading layup: " + layupName;

    // method: optional; defaults to "layer list" when attribute is absent
    std::string layupMethod{"layer list"};
    xml_attribute<> *attrMethod = nodeLayup->first_attribute("method");
    if (attrMethod)
      layupMethod = lowerString(attrMethod->value());

    Layup *layup = new Layup(layupName);

    if ((layupMethod == "layer list") || (layupMethod == "ll")) {
      for (xml_node<> *nodeLayer = nodeLayup->first_node("layer"); nodeLayer;
           nodeLayer = nodeLayer->next_sibling("layer")) {
        std::string laminaName{};
        xml_attribute<> *attrLamina = nodeLayer->first_attribute("lamina");
        xml_attribute<> *attrLayup = nodeLayer->first_attribute("layup");
        std::string layerContext =
          "<layer> in <layup name='" + layupName + "'>";
        if (attrLamina && attrLayup) {
          throw std::runtime_error(
            layerContext + " cannot define both 'lamina' and 'layup'"
          );
        }
        if (!attrLamina && !attrLayup) {
          throw std::runtime_error(
            "Missing required XML attribute 'lamina' or 'layup' in "
            + layerContext
          );
        }
        if (attrLamina) {
          laminaName = attrLamina->value();

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

            angle = parseRequiredDouble(vAngleStack[0],
              "<layer> angle in layup '" + layupName + "'");
            if (vAngleStack.size() > 1)
              stack = parseRequiredInt(vAngleStack[1],
                "<layer> stack in layup '" + layupName + "'");
            else
              stack = 1;
          }

          Lamina *lamina;
          Material *p_material;
          LayerType *p_layertype;

          lamina = pmodel->getLaminaByName(laminaName);
          if (lamina == nullptr) {
            throw std::runtime_error(
              "cannot find lamina '" + laminaName + "' in layup '" + layupName + "'"
            );
          }

          p_material = lamina->getMaterial();
          if (p_material == nullptr) {
            throw std::runtime_error(
              "lamina '" + laminaName + "' in layup '" + layupName
              + "' has no material"
            );
          }

          p_layertype = ensureLayerType(
            p_material, angle, pmodel,
            "<layup name='" + layupName + "'>"
          );

          if (stack > 0) {
            layup->addLayer(lamina, angle, stack, p_layertype);
          }

          for (int i = 1; i <= stack; ++i)
            layup->addPly(lamina->getMaterial(), lamina->getThickness(), angle);
        }
        else {
          // handle sub-layup
          // must make sure the sub-layup appears before current layup
          std::string subLayupName{attrLayup->value()};
          Layup *subLayup = pmodel->getLayupByName(subLayupName);
          if (subLayup == nullptr) {
            throw std::runtime_error(
              "cannot find sub-layup '" + subLayupName
              + "' in layup '" + layupName + "'"
            );
          }

          std::vector<Layer> llayers{subLayup->getLayers()};
          std::vector<Ply> lplies{subLayup->getPlies()};
      
          for (auto it=llayers.begin(); it != llayers.end(); it++) {
            Lamina *subLamina = it->getLamina();
            if (subLamina == nullptr) {
              throw std::runtime_error(
                "sub-layup '" + subLayupName + "' in layup '" + layupName
                + "' contains a layer without lamina"
              );
            }

            Material *subMaterial = subLamina->getMaterial();
            if (subMaterial == nullptr) {
              throw std::runtime_error(
                "sub-layup '" + subLayupName + "' in layup '" + layupName
                + "' contains a lamina without material"
              );
            }

            LayerType *subLayerType = it->getLayerType();
            if (subLayerType == nullptr) {
              subLayerType = ensureLayerType(
                subMaterial, it->getAngle(), pmodel,
                "<layup name='" + layupName + "'>"
              );
            }

            layup->addLayer(
              subLamina, it->getAngle(), it->getStack(), subLayerType
            );
          }

          for (auto jt=lplies.begin(); jt != lplies.end(); jt++) {
            layup->addPly(jt->getMaterial(), jt->getThickness(), jt->getAngle());
          }
        }
      }
    }
    else if ((layupMethod == "stack sequence") || (layupMethod == "ss")) {
      const std::string ssctx = "<layup name='" + layupName + "'>";
      std::string laminaName{requireNode(nodeLayup, "lamina", ssctx)->value()};
      Lamina *lamina = pmodel->getLaminaByName(laminaName);
      if (lamina == nullptr) {
        throw std::runtime_error(
          "cannot find lamina '" + laminaName + "' in layup '" + layupName + "'"
        );
      }

      LayerType *p_lt = nullptr;
      Material *p_material;
      p_material = lamina->getMaterial();
      if (p_material == nullptr) {
        throw std::runtime_error(
          "lamina '" + laminaName + "' in layup '" + layupName
          + "' has no material"
        );
      }

      std::string layupCode{requireNode(nodeLayup, "code", ssctx)->value()};
      std::vector<double> anglesList{decodeStackSequence(layupCode)};
      for (auto angle : anglesList) {
        p_lt = ensureLayerType(p_material, angle, pmodel, ssctx);

        layup->addLayer(lamina, angle, 1, p_lt);
        layup->addPly(lamina->getMaterial(), lamina->getThickness(), angle);
      }
    }
    pmodel->addLayup(layup);
  }

  return 0;
}
