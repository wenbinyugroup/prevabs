#include "PModelIO.hpp"

#include "CrossSection.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "geometry_tolerance.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "pui.hpp"

#include "geo_types.hpp"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <fstream>
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

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel
#endif

namespace {

void configureModelGeometryTolerance(PModel *pmodel) {
  std::vector<SPoint2> points;
  points.reserve(pmodel->vertices().size());
  for (const auto *vertex : pmodel->vertices()) {
    points.emplace_back(vertex->y(), vertex->z());
  }

  std::vector<double> lamina_thicknesses;
  lamina_thicknesses.reserve(pmodel->laminas().size());
  for (auto *lamina : pmodel->laminas()) {
    lamina_thicknesses.push_back(lamina->getThickness());
  }

  const auto result = prevabs::geo::computeModelGeometryTolerance(
      points, lamina_thicknesses, REL_PREDICATE);
  if (!result.valid()) {
    throw std::runtime_error(
        "cannot determine geometry tolerance: model has no positive point "
        "spacing or lamina thickness");
  }

  setGeometryTolerance(result.tolerance);
  config.app.geo_tol = result.tolerance;
  PLOG(debug) << "model geometry tolerance: min_point_distance="
              << result.min_point_distance
              << ", min_lamina_thickness="
              << result.min_lamina_thickness
              << ", characteristic_length="
              << result.characteristic_length
              << ", rel_predicate=" << REL_PREDICATE
              << ", tolerance=" << result.tolerance;
}

unsigned int parseElementShapeValue(const std::string &value) {
  const std::string s = lowerString(trim(value));
  if (s == "3" || s == "tri" || s == "triangle") {
    return 3;
  }
  if (s == "4" || s == "quad" || s == "quadrilateral") {
    return 4;
  }
  throw std::runtime_error(
    "Invalid <element_shape> value: " + value
    + " (expected 3/tri/triangle or 4/quad/quadrilateral)"
  );
}

// ---------------------------------------------------------------------------
// XML node value readers — return default when node is absent or empty

int readOptionalIntNode(
  const rapidxml::xml_node<> *parent, const char *name, int default_val
) {
  rapidxml::xml_node<> *n = parent->first_node(name);
  if (n) {
    std::string s{n->value()};
    if (s[0] != '\0')
      return parseRequiredInt(s, std::string("<") + name + ">");
  }
  return default_val;
}

double readOptionalDoubleNode(
  const rapidxml::xml_node<> *parent, const char *name, double default_val
) {
  rapidxml::xml_node<> *n = parent->first_node(name);
  if (n) {
    std::string s{n->value()};
    if (s[0] != '\0')
      return parseRequiredDouble(s, std::string("<") + name + ">");
  }
  return default_val;
}

std::string readOptionalStringNode(
  const rapidxml::xml_node<> *parent, const char *name,
  const std::string &default_val
) {
  rapidxml::xml_node<> *n = parent->first_node(name);
  if (!n) return default_val;
  const std::string s{trim(n->value())};
  return s.empty() ? default_val : s;
}

bool readOptionalBoolAttr(
  const rapidxml::xml_node<> *node, const char *name, bool default_val,
  const std::string &context
) {
  rapidxml::xml_attribute<> *attr = node->first_attribute(name);
  if (!attr) return default_val;
  const std::string s{trim(attr->value())};
  return s.empty() ? default_val : parseXmlBoolValue(s, context);
}

// ---------------------------------------------------------------------------
// Geometry transform and mesh parameters returned from readGeneralSection

struct GeneralResult {
  double dx = 0, dy = 0, dz = 0;
  double sfactor = 1.0;
  double rangle = 0.0;
  double meshsize = 0.0;
};

// ---------------------------------------------------------------------------
// Section readers — each handles one top-level XML block

void readSettingsSection(const rapidxml::xml_node<> *xn_settings) {
  if (!xn_settings) return;
  PLOG_DEBUG_AT(geo) << "reading settings...";
  rapidxml::xml_node<> *p_xn_tolerance{xn_settings->first_node("tolerance")};
  if (p_xn_tolerance) {
    std::string ss{p_xn_tolerance->value()};
    if (ss[0] != '\0') {
      config.app.geo_tol = parseRequiredDouble(ss, "<settings>/<tolerance>");
      if (config.app.geo_tol <= 0.0) {
        throw std::runtime_error(
          "<settings>/<tolerance> must be positive, got '" + ss + "'"
        );
      }
    }
  }
}


void readAdaptiveThicknessSection(
  const rapidxml::xml_node<> *xn_adaptive
) {
  config.adaptive_thickness = AdaptiveThicknessConfig{};
  if (!xn_adaptive) return;

  auto &cfg = config.adaptive_thickness;
  cfg.enabled = readOptionalBoolAttr(
    xn_adaptive, "enabled", cfg.enabled,
    "<adaptive_thickness>@enabled"
  );

  rapidxml::xml_node<> *nodeReportOnly{
    xn_adaptive->first_node("report_only")
  };
  if (nodeReportOnly) {
    const std::string s{trim(nodeReportOnly->value())};
    if (!s.empty()) {
      cfg.report_only = parseXmlBoolValue(
        s, "<adaptive_thickness>/<report_only>"
      );
    }
  }

  cfg.mode = lowerString(readOptionalStringNode(
    xn_adaptive, "mode", cfg.mode
  ));
  if (cfg.mode != "linear") {
    throw std::runtime_error(
      "<adaptive_thickness>/<mode> currently supports only 'linear'"
    );
  }

  cfg.safety = readOptionalDoubleNode(
    xn_adaptive, "safety", cfg.safety
  );
  if (cfg.safety <= 0.0 || cfg.safety > 1.0) {
    throw std::runtime_error(
      "<adaptive_thickness>/<safety> must be in (0, 1]"
    );
  }

  cfg.transition_base_count = readOptionalIntNode(
    xn_adaptive, "transition_base_count", cfg.transition_base_count
  );
  cfg.repair_base_padding = readOptionalIntNode(
    xn_adaptive, "repair_base_padding", cfg.repair_base_padding
  );
  if (cfg.repair_base_padding < 0) {
    throw std::runtime_error(
      "<adaptive_thickness>/<repair_base_padding> must be non-negative"
    );
  }
  if (cfg.transition_base_count < 0) {
    throw std::runtime_error(
      "<adaptive_thickness>/<transition_base_count> must be non-negative"
    );
  }

  cfg.min_half_thickness = readOptionalDoubleNode(
    xn_adaptive, "min_half_thickness", cfg.min_half_thickness
  );
  if (cfg.min_half_thickness < 0.0) {
    throw std::runtime_error(
      "<adaptive_thickness>/<min_half_thickness> must be non-negative"
    );
  }

  rapidxml::xml_node<> *nodeTargets{
    xn_adaptive->first_node("target_segments")
  };
  if (nodeTargets) {
    const std::string s{trim(nodeTargets->value())};
    if (!s.empty()) {
      cfg.target_segments = splitString(s, ',');
      for (auto &segment : cfg.target_segments) {
        segment = trim(segment);
      }
    }
  }

  if (cfg.enabled) {
    PLOG(info) << "adaptive thickness configured: mode=" << cfg.mode
               << ", report_only=" << (cfg.report_only ? "true" : "false")
               << ", safety=" << cfg.safety
               << ", repair_base_padding="
               << cfg.repair_base_padding
               << ", transition_base_count="
               << cfg.transition_base_count;
  }
}




void readAnalysisSection(
  const rapidxml::xml_node<> *xn_sg, PModel *pmodel
) {
  PLOG_DEBUG_AT(geo) << "reading analysis...";

  int model_dim   = 1;
  int model       = 0;
  int flag_damping  = 0;
  int flag_thermal  = 0;
  int flag_curvature = 0;
  int flag_oblique  = 0;
  int flag_trapeze  = 0;
  int flag_vlasov   = 0;
  double k1 = 0.0, k2 = 0.0, k3 = 0.0;
  double cos11 = 1.0, cos21 = 0.0;

  rapidxml::xml_node<> *p_xn_analysis{xn_sg->first_node("analysis")};
  if (p_xn_analysis) {
    model_dim    = readOptionalIntNode(p_xn_analysis, "model_dim", model_dim);
    model        = readOptionalIntNode(p_xn_analysis, "model",     model);
    flag_damping = readOptionalIntNode(p_xn_analysis, "damping",   flag_damping);
    flag_thermal = readOptionalIntNode(p_xn_analysis, "thermal",   flag_thermal);
    flag_trapeze = readOptionalIntNode(p_xn_analysis, "trapeze",   flag_trapeze);
    flag_vlasov  = readOptionalIntNode(p_xn_analysis, "vlasov",    flag_vlasov);

    k1    = readOptionalDoubleNode(p_xn_analysis, "initial_twist",       k1);
    k2    = readOptionalDoubleNode(p_xn_analysis, "initial_curvature_2", k2);
    k3    = readOptionalDoubleNode(p_xn_analysis, "initial_curvature_3", k3);
    cos11 = readOptionalDoubleNode(p_xn_analysis, "oblique_y1",          cos11);
    cos21 = readOptionalDoubleNode(p_xn_analysis, "oblique_y2",          cos21);

    rapidxml::xml_node<> *p_xn_physics{p_xn_analysis->first_node("physics")};
    if (p_xn_physics) {
      std::string ss{p_xn_physics->value()};
      pmodel->setAnalysisPhysics(std::stoi(ss.c_str()));
    }
  }

  if (k1 != 0.0 || k2 != 0.0 || k3 != 0.0) flag_curvature = 1;
  if (cos11 != 1.0 || cos21 != 0.0) flag_oblique = 1;

  PLOG_DEBUG_AT(geo) << "finished reading analysis.";

  pmodel->setAnalysisModelDim(model_dim);
  pmodel->setAnalysisModel(model);
  pmodel->setAnalysisDamping(flag_damping);
  pmodel->setAnalysisThermal(flag_thermal);
  pmodel->setAnalysisCurvature(flag_curvature);
  pmodel->setAnalysisOblique(flag_oblique);
  pmodel->setAnalysisTrapeze(flag_trapeze);
  pmodel->setAnalysisVlasov(flag_vlasov);
  pmodel->setCurvatures(k1, k2, k3);
  pmodel->setObliques(cos11, cos21);
}




GeneralResult readGeneralSection(
  const rapidxml::xml_node<> *xn_general, PModel *pmodel
) {
  PLOG_DEBUG_AT(geo) << "reading general...";
  GeneralResult gen;

  rapidxml::xml_node<> *nodeTranslate{xn_general->first_node("translate")};
  if (nodeTranslate) {
    std::stringstream ss{nodeTranslate->value()};
    if (ss.str()[0] != '\0') ss >> gen.dy >> gen.dz;
  }

  gen.sfactor  = readOptionalDoubleNode(xn_general, "scale",     gen.sfactor);
  gen.rangle   = readOptionalDoubleNode(xn_general, "rotate",    gen.rangle);
  gen.meshsize = readOptionalDoubleNode(xn_general, "mesh_size", gen.meshsize);
  if (gen.meshsize < 0.0) {
    throw std::runtime_error(
      "<general>/<mesh_size> must be positive, got a negative value"
    );
  }

  int elementtype = 2;
  rapidxml::xml_node<> *nodeElementType{xn_general->first_node("element_type")};
  if (nodeElementType && nodeElementType->value()[0] != '\0') {
    std::string et{nodeElementType->value()};
    if (et == "linear" || et == "1") elementtype = 1;
    else if (et == "quadratic" || et == "2") elementtype = 2;
  }
  pmodel->setElementType(elementtype);

  rapidxml::xml_node<> *nodeElementShape{xn_general->first_node("element_shape")};
  if (nodeElementShape) {
    std::string es{trim(nodeElementShape->value())};
    if (!es.empty()) pmodel->setElementShape(parseElementShapeValue(es));
  }

  rapidxml::xml_node<> *nodeTransfiniteAuto{
    xn_general->first_node("transfinite_auto")
  };
  if (nodeTransfiniteAuto) {
    std::string s{trim(nodeTransfiniteAuto->value())};
    if (!s.empty())
      pmodel->setTransfiniteAuto(
        parseXmlBoolValue(s, "<general>/<transfinite_auto>")
      );
  }

  rapidxml::xml_node<> *nodeTransfiniteCornerAngle{
    xn_general->first_node("transfinite_corner_angle")
  };
  if (nodeTransfiniteCornerAngle) {
    std::string s{trim(nodeTransfiniteCornerAngle->value())};
    if (!s.empty()) pmodel->setTransfiniteCornerAngle(std::stod(s));
  }

  rapidxml::xml_node<> *nodeTransfiniteRecombine{
    xn_general->first_node("transfinite_recombine")
  };
  if (nodeTransfiniteRecombine) {
    std::string s{trim(nodeTransfiniteRecombine->value())};
    if (!s.empty())
      pmodel->setTransfiniteRecombine(
        parseXmlBoolValue(s, "<general>/<transfinite_recombine>")
      );
  }

  rapidxml::xml_node<> *nodeRecombine{xn_general->first_node("recombine")};
  if (nodeRecombine) {
    std::string s{trim(nodeRecombine->value())};
    if (!s.empty())
      pmodel->setRecombine(parseXmlBoolValue(s, "<general>/<recombine>"));
  }

  // Layered per-layer-offset build path (default ON; see globalVariables.hpp).
  rapidxml::xml_node<> *nodeLayeredOffset{
    xn_general->first_node("layered_offset")};
  if (nodeLayeredOffset) {
    std::string s{trim(nodeLayeredOffset->value())};
    if (!s.empty())
      config.layered_offset =
          parseXmlBoolValue(s, "<general>/<layered_offset>");
  }

  // Skip area construction over Clipper2 "dropped" base ranges
  // (default OFF; see globalVariables.hpp).
  rapidxml::xml_node<> *nodeSkipDroppedAreas{
    xn_general->first_node("skip_dropped_areas")};
  if (nodeSkipDroppedAreas) {
    std::string s{trim(nodeSkipDroppedAreas->value())};
    if (!s.empty())
      config.skip_dropped_areas =
          parseXmlBoolValue(s, "<general>/<skip_dropped_areas>");
  }

  // Per-layer offset boundary generation method "dir"/"seq"
  // (default "dir"; see globalVariables.hpp).
  rapidxml::xml_node<> *nodeLayerOffsetMethod{
    xn_general->first_node("layer_offset_method")};
  if (nodeLayerOffsetMethod) {
    std::string s{trim(nodeLayerOffsetMethod->value())};
    for (auto &c : s)
      c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    if (s == "dir" || s == "seq")
      config.layer_offset_method = s;
    else if (!s.empty())
      PLOG(warning) << "<general>/<layer_offset_method>: unknown value '" << s
                    << "', keeping '" << config.layer_offset_method << "'";
  }

  rapidxml::xml_node<> *nodeOffsetResampleMode{
    xn_general->first_node("offset_resample_mode")};
  if (nodeOffsetResampleMode) {
    std::string s{trim(nodeOffsetResampleMode->value())};
    for (auto &c : s)
      c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    if (s == "insert" || s == "replace")
      config.offset_resample_mode = s;
    else if (!s.empty())
      PLOG(warning) << "<general>/<offset_resample_mode>: unknown value '"
                    << s << "', keeping '" << config.offset_resample_mode
                    << "'";
  }

  rapidxml::xml_node<> *nodeRecombineAngle{
    xn_general->first_node("recombine_angle")
  };
  if (nodeRecombineAngle) {
    std::string s{trim(nodeRecombineAngle->value())};
    if (!s.empty()) pmodel->setRecombineAngle(std::stod(s));
  }

  double omega = readOptionalDoubleNode(xn_general, "omega", 1.0);
  pmodel->setOmega(omega);

  rapidxml::xml_node<> *p_xn_tol{xn_general->first_node("tolerance")};
  if (p_xn_tol) {
    std::string stol{p_xn_tol->value()};
    if (stol[0] != '\0') {
      config.app.tol = parseRequiredDouble(stol, "<general>/<tolerance>");
      if (config.app.tol <= 0.0) {
        throw std::runtime_error(
          "<general>/<tolerance> must be positive, got '" + stol + "'"
        );
      }
    }
  }
  std::stringstream ss_tol;
  ss_tol << config.app.tol;
  PLOG_DEBUG_AT(geo) << "tolerance = " + ss_tol.str();

  rapidxml::xml_node<> *p_xn_itf;
  p_xn_itf = xn_general->first_node("track_interface");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') pmodel->setInterfaceOutput(std::stoi(ss.c_str()));
  }

  p_xn_itf = xn_general->first_node("interface_theta1_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') pmodel->setInterfaceTheta1DiffThreshold(std::stod(ss));
  }

  p_xn_itf = xn_general->first_node("interface_theta3_diff_threshold");
  if (p_xn_itf) {
    std::string ss{p_xn_itf->value()};
    if (ss[0] != '\0') pmodel->setInterfaceTheta3DiffThreshold(std::stod(ss));
  }

  PLOG_DEBUG_AT(geo) << "finished reading general.";
  return gen;
}




void readGeometrySection(
  const rapidxml::xml_node<> *xn_sg,
  const rapidxml::xml_node<> *xn_include,
  const std::string &filePath,
  const std::string &cs_type,
  const GeneralResult &gen,
  PModel *pmodel
) {
  PLOG_DEBUG_AT(geo) << "finding includings...";
  PLOG_DEBUG_AT(geo) << "reading geometry...";

  if (xn_include) {
    rapidxml::xml_node<> *p_xn_include_bsl{xn_include->first_node("baseline")};
    if (p_xn_include_bsl) {
      if (cs_type == "general") {
        std::string filenameBaselines{
          filePath + p_xn_include_bsl->value() + ".xml"
        };
        PLOG_DEBUG_AT(geo) << "reading base line file: " + filenameBaselines;

        std::ifstream fileBaselines;
        openFile(fileBaselines, filenameBaselines);
        rapidxml::xml_document<> xmlDocBaselines;
        std::vector<char> buffer_bsl{
          (std::istreambuf_iterator<char>(fileBaselines)),
          std::istreambuf_iterator<char>()
        };
        buffer_bsl.push_back('\0');

        try {
          xmlDocBaselines.parse<0>(&buffer_bsl[0]);
        } catch (rapidxml::parse_error &e) {
          throw std::runtime_error(
            "XML parse error in '" + filenameBaselines + "': " + e.what()
          );
        }
        rapidxml::xml_node<> *nodeBaselines =
          xmlDocBaselines.first_node("baselines");
        readBaselines(
          nodeBaselines, pmodel, filePath,
          gen.dx, gen.dy, gen.dz, gen.sfactor, gen.rangle
        );
      } else if (cs_type == "airfoil") {
        throw std::runtime_error(
          "cs_type='airfoil' is no longer supported; "
          "use cs_type='general' with explicit <baselines>"
        );
      }
    }
  }

  rapidxml::xml_node<> *nodeBaselines = xn_sg->first_node("baselines");
  if (nodeBaselines) {
    readBaselines(
      nodeBaselines, pmodel, filePath,
      gen.dx, gen.dy, gen.dz, gen.sfactor, gen.rangle
    );
  }

  if (config.debug_level >= DebugLevel::geo) {
    PLOG_DEBUG_AT(geo) << " ";
    PLOG_DEBUG_AT(geo) << "summary of base lines (before transformation)";
    for (auto bsl : pmodel->baselines()) {
      bsl->print();
      PLOG_DEBUG_AT(geo) << " ";
    }
  }

  PLOG_DEBUG_AT(geo) << "transforming base geometry";
  for (auto v : pmodel->vertices()) {
    v->translate(gen.dx, gen.dy, gen.dz);
    v->scale(gen.sfactor);
    v->rotate(gen.rangle);
  }

  if (config.debug_level >= DebugLevel::geo) {
    PLOG_DEBUG_AT(geo) << " ";
    PLOG_DEBUG_AT(geo) << "summary of base lines (after transformation)";
    for (auto bsl : pmodel->baselines()) {
      bsl->print();
      PLOG_DEBUG_AT(geo) << " ";
    }
  }

  PLOG_DEBUG_AT(geo) << "finished reading geometry.";
}




void readMaterialsSection(
  const rapidxml::xml_node<> *xn_sg,
  const rapidxml::xml_node<> *xn_include,
  const std::string &filePath,
  PModel *pmodel
) {
  PLOG_DEBUG_AT(geo) << "reading materials...";

  // Default material database, read on every run independently of the input's
  // <include><material>. Its path is configurable via paths.material_db (a full
  // path to the .xml); when unset, fall back to the built-in
  // <exe-dir>/MaterialDB.xml. An explicitly configured path is required (a
  // missing file is a misconfiguration → error); the built-in default is
  // optional (missing = warn and continue).
  std::string fn_material_global = config.app.paths.material_db;
  const bool material_db_configured = !fn_material_global.empty();
  if (!material_db_configured) {
#ifdef __linux__
    char buffer2[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer2, sizeof(buffer2) - 1);
    if (count != -1) {
      buffer2[count] = '\0';
      fn_material_global = dirname(buffer2);
      fn_material_global = fn_material_global + "/";
    }
#elif _WIN32
    char buffer2[MAX_PATH];
    GetModuleFileNameA(NULL, buffer2, sizeof(buffer2));
    std::string s_fullpath{buffer2};
    std::vector<std::string> vs;
    vs = splitFilePath(s_fullpath);
    fn_material_global = vs[0];
#endif
    fn_material_global = fn_material_global + "MaterialDB.xml";
  }

  std::string fn_material_local = "";
  if (xn_include) {
    rapidxml::xml_node<> *xn_material{xn_include->first_node("material")};
    if (xn_material) {
      fn_material_local = filePath + xn_material->value() + ".xml";
    }
  }

  if (!fn_material_global.empty())
    readMaterialsFile(fn_material_global, pmodel, material_db_configured);
  // Local material path is explicitly provided via <include><material>; missing = error.
  if (!fn_material_local.empty()) readMaterialsFile(fn_material_local, pmodel, true);

  rapidxml::xml_node<> *p_xn_materials{xn_sg->first_node("materials")};
  if (p_xn_materials) readMaterials(p_xn_materials, pmodel);

  PLOG_DEBUG_AT(geo) << "finished reading materials.";
}




void readLayupsSection(
  const rapidxml::xml_node<> *xn_sg,
  const rapidxml::xml_node<> *xn_include,
  const std::string &filePath,
  PModel *pmodel
) {
  PLOG_DEBUG_AT(geo) << "reading layups...";

  if (xn_include) {
    for (auto *p_xn_include_lyp = xn_include->first_node("layup");
         p_xn_include_lyp;
         p_xn_include_lyp = p_xn_include_lyp->next_sibling("layup")) {
      std::string filenameLayups{
        filePath + p_xn_include_lyp->value() + ".xml"
      };
      PLOG(info) << "include layups file: " << filenameLayups;
      std::ifstream fileLayups;
      openFile(fileLayups, filenameLayups);
      rapidxml::xml_document<> xmlDocLayups;
      std::vector<char> buffer_lyp{
        (std::istreambuf_iterator<char>(fileLayups)),
        std::istreambuf_iterator<char>()
      };
      buffer_lyp.push_back('\0');
      try {
        xmlDocLayups.parse<0>(&buffer_lyp[0]);
      } catch (rapidxml::parse_error &e) {
        throw std::runtime_error(
          "XML parse error in '" + filenameLayups + "': " + e.what()
        );
      }
      rapidxml::xml_node<> *nodeLayups = xmlDocLayups.first_node("layups");
      readLayups(nodeLayups, pmodel);
    }
  }

  rapidxml::xml_node<> *nodeLayups = xn_sg->first_node("layups");
  if (nodeLayups) readLayups(nodeLayups, pmodel);

  PLOG_DEBUG_AT(geo) << "finished reading layups.";
}




void readComponentsSection(
  const rapidxml::xml_node<> *xn_sg,
  CrossSection *cs,
  PModel *pmodel,
  std::vector<Layup *> &p_layups,
  int &num_combined_layups
) {
  PLOG_DEBUG_AT(geo) << "reading components...";

  std::vector<std::vector<std::string>> tmp_dependents_all;

  for (auto xn_component = xn_sg->first_node("component");
       xn_component;
       xn_component = xn_component->next_sibling("component")) {
    PComponent *p_component = readXMLElementComponent(
      xn_component, tmp_dependents_all, p_layups, num_combined_layups, cs, pmodel
    );
    cs->addComponent(p_component);
  }
  PLOG_DEBUG_AT(geo) << "finished reading components.";

  // Resolve dependency names to pointers
  for (auto cmp : cs->components()) {
    for (auto depend_list : tmp_dependents_all) {
      if (cmp->name() == depend_list.front()) {
        for (int cni = 1; cni < static_cast<int>(depend_list.size()); ++cni) {
          for (auto cmp_d : cs->components()) {
            if (cmp_d->name() == depend_list[cni]) {
              cmp->addDependent(cmp_d);
              break;
            }
          }
        }
        break;
      }
    }
  }

  cs->sortComponents();
}

}  // namespace




int readCrossSection(const std::string &filenameCrossSection,
                     const std::string &filePath, PModel *pmodel) {
  rapidxml::xml_document<> xmlDocCrossSection;
  std::ifstream fileCrossSection{filenameCrossSection};
  if (!fileCrossSection.is_open()) {
    PLOG(error) << "unable to open file: " << filenameCrossSection;
    return 1;
  }
  PUI_INFO << "reading main input file: " << filenameCrossSection;

  std::vector<char> buffer{(std::istreambuf_iterator<char>(fileCrossSection)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xmlDocCrossSection.parse<0>(&buffer[0]);
  } catch (rapidxml::parse_error &e) {
    throw std::runtime_error(
      "XML parse error in '" + filenameCrossSection + "': " + e.what()
    );
  }

  rapidxml::xml_node<> *p_xn_sg{xmlDocCrossSection.first_node("cross_section")};
  if (!p_xn_sg) p_xn_sg = xmlDocCrossSection.first_node("sg");
  if (!p_xn_sg) {
    throw std::runtime_error("Missing root XML element <cross_section> or <sg>");
  }

  std::string csName{requireAttr(p_xn_sg, "name", "root element")->value()};
  std::string cs_type{"general"};
  if (p_xn_sg->first_attribute("type")) {
    cs_type = p_xn_sg->first_attribute("type")->value();
  }
  PLOG_DEBUG_AT(geo) << "find cross-section: " + csName;

  CrossSection *cs = new CrossSection(csName);

  int format_version{1};
  rapidxml::xml_attribute<> *p_xa_fmt{p_xn_sg->first_attribute("format")};
  if (p_xa_fmt) {
    std::string ss{p_xa_fmt->value()};
    if (ss[0] != '\0') {
      format_version = parseFormatVersionValue(ss, "root attribute 'format'");
    }
  }

  rapidxml::xml_node<> *xn_include{p_xn_sg->first_node("include")};

  readSettingsSection(p_xn_sg->first_node("settings"));
  readAdaptiveThicknessSection(p_xn_sg->first_node("adaptive_thickness"));
  readAnalysisSection(p_xn_sg, pmodel);

  rapidxml::xml_node<> *xn_general{p_xn_sg->first_node("general")};
  if (!xn_general) {
    throw std::runtime_error("Missing required XML element <general>");
  }
  GeneralResult gen = readGeneralSection(xn_general, pmodel);

  readGeometrySection(p_xn_sg, xn_include, filePath, cs_type, gen, pmodel);
  readMaterialsSection(p_xn_sg, xn_include, filePath, pmodel);
  readLayupsSection(p_xn_sg, xn_include, filePath, pmodel);
  configureModelGeometryTolerance(pmodel);

  std::vector<Layup *> p_layups{};
  int num_combined_layups = 0;
  readComponentsSection(p_xn_sg, cs, pmodel, p_layups, num_combined_layups);

  // Set default mesh size as the smallest layer thickness
  double meshsize = gen.meshsize;
  if (meshsize == 0.0) {
    PLOG(info) << "use default mesh size";
    for (auto p_layup : p_layups) {
      for (auto layer : p_layup->getLayers()) {
        double thickness = layer.getLamina()->getThickness();
        int stack = layer.getStack();
        if (meshsize > thickness * stack || meshsize == 0.0)
          meshsize = thickness * stack;
      }
    }
  }
  pmodel->setGlobalMeshSize(meshsize);

  pmodel->setCrossSection(cs);

  return 0;
}
