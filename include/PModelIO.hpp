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

#include <cstdio>
#include <fstream>
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

using namespace rapidxml;

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
int readBaselines(const xml_node<> *, PModel *, const std::string &, double,
                  double, double, double, double);
Baseline *readXMLElementLine(const xml_node<> *, const xml_node<> *, PModel *);
void readLineTypeStraight(Baseline *, const xml_node<> *, const xml_node<> *, PModel *);
void readLineTypeCircle(Baseline *, const xml_node<> *, const xml_node<> *, PModel *);
void readLineTypeArc(Baseline *, const xml_node<> *, const xml_node<> *, PModel *);
int readLineTypeAirfoil(Baseline *, const xml_node<> *, const xml_node<> *, PModel *, const double);
int readLineByJoin(Baseline *, const xml_node<> *, const xml_node<> *, PModel *);
Baseline *findLineByName(const std::string &, const xml_node<> *, PModel *);

/** @ingroup io
 * Read the point definitions from a file.
 */
int readPointsFromFile(const std::string &, PModel *, double);
PDCELVertex *readXMLElementPoint(const xml_node<> *, const xml_node<> *,
                                 PModel *);
PDCELVertex *findPointByName(const std::string &, const xml_node<> *, PModel *);

/** @ingroup io
 * Read the material definitions from a file.
 */
int readMaterialsFile(const std::string &, PModel *);
int readMaterials(const xml_node<> *, PModel *);
Material *readXMLElementMaterial(const xml_node<> *, const xml_node<> *,
                                 PModel *);
Lamina *readXMLElementLamina(const xml_node<> *, const xml_node<> *, PModel *);

/** @ingroup io
 * Read the layup definitions.
 */
int readLayups(const xml_node<> *, PModel *);

PComponent *readXMLElementComponent(const xml_node<> *,
                                    std::vector<std::vector<std::string>> &,
                                    std::vector<Layup *> &, int &,
                                    CrossSection *, PModel *);
int readXMLElementComponentLaminate(PComponent *, const xml_node<> *,
                                    std::vector<std::vector<std::string>> &,
                                    std::vector<std::string> &,
                                    std::vector<Layup *> &, int &,
                                    CrossSection *, PModel *);
int readXMLElementComponentFilling(PComponent *, const xml_node<> *,
                                   CrossSection *, PModel *);

int addBaselinesFromAirfoil(const xml_node<> *, PModel *, const std::string &,
                            std::vector<std::pair<double, double>>, double,
                            double, double, double, double);
int addBaselineByPointAndAngle(PModel *, std::string, PDCELVertex *, double);
double getWebEnd(const xml_node<> *, const std::string &, double,
                 bool top = true);


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


LoadCase readXMLElementLoadCase(const xml_node<> *, const int &, const int &, PModel *);
int readXMLElementLoadCaseInclude(const xml_node<> *, const int &, const int &, PModel *);
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
