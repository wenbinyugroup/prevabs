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

int readMaterialsFile(
  const std::string &fn_material_global, PModel *pmodel
  ) {
  xml_document<> xmlDocMaterials;
  std::ifstream fileMaterials{fn_material_global};
  if (!fileMaterials.is_open()) {
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
  // -----------------------------------------------------------------
  // Read material data
  for (xml_node<> *nodeMaterial = nodeMaterials->first_node("material");
       nodeMaterial; nodeMaterial = nodeMaterial->next_sibling("material")) {

    Material *m;

    m = readXMLElementMaterial(nodeMaterial, nodeMaterials, pmodel);

  }

  // -----------------------------------------------------------------
  // Read lamina data
  for (xml_node<> *nodeLamina = nodeMaterials->first_node("lamina"); nodeLamina;
       nodeLamina = nodeLamina->next_sibling("lamina")) {

    Lamina *l;

    l = readXMLElementLamina(nodeLamina, nodeMaterials, pmodel);

  }

  return 0;
}

Strength readXMLElementStrength(const xml_node<> *p_xn_strength) {
  Strength strength;

  for (xml_node<> *p_xn_sp = p_xn_strength->first_node();
       p_xn_sp; p_xn_sp = p_xn_sp->next_sibling()) {

    std::string tag = p_xn_sp->name();

    if ((tag == "t1") || (tag == "xt") || (tag == "x")) {
      strength.t1 = atof(p_xn_sp->value());
    }
    else if ((tag == "t2") || (tag == "yt") || (tag == "y")) {
      strength.t2 = atof(p_xn_sp->value());
    }
    else if ((tag == "t3") || (tag == "zt") || (tag == "z")) {
      strength.t3 = atof(p_xn_sp->value());
    }
    else if ((tag == "c1") || (tag == "xc")) {
      strength.c1 = atof(p_xn_sp->value());
    }
    else if ((tag == "c2") || (tag == "yc")) {
      strength.c2 = atof(p_xn_sp->value());
    }
    else if ((tag == "c3") || (tag == "zc")) {
      strength.c3 = atof(p_xn_sp->value());
    }
    else if ((tag == "s23") || (tag == "r")) {
      strength.s23 = atof(p_xn_sp->value());
    }
    else if ((tag == "s13") || (tag == "t")) {
      strength.s13 = atof(p_xn_sp->value());
    }
    else if ((tag == "s12") || (tag == "s")) {
      strength.s12 = atof(p_xn_sp->value());
    }

  }

  return strength;
}

Material *readXMLElementMaterial(const xml_node<> *p_xn_material, const xml_node<> * /*p_xn_mdb*/, PModel *pmodel) {
  MESSAGE_SCOPE(g_msg);

  Material *m;
  LayerType *p_layertype;

  std::string materialName{};
  materialName = requireAttr(p_xn_material, "name", "<material>")->value();

    PLOG(debug) << "reading material: " + materialName;

  // Check if the material with the name exists
  m = pmodel->getMaterialByName(materialName);
  if (!m) {
    m = new Material{materialName};
    p_layertype = new LayerType{0, m, 0.0};
    pmodel->addMaterial(m);
    pmodel->addLayerType(p_layertype);
  }

  // Type
  std::string materialType{};
  materialType = lowerString(
    requireAttr(p_xn_material, "type", "<material name='" + materialName + "'")->value()
  );
  m->setType(materialType);

  // Density — optional; defaults to 1.0 when absent
  double materialDensity{1.0};
  xml_node<> *nodeDensity{p_xn_material->first_node("density")};
  if (nodeDensity) {
    if (nodeDensity->value()[0] != '\0')
      materialDensity = atof(nodeDensity->value());
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
        materialElastic[0] = atof(p_xn_e->value());
      }
      xml_node<> *p_xn_nu = nodeElastic->first_node("nu");
      if (p_xn_nu) {
        materialElastic[1] = atof(p_xn_nu->value());
      }
      // for (std::string label : elasticLabelIso) {
      //   double value{atof(nodeElastic->first_node(label.c_str())->value())};
      //   materialElastic.push_back(value);
      // }
    }
    
    else if (materialType == "orthotropic" || materialType == "engineering") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      for (std::string label : elasticLabelOrtho) {
        double value{atof(requireNode(nodeElastic, label.c_str(), ectx)->value())};
        materialElastic.push_back(value);
      }
    }

    else if (materialType == "anisotropic") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      for (std::string label : elasticLabelAniso) {
        double value{atof(requireNode(nodeElastic, label.c_str(), ectx)->value())};
        materialElastic.push_back(value);
      }
    }

    else if (materialType == "lamina") {
      materialElastic.clear();
      const std::string ectx = "<elastic> in <material name='" + materialName + "'>";
      double e1, e2, e3, g12, g13, g23, nu12, nu13, nu23;
      e1   = atof(requireNode(nodeElastic, "e1",   ectx)->value());
      e2   = atof(requireNode(nodeElastic, "e2",   ectx)->value());
      nu12 = atof(requireNode(nodeElastic, "nu12", ectx)->value());
      g12  = atof(requireNode(nodeElastic, "g12",  ectx)->value());

      // Optional lamina properties — transverse isotropy defaults (2-3 plane):
      //   e3   defaults to e2   (E3 = E2)
      //   nu13 defaults to nu12 (nu13 = nu12)
      //   g13  defaults to g12  (G13 = G12)
      //   g23  defaults to e3/(2*(1+nu23)) (isotropic 2-3 plane relation)
      // nu23 has NO physically derived default; 0.3 is an arbitrary fallback —
      //   always specify nu23 explicitly for accuracy.

      xml_node<> *p_xn_e3 = nodeElastic->first_node("e3");
      if (p_xn_e3) {
        e3 = atof(p_xn_e3->value());
      } else {
        e3 = e2;  // transverse isotropy: E3 = E2
      }

      xml_node<> *p_xn_nu13 = nodeElastic->first_node("nu13");
      if (p_xn_nu13) {
        nu13 = atof(p_xn_nu13->value());
      } else {
        nu13 = nu12;  // transverse isotropy: nu13 = nu12
      }

      xml_node<> *p_xn_nu23 = nodeElastic->first_node("nu23");
      if (p_xn_nu23) {
        nu23 = atof(p_xn_nu23->value());
      } else {
        nu23 = 0.3;  // arbitrary fallback — not physically derived
        PLOG(warning) << "material '" + materialName
                         + "': <nu23> not specified, using default 0.3; "
                           "provide nu23 explicitly for accuracy";
      }

      xml_node<> *p_xn_g13 = nodeElastic->first_node("g13");
      if (p_xn_g13) {
        g13 = atof(p_xn_g13->value());
      } else {
        g13 = g12;  // transverse isotropy: G13 = G12
      }

      xml_node<> *p_xn_g23 = nodeElastic->first_node("g23");
      if (p_xn_g23) {
        g23 = atof(p_xn_g23->value());
      } else {
        g23 = e3 / (2.0 * (1 + nu23));  // isotropic 2-3 plane: G23 = E3/(2*(1+nu23))
      }

      materialElastic = {e1, e2, e3, g12, g13, g23, nu12, nu13, nu23};
      // materialType = "orthotropic";

    }
  }

  m->setElastic(materialElastic);

  // for (auto p : materialElastic) {
  //   std::cout << p << std::endl;
  // }

  // Read thermal properties
  // -----------------------
  xml_node<> *p_xn_cte = p_xn_material->first_node("cte");

  if (p_xn_cte) {
    std::vector<double> cte;

    const std::string ctectx = "<cte> in <material name='" + materialName + "'>";
    if (materialType == "isotropic") {
      double _a = atof(requireNode(p_xn_cte, "a", ctectx)->value());
      cte.push_back(_a);
    }

    else if (materialType == "orthotropic" || materialType == "engineering") {
      for (auto _name : TAG_NAME_CTE_ORTHO) {
        double _value{atof(requireNode(p_xn_cte, _name.c_str(), ctectx)->value())};
        cte.push_back(_value);
      }
    }

    m->setCte(cte);

  }

  xml_node<> *p_xn_sh = p_xn_material->first_node("specific_heat");
  if (p_xn_sh) {
    double _sh = atof(p_xn_sh->value());
    m->setSpecificHeat(_sh);
  }

  // Read failure criterion
  // ----------------------

  int fc = 0; // failure criterion
  xml_node<> *p_xn_fc = p_xn_material->first_node("failure_criterion");

  if (p_xn_fc) {

    std::string s_fc;
    s_fc = lowerString(p_xn_fc->value());

    if (s_fc == "max principal stress" || s_fc == "max stress" || s_fc == "1")
      fc = 1;

    else if (s_fc == "max principal strain" || s_fc == "max strain" ||
              s_fc == "2")
      fc = 2;

    else if (s_fc == "max shear stress" || s_fc == "tresca" ||
              s_fc == "tsai-hill" || s_fc == "3")
      fc = 3;

    else if (s_fc == "max shear strain" || s_fc == "tsai-wu" || s_fc == "4")
      fc = 4;

    else if (s_fc == "mises" || s_fc == "hashin" || s_fc == "5")
      fc = 5;

  }
  m->setFailureCriterion(fc);

  double cl = 0.0; // characteristic length
  xml_node<> *p_xn_cl = p_xn_material->first_node("characteristic_length");
  if (p_xn_cl) {
    cl = atof(p_xn_cl->value());
  }
  m->setCharacteristicLength(cl);

  // Read strength properties
  // ------------------------

  // std::vector<double> v_d_strength{};
  xml_node<> *p_xn_strength = p_xn_material->first_node("strength");

  if (p_xn_strength) {

    // New input
    Strength struct_strength;
    struct_strength = readXMLElementStrength(p_xn_strength);
    struct_strength._type = materialType;
    m->setStrength(struct_strength);

    // Old input
    // if (materialType == "isotropic") {

    //   if (fc == 1 || fc == 2) {
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xc")->value()));
    //   }

    //   else if (fc == 3 || fc == 4) {
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("s")->value()));
    //   }

    //   else if (fc == 5) {
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("x")->value()));
    //   }
    // }

    // else if (materialType == "orthotropic" ||
    //             materialType == "anisotropic") {

    //   if (fc == 1 || fc == 2 || fc == 4) {
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("yt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("zt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xc")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("yc")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("zc")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("r")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("t")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("s")->value()));
    //   }
      
    //   else if (fc == 3) {
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("x")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("y")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("z")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("r")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("t")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("s")->value()));
    //   }
      
    //   else if (fc == 5) {
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("yt")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("xc")->value()));
    //     v_d_strength.push_back(
    //         atof(p_xn_strength->first_node("yc")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("r")->value()));
    //     v_d_strength.push_back(atof(p_xn_strength->first_node("s")->value()));
    //   }
    // }

    // else if (materialType == "lamina") {

    //   if (fc == 1 || fc == 2 || fc == 4) {
    //     double xt, yt, zt, xc, yc, zc, rr, tt, ss;

    //     xt = atof(p_xn_strength->first_node("xt")->value());
    //     xc = atof(p_xn_strength->first_node("xc")->value());
    //     yt = atof(p_xn_strength->first_node("yt")->value());
    //     ss = atof(p_xn_strength->first_node("s")->value());

    //     xml_node<> *p_xn_yc = p_xn_strength->first_node("yc");
    //     if (p_xn_yc) yc = atof(p_xn_yc->value());
    //     else yc = yt;

    //     xml_node<> *p_xn_zt = p_xn_strength->first_node("zt");
    //     if (p_xn_zt) zt = atof(p_xn_zt->value());
    //     else zt = yt;

    //     xml_node<> *p_xn_zc = p_xn_strength->first_node("zc");
    //     if (p_xn_zc) zc = atof(p_xn_zc->value());
    //     else zc = zt;

    //     xml_node<> *p_xn_tt = p_xn_strength->first_node("t");
    //     if (p_xn_tt) tt = atof(p_xn_tt->value());
    //     else tt = ss;

    //     xml_node<> *p_xn_rr = p_xn_strength->first_node("r");
    //     if (p_xn_rr) rr = atof(p_xn_rr->value());
    //     else rr = (yt + yc) / 4;

    //     v_d_strength.push_back(xt);
    //     v_d_strength.push_back(yt);
    //     v_d_strength.push_back(zt);
    //     v_d_strength.push_back(xc);
    //     v_d_strength.push_back(yc);
    //     v_d_strength.push_back(zc);
    //     v_d_strength.push_back(rr);
    //     v_d_strength.push_back(tt);
    //     v_d_strength.push_back(ss);

    //   }

    //   else if (fc == 3) {

    //   }

    //   else if (fc == 5) {

    //   }

    // }
  }

  // m = pmodel->getMaterialByName(materialName);

  if (materialType == "lamina") {
    materialType = "orthotropic";
    m->setType("orthotropic");
  }

  // if (m == nullptr) {
  //   m = new Material{
  //       0,  materialName, materialType, materialDensity, materialElastic,
  //       fc, cl,           v_d_strength};
  //   p_layertype = new LayerType{0, m, 0.0};

  //   // pmodel->p_materials.push_back(m);
  //   // pmodel->p_layertypes.push_back(p_layertype);
  //   pmodel->addMaterial(m);
  //   pmodel->addLayerType(p_layertype);
  // }
  
  // else {
  //   m->setType(materialType);
  //   m->setDensity(materialDensity);
  //   m->setElastic(materialElastic);
  //   m->setFailureCriterion(fc);
  //   m->setCharacteristicLength(cl);
  //   m->setStrength(v_d_strength);
  // }

  // m->printMaterial();

  return m;
}

Lamina *readXMLElementLamina(const xml_node<> *p_xn_lamina, const xml_node<> * /*p_xn_mdb*/, PModel *pmodel) {
  MESSAGE_SCOPE(g_msg);

  Lamina *l;

  std::string laminaName{};
  laminaName = requireAttr(p_xn_lamina, "name", "<lamina>")->value();

    PLOG(debug) << "reading lamina: " + laminaName;

  const std::string ctx = "<lamina name='" + laminaName + "'>";
  std::string lm{};
  lm = requireNode(p_xn_lamina, "material", ctx)->value();
  Material *p_laminaMaterial{};
  p_laminaMaterial = pmodel->getMaterialByName(lm);

  double laminaThickness{};
  laminaThickness = atof(requireNode(p_xn_lamina, "thickness", ctx)->value());

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
