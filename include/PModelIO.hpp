#pragma once

#include "Material.hpp"
#include "PBaseLine.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PDataClasses.hpp"
#include "PModel.hpp"
#include "declarations.hpp"
#include "utilities.hpp"

#include "rapidxml/rapidxml.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
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

// ---------------------------------------------------------------------------
// XML access utilities
// ---------------------------------------------------------------------------

/** Require a child element; throw std::runtime_error when absent.
 *
 * Parameters:
 *   parent  - parent node to search under
 *   name    - element tag name
 *   context - human-readable description of what is being read (for errors)
 *
 * Returns:
 *   Non-null rapidxml::xml_node<>* pointing to the found child.
 */
inline rapidxml::xml_node<> *requireNode(
  const rapidxml::xml_node<> *parent, const char *name,
  const std::string &context
) {
  rapidxml::xml_node<> *n = parent->first_node(name);
  if (!n) {
    throw std::runtime_error(
      std::string("Missing required XML element <") + name + "> in " + context
    );
  }
  return n;
}

/** Require an attribute; throw std::runtime_error when absent.
 *
 * Parameters:
 *   node    - element node to search on
 *   name    - attribute name
 *   context - human-readable description of what is being read (for errors)
 *
 * Returns:
 *   Non-null rapidxml::xml_attribute<>* pointing to the found attribute.
 */
inline rapidxml::xml_attribute<> *requireAttr(
  const rapidxml::xml_node<> *node, const char *name,
  const std::string &context
) {
  rapidxml::xml_attribute<> *a = node->first_attribute(name);
  if (!a) {
    throw std::runtime_error(
      std::string("Missing required XML attribute '") + name + "' in " + context
    );
  }
  return a;
}

/** Parse a strict XML boolean value.
 *
 * Accepted true values: 1, true, yes, on
 * Accepted false values: 0, false, no, off
 */
inline bool parseXmlBoolValue(
  const std::string &value, const std::string &context
) {
  const std::string lowered = lowerString(trim(value));
  if (lowered == "1" || lowered == "true" || lowered == "yes"
      || lowered == "on") {
    return true;
  }
  if (lowered == "0" || lowered == "false" || lowered == "no"
      || lowered == "off") {
    return false;
  }
  throw std::runtime_error(
    "Invalid boolean value '" + trim(value) + "' in " + context
  );
}

/** Parse an integer-valued XML format version.
 *
 * Accepts integer-like legacy strings such as "1" and "1.0", but always
 * returns a discrete int so the caller can branch without floating compares.
 */
inline int parseFormatVersionValue(
  const std::string &value, const std::string &context
) {
  const std::string trimmed = trim(value);
  if (trimmed.empty()) {
    throw std::runtime_error("Missing format version in " + context);
  }

  std::size_t pos = 0;
  try {
    const int version = std::stoi(trimmed, &pos);
    if (pos == trimmed.size()) {
      return version;
    }
  } catch (const std::exception &) {
  }

  pos = 0;
  double parsed = 0.0;
  try {
    parsed = std::stod(trimmed, &pos);
  } catch (const std::exception &) {
    throw std::runtime_error(
      "Invalid format version '" + trimmed + "' in " + context
    );
  }

  if (pos != trimmed.size() || !std::isfinite(parsed)) {
    throw std::runtime_error(
      "Invalid format version '" + trimmed + "' in " + context
    );
  }

  double integral_part = 0.0;
  if (std::modf(parsed, &integral_part) != 0.0
      || integral_part
           < static_cast<double>((std::numeric_limits<int>::min)())
      || integral_part
           > static_cast<double>((std::numeric_limits<int>::max)())) {
    throw std::runtime_error(
      "Format version must be an integer value in " + context
      + " (got '" + trimmed + "')"
    );
  }

  return static_cast<int>(integral_part);
}

// ---------------------------------------------------------------------------
// Reading functions

/** @ingroup io
 * Top level function for reading inputs.
 */
int readInputMain(const std::string &, const std::string &, PModel *);

/** @ingroup io
 * Read the cross-section input file.
 */
int readCrossSection(const std::string &, const std::string &, PModel *);


/** @ingroup io
 * Read the geometry definitions.
 */
int readBaselines(const rapidxml::xml_node<> *, PModel *,
                  const std::string &, double,
                  double, double, double, double);
Baseline *readXMLElementLine(const rapidxml::xml_node<> *,
                             const rapidxml::xml_node<> *, PModel *);
void readLineTypeStraight(Baseline *, const rapidxml::xml_node<> *,
                          const rapidxml::xml_node<> *, PModel *);
void readLineTypeCircle(Baseline *, const rapidxml::xml_node<> *,
                        const rapidxml::xml_node<> *, PModel *);
void readLineTypeArc(Baseline *, const rapidxml::xml_node<> *,
                     const rapidxml::xml_node<> *, PModel *);
int readLineTypeAirfoil(Baseline *, const rapidxml::xml_node<> *,
                        const rapidxml::xml_node<> *, PModel *,
                        const double);
int readLineByJoin(Baseline *, const rapidxml::xml_node<> *,
                   const rapidxml::xml_node<> *, PModel *);
Baseline *findLineByName(const std::string &, const rapidxml::xml_node<> *,
                         PModel *);

/** @ingroup io
 * Read the point definitions from a file.
 */
void readPointsFromFile(const std::string &, PModel *, double);
PDCELVertex *readXMLElementPoint(const rapidxml::xml_node<> *,
                                 const rapidxml::xml_node<> *, PModel *);
PDCELVertex *findPointByName(const std::string &,
                             const rapidxml::xml_node<> *, PModel *);

/** @ingroup io
 * Read the material definitions from a file.
 */
int readMaterialsFile(const std::string &, PModel *);
int readMaterials(const rapidxml::xml_node<> *, PModel *);
Material *readXMLElementMaterial(const rapidxml::xml_node<> *,
                                 const rapidxml::xml_node<> *,
                                 PModel *);
Lamina *readXMLElementLamina(const rapidxml::xml_node<> *,
                             const rapidxml::xml_node<> *, PModel *);
LayerType *ensureLayerType(Material *, double, PModel *, const std::string &);

/** @ingroup io
 * Read the layup definitions.
 */
int readLayups(const rapidxml::xml_node<> *, PModel *);

PComponent *readXMLElementComponent(const rapidxml::xml_node<> *,
                                    std::vector<std::vector<std::string>> &,
                                    std::vector<Layup *> &, int &,
                                    CrossSection *, PModel *);
int readXMLElementComponentLaminate(PComponent *,
                                    const rapidxml::xml_node<> *,
                                    std::vector<std::vector<std::string>> &,
                                    std::vector<std::string> &,
                                    std::vector<Layup *> &, int &,
                                    CrossSection *, PModel *);
int readXMLElementComponentFilling(PComponent *,
                                   const rapidxml::xml_node<> *,
                                   CrossSection *, PModel *);


// Dehomogenization
/** @ingroup io
 * Read inputs for dehomogenization from the main xml file.
 */
int readInputDehomo(const std::string &, const std::string &, PModel *);
/** @ingroup io
 * Read data in the SG input file.
 */
int readSG(const std::string &, PModel *, const WriterConfig &);

/** @ingroup io
 * Read data from the dehomogenization output files.
 */
int readOutputDehomo(const std::string &, PModel *);


LoadCase readXMLElementLoadCase(const rapidxml::xml_node<> *,
                                const int &, const int &, PModel *);
int readXMLElementLoadCaseInclude(const rapidxml::xml_node<> *,
                                  const int &, const int &, PModel *);
int readLoadCasesFromCSV(const std::string &, const int &, const int &, const int &, PModel *);
int readVABSU(const std::string &, LocalState *);
int readVABSEle(const std::string &, LocalState *);
int readSCSn(const std::string &, LocalState *);
int readMsgFi(const std::string &, LocalState *, std::size_t);
// std::vector<std::vector<double>> readOutputDehomoElementSC(
//   const std::string &, const int &);
// std::vector<std::vector<double>> readOutputDehomoElementNodeVABS(
//   const std::string &, const int &, const std::vector<std::vector<int>> &);
// std::vector<std::vector<double>> readOutputDehomoElementNodeSC(
//   const std::string &, const int &, const std::vector<std::vector<int>> &);









// Writing functions

template <class T>
void writeNumbers(FILE *file, const char *fmt, const std::vector<T> &nums) {
  for (auto n : nums) {
    fprintf(file, fmt, n);
  }
  fprintf(file, "\n");
}

int writeFace(FILE *, PDCELFace *);

/** @ingroup io
 * Write the node block to a file.
 */
// void writeNodes(FILE *, PModel *);

/** @ingroup io
 * Write header settings block in VABS format to a file.
 */
void writeSettingsVABS(FILE *, PModel *, const WriterConfig &);

/** @ingroup io
 * Write the element block in VABS format to a file.
 */
// void writeElementsVABS(FILE *, PModel *);

/** @ingroup io
 * Write the material block in VABS format to a file.
 */
void writeMaterialsVABS(FILE *, PModel *);

void writeMaterialStrength(FILE *, Material *);

void writeSettingsSC(FILE *, PModel *, const WriterConfig &);
// void writeElementsSC(FILE *, PModel *);
void writeMaterialsSC(FILE *, PModel *);

void writeGmshElementData(std::ofstream &,
                          const std::vector<PElementNodeData> &,
                          const std::vector<std::string> &);


// Supplement files
void writeInterfaceNodes(PModel *);
void writeInterfacePairs(PModel *);
void writeNodeElements(PModel *);
