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

namespace {

bool usesIsotropicFailureCriteria(const std::string &materialType) {
  return materialType == "isotropic";
}

bool usesNonIsotropicFailureCriteria(const std::string &materialType) {
  return materialType == "transversely isotropic"
      || materialType == "orthotropic"
      || materialType == "engineering"
      || materialType == "anisotropic";
}

std::string normalizeMaterialSymmetryType(
  const std::string &rawType,
  const std::string &materialName
) {
  const std::string materialType = lowerString(trim(rawType));

  if (materialType == "lamina") {
    throw std::runtime_error(
      "Material '" + materialName
      + "': type='lamina' is no longer supported as a material symmetry type. "
        "Use type='transversely isotropic' for the material symmetry class "
        "and keep <lamina> only for material-thickness definitions."
    );
  }

  if (materialType == "transversely isotropic"
      || materialType == "transverse isotropic"
      || materialType == "transversely-isotropic"
      || materialType == "transverse-isotropic") {
    return "transversely isotropic";
  }

  if (materialType == "isotropic"
      || materialType == "orthotropic"
      || materialType == "engineering"
      || materialType == "anisotropic") {
    return materialType;
  }

  throw std::runtime_error(
    "Unsupported material type '" + trim(rawType) + "' for material '"
    + materialName + "'. Supported values: isotropic, transversely isotropic, "
      "orthotropic, engineering, anisotropic"
  );
}

std::string toInternalMaterialType(const std::string &symmetryType) {
  if (symmetryType == "transversely isotropic") {
    return "orthotropic";
  }
  return symmetryType;
}

std::string supportedFailureCriteriaFor(const std::string &materialType) {
  if (usesIsotropicFailureCriteria(materialType)) {
    return "1/max principal stress, 2/max principal strain, "
           "3/max shear stress/tresca, 4/max shear strain, 5/mises";
  }

  if (usesNonIsotropicFailureCriteria(materialType)) {
    return "1/max stress, 2/max strain, 3/tsai-hill, 4/tsai-wu, 5/hashin";
  }

  return "unknown because material type is unsupported";
}

int parseFailureCriterion(
  const std::string &rawValue,
  const std::string &materialType,
  const std::string &materialName
) {
  const std::string value = lowerString(trim(rawValue));
  const std::string context =
    "material '" + materialName + "' with type '" + materialType + "'";

  if (value.empty()) {
    throw std::runtime_error(
      "Empty failure criterion in " + context
    );
  }

  if (value == "1" || value == "max principal stress" || value == "max stress") {
    return 1;
  }

  if (value == "2" || value == "max principal strain" || value == "max strain") {
    return 2;
  }

  if (usesIsotropicFailureCriteria(materialType)) {
    if (value == "3" || value == "max shear stress" || value == "tresca") {
      return 3;
    }
    if (value == "4" || value == "max shear strain") {
      return 4;
    }
    if (value == "5" || value == "mises") {
      return 5;
    }
  }
  else if (usesNonIsotropicFailureCriteria(materialType)) {
    if (value == "3" || value == "tsai-hill") {
      return 3;
    }
    if (value == "4" || value == "tsai-wu") {
      return 4;
    }
    if (value == "5" || value == "hashin") {
      return 5;
    }
  }
  else {
    throw std::runtime_error(
      "Unsupported material type '" + materialType
      + "' while parsing failure criterion for material '" + materialName + "'"
    );
  }

  throw std::runtime_error(
    "Unsupported failure criterion '" + trim(rawValue) + "' for " + context
    + ". Supported values: " + supportedFailureCriteriaFor(materialType)
  );
}

}  // namespace

int readMaterialsFile(
  const std::string &fn_material_global, PModel *pmodel, bool required
  ) {
  xml_document<> xmlDocMaterials;
  std::ifstream fileMaterials{fn_material_global};
  if (!fileMaterials.is_open()) {
    if (required) {
      throw std::runtime_error(
        "cannot open material file '" + fn_material_global + "'"
      );
    }
    PLOG(warning) << "unable to open file: " << fn_material_global;
    return 1;
  } else {
    PLOG(info) << "reading materials file: " << fn_material_global;
  }

  std::vector<char> buffer{(std::istreambuf_iterator<char>(fileMaterials)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xmlDocMaterials.parse<0>(&buffer[0]);
  } catch (parse_error &e) {
    throw std::runtime_error(
      "XML parse error in '" + fn_material_global + "': " + e.what()
    );
  }

  xml_node<> *nodeMaterials{};
  nodeMaterials = xmlDocMaterials.first_node("materials");

  readMaterials(nodeMaterials, pmodel);

  return 0;
}

int readMaterials(const xml_node<> *nodeMaterials, PModel *pmodel) {
  if (!nodeMaterials) {
    throw std::runtime_error("Missing required XML element <materials>");
  }

  // -----------------------------------------------------------------
  // Read material data
  for (xml_node<> *nodeMaterial = nodeMaterials->first_node("material");
       nodeMaterial; nodeMaterial = nodeMaterial->next_sibling("material")) {
    readXMLElementMaterial(nodeMaterial, nodeMaterials, pmodel);
  }

  // -----------------------------------------------------------------
  // Read lamina data
  for (xml_node<> *nodeLamina = nodeMaterials->first_node("lamina"); nodeLamina;
       nodeLamina = nodeLamina->next_sibling("lamina")) {
    readXMLElementLamina(nodeLamina, nodeMaterials, pmodel);
  }

  return 0;
}

LayerType *ensureLayerType(
  Material *material,
  double angle,
  PModel *pmodel,
  const std::string &context
) {
  if (!material) {
    throw std::runtime_error(
      "Missing material while resolving layer type in " + context
    );
  }

  LayerType *layertype = pmodel->getLayerTypeByMaterialAngle(material, angle);
  if (!layertype) {
    layertype = new LayerType{0, material, angle};
    pmodel->addLayerType(layertype);
  }

  return layertype;
}

Strength readXMLElementStrength(const xml_node<> *p_xn_strength) {
  Strength strength;

  for (xml_node<> *p_xn_sp = p_xn_strength->first_node();
       p_xn_sp; p_xn_sp = p_xn_sp->next_sibling()) {

    std::string tag = p_xn_sp->name();

    const std::string ctx = "<strength>/<" + tag + ">";
    if ((tag == "t1") || (tag == "xt") || (tag == "x")) {
      strength.t1 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "t2") || (tag == "yt") || (tag == "y")) {
      strength.t2 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "t3") || (tag == "zt") || (tag == "z")) {
      strength.t3 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "c1") || (tag == "xc")) {
      strength.c1 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "c2") || (tag == "yc")) {
      strength.c2 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "c3") || (tag == "zc")) {
      strength.c3 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "s23") || (tag == "r")) {
      strength.s23 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "s13") || (tag == "t")) {
      strength.s13 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }
    else if ((tag == "s12") || (tag == "s")) {
      strength.s12 = parseRequiredDouble(p_xn_sp->value(), ctx);
    }

  }

  return strength;
}

Material *readXMLElementMaterial(const xml_node<> *p_xn_material, const xml_node<> * /*p_xn_mdb*/, PModel *pmodel) {

  Material *m;

  std::string materialName{};
  materialName = requireAttr(p_xn_material, "name", "<material>")->value();

    PLOG(debug) << "reading material: " + materialName;

  // Check if the material with the name exists
  m = pmodel->getMaterialByName(materialName);
  if (!m) {
    m = new Material{materialName};
    pmodel->addMaterial(m);
  }

  ensureLayerType(m, 0.0, pmodel, "<material name='" + materialName + "'>");

  // Type
  const std::string materialTypeContext =
    "<material name='" + materialName + "'>";
  const std::string materialSymmetryType = normalizeMaterialSymmetryType(
    requireAttr(p_xn_material, "type", materialTypeContext)->value(),
    materialName
  );
  const std::string materialType = toInternalMaterialType(materialSymmetryType);
  m->setSymmetryType(materialSymmetryType);
  m->setType(materialType);

  // Density — optional; defaults to 1.0 when absent
  double materialDensity{1.0};
  xml_node<> *nodeDensity{p_xn_material->first_node("density")};
  if (nodeDensity) {
    if (nodeDensity->value()[0] != '\0')
      materialDensity = parseRequiredDouble(nodeDensity->value(),
        "<density> in <material name='" + materialName + "'>");
  }
  m->setDensity(materialDensity);

  // Read elastic properties
  // -----------------------

  std::vector<double> materialElastic{1.0e-6, 0.0};  // Default for isotropic
  xml_node<> *nodeElastic = p_xn_material->first_node("elastic");

  if (nodeElastic) {

    if (materialType == "isotropic") {
      xml_node<> *p_xn_e = nodeElastic->first_node("e");
      if (p_xn_e) {
        materialElastic[0] = parseRequiredDouble(p_xn_e->value(),
          "<elastic>/<e> in <material name='" + materialName + "'>");
      }
      xml_node<> *p_xn_nu = nodeElastic->first_node("nu");
      if (p_xn_nu) {
        materialElastic[1] = parseRequiredDouble(p_xn_nu->value(),
          "<elastic>/<nu> in <material name='" + materialName + "'>");
      }
    }
    
    else if (materialSymmetryType == "transversely isotropic") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      double e1, e2, e3, g12, g13, g23, nu12, nu13, nu23;
      e1   = parseRequiredDouble(requireNode(nodeElastic, "e1",   ectx)->value(), ectx + "/<e1>");
      e2   = parseRequiredDouble(requireNode(nodeElastic, "e2",   ectx)->value(), ectx + "/<e2>");
      nu12 = parseRequiredDouble(requireNode(nodeElastic, "nu12", ectx)->value(), ectx + "/<nu12>");
      g12  = parseRequiredDouble(requireNode(nodeElastic, "g12",  ectx)->value(), ectx + "/<g12>");

      // Optional transverse-isotropic properties — keep the legacy defaults:
      //   e3   defaults to e2   (E3 = E2)
      //   nu13 defaults to nu12 (nu13 = nu12)
      //   g13  defaults to g12  (G13 = G12)
      //   g23  defaults to e3/(2*(1+nu23)) (isotropic 2-3 plane relation)
      // nu23 has NO physically derived default; 0.3 is an arbitrary fallback —
      //   always specify nu23 explicitly for accuracy.

      xml_node<> *p_xn_e3 = nodeElastic->first_node("e3");
      if (p_xn_e3) {
        e3 = parseRequiredDouble(p_xn_e3->value(), ectx + "/<e3>");
      } else {
        e3 = e2;  // transverse isotropy: E3 = E2
      }

      xml_node<> *p_xn_nu13 = nodeElastic->first_node("nu13");
      if (p_xn_nu13) {
        nu13 = parseRequiredDouble(p_xn_nu13->value(), ectx + "/<nu13>");
      } else {
        nu13 = nu12;  // transverse isotropy: nu13 = nu12
      }

      xml_node<> *p_xn_nu23 = nodeElastic->first_node("nu23");
      if (p_xn_nu23) {
        nu23 = parseRequiredDouble(p_xn_nu23->value(), ectx + "/<nu23>");
      } else {
        nu23 = 0.3;  // arbitrary fallback — not physically derived
        PLOG(warning) << "material '" + materialName
                         + "': <nu23> not specified, using default 0.3; "
                           "provide nu23 explicitly for accuracy";
      }

      xml_node<> *p_xn_g13 = nodeElastic->first_node("g13");
      if (p_xn_g13) {
        g13 = parseRequiredDouble(p_xn_g13->value(), ectx + "/<g13>");
      } else {
        g13 = g12;  // transverse isotropy: G13 = G12
      }

      xml_node<> *p_xn_g23 = nodeElastic->first_node("g23");
      if (p_xn_g23) {
        g23 = parseRequiredDouble(p_xn_g23->value(), ectx + "/<g23>");
      } else {
        g23 = e3 / (2.0 * (1 + nu23));  // isotropic 2-3 plane: G23 = E3/(2*(1+nu23))
      }

      materialElastic = {e1, e2, e3, g12, g13, g23, nu12, nu13, nu23};
    }

    else if (materialType == "orthotropic" || materialType == "engineering") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      for (std::string label : elasticLabelOrtho) {
        double value{parseRequiredDouble(
          requireNode(nodeElastic, label.c_str(), ectx)->value(),
          ectx + "/<" + label + ">")};
        materialElastic.push_back(value);
      }
    }

    else if (materialType == "anisotropic") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      for (std::string label : elasticLabelAniso) {
        double value{parseRequiredDouble(
          requireNode(nodeElastic, label.c_str(), ectx)->value(),
          ectx + "/<" + label + ">")};
        materialElastic.push_back(value);
      }
    }

  }

  m->setElastic(materialElastic);

  // Read thermal properties
  // -----------------------
  xml_node<> *p_xn_cte = p_xn_material->first_node("cte");

  if (p_xn_cte) {
    std::vector<double> cte;

    const std::string ctectx = "<cte> in <material name='" + materialName + "'>";
    if (materialType == "isotropic") {
      double _a = parseRequiredDouble(
        requireNode(p_xn_cte, "a", ctectx)->value(), ctectx + "/<a>");
      cte.push_back(_a);
    }

    else if (materialSymmetryType == "transversely isotropic"
          || materialType == "orthotropic"
          || materialType == "engineering") {
      for (auto _name : TAG_NAME_CTE_ORTHO) {
        double _value{parseRequiredDouble(
          requireNode(p_xn_cte, _name.c_str(), ctectx)->value(),
          ctectx + "/<" + _name + ">")};
        cte.push_back(_value);
      }
    }

    m->setCte(cte);

  }

  xml_node<> *p_xn_sh = p_xn_material->first_node("specific_heat");
  if (p_xn_sh) {
    double _sh = parseRequiredDouble(p_xn_sh->value(),
      "<specific_heat> in <material name='" + materialName + "'>");
    m->setSpecificHeat(_sh);
  }

  // Read failure criterion
  // ----------------------

  int fc = 0; // failure criterion
  xml_node<> *p_xn_fc = p_xn_material->first_node("failure_criterion");

  if (p_xn_fc) {
    fc = parseFailureCriterion(
      p_xn_fc->value(), materialSymmetryType, materialName
    );
  }
  m->setFailureCriterion(fc);

  double cl = 0.0; // characteristic length
  xml_node<> *p_xn_cl = p_xn_material->first_node("characteristic_length");
  if (p_xn_cl) {
    cl = parseRequiredDouble(p_xn_cl->value(),
      "<characteristic_length> in <material name='" + materialName + "'>");
  }
  m->setCharacteristicLength(cl);

  // Read strength properties
  // ------------------------

  xml_node<> *p_xn_strength = p_xn_material->first_node("strength");

  if (p_xn_strength) {
    Strength struct_strength;
    struct_strength = readXMLElementStrength(p_xn_strength);
    struct_strength._type = materialSymmetryType;
    m->setStrength(struct_strength);
  }

  return m;
}

Lamina *readXMLElementLamina(const xml_node<> *p_xn_lamina, const xml_node<> * /*p_xn_mdb*/, PModel *pmodel) {

  Lamina *l;

  std::string laminaName{};
  laminaName = requireAttr(p_xn_lamina, "name", "<lamina>")->value();

    PLOG(debug) << "reading lamina: " + laminaName;

  const std::string ctx = "<lamina name='" + laminaName + "'>";
  std::string lm{};
  lm = requireNode(p_xn_lamina, "material", ctx)->value();
  Material *p_laminaMaterial{};
  p_laminaMaterial = pmodel->getMaterialByName(lm);
  if (!p_laminaMaterial) {
    throw std::runtime_error(
      "cannot find material '" + lm + "' for lamina '" + laminaName + "'"
    );
  }
  ensureLayerType(p_laminaMaterial, 0.0, pmodel, ctx);

  double laminaThickness{};
  laminaThickness = parseRequiredDouble(
    requireNode(p_xn_lamina, "thickness", ctx)->value(),
    "<thickness> in <lamina name='" + laminaName + "'>");

  l = pmodel->getLaminaByName(laminaName);

  if (l == nullptr) {
    l = new Lamina(laminaName, p_laminaMaterial, laminaThickness);
    pmodel->addLamina(l);
  } else {
    l->setMaterial(p_laminaMaterial);
    l->setThickness(laminaThickness);
  }

  return l;
}