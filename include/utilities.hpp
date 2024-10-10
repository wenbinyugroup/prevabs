#pragma once

#include "declarations.hpp"
#include "PComponent.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PModel.hpp"
#include "globalConstants.hpp"
#include "PBaseLine.hpp"

#include "gmsh_mod/STensor3.h"
#include "rapidxml/rapidxml.hpp"

#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

using namespace rapidxml;

class PGeoArc;

class Message {
private:
  std::string m_s_file_name;
  int m_i_indent;
  int m_i_indent_inc;
  bool m_b_to_screen;

public:
  std::ofstream m_ofs;

  Message() : m_i_indent(0), m_i_indent_inc(2), m_b_to_screen(true) {}
  Message(std::string file_name) : m_s_file_name(file_name), m_i_indent(0), m_i_indent_inc(2), m_b_to_screen(true) {}

  std::string getFileName() { return m_s_file_name; }
  int getIndentInc() { return m_i_indent_inc; }
  bool isToScreen() { return m_b_to_screen; }

  // void setFileName(std::string);
  void setIndentInc(int);
  void setToScreen(bool);

  int openFile();
  int closeFile();

  void increaseIndent();
  void decreaseIndent();

  std::string message(const std::string &);

  void printPrompt(int, int = 0);
  int print(int, std::string);
  int print(int, std::stringstream &);
  void printBlank(int = 1);
  void printDivider(int, char);
  void printTitle();
  // int printInfo(std::string);
  // int printError(std::string);
  // int printDebug(std::string);
};




void printInfo(int, std::string);
void printWarning(int, std::string);
void printError(int, std::string);
// void printMessage(int, std::string, std::ofstream &, bool, int = 0);

// template <typename T, typename A>
// void writeVectorToFile(std::ofstream &, std::vector<T, A>);
void writeVectorToFile(std::ofstream &, std::vector<double>);
void writeVectorToFile(std::ofstream &, std::vector<int>);
void writeVectorToFile(std::ofstream &, std::vector<std::string>);
void writeVectorToFile(FILE *, std::vector<double>, std::string="%16e", bool=true);

void printVector(const std::vector<double> &);
void printVector(const std::vector<int> &);

int openFile(std::ifstream &, const std::string &);
int closeFile(std::ifstream &);
int parseXMLDoc(xml_document<> &, std::ifstream &, const std::string &);
int parseCSVString(const std::string &, std::vector<std::vector<std::string>> &);
// xml_node<> *getXMLRoot(std::ifstream &, const std::string &);

double deg2rad(double);
double rad2deg(double);
// double getDistanceSquare(const Point2 &, const Point2 &);
// double innerVV(const Vector2 &, const Vector2 &);
// Matrix2 outerVV(const Vector2&, const Vector2&);
// Vector2 productMV(const Matrix2 &, const Vector2 &);
// GeoConst getTurnDirection(const Vector2 &, const Vector2 &);
STensor3 getRotationMatrix(double, int, GeoConst = DEGREE);
std::vector<std::string> splitString(std::string, char);
std::vector<double> parseNumbersFromString(const std::string &);
std::vector<int> parseIntegersFromString(const std::string &);
std::string lowerString(const std::string &);
std::string upperString(const std::string &);
std::string removeChar(std::string, char = ' ');
std::vector<double> getDxyFromAngle(double, char = 'x', double = 1.0,
                                    bool = false);
void discretizeArcN(const PGeoArc &, int, Baseline *, PModel *);
void discretizeArcA(const PGeoArc &, double, Baseline *, PModel *);
// Basepoint *getBasepointByName(std::string, std::vector<Basepoint> &);
PDCELVertex *getPVertexByName(std::string, std::vector<PDCELVertex *> &);
Baseline *getBaselineByName(std::string, std::vector<Baseline> &);
Material *getMaterialByName(std::string, std::vector<Material> &);
std::vector<double> decodeStackSequence(std::string);

template <class T>
bool isInContainer(const std::list<T *> &container, T *element) {
  for (auto item : container) {
    if (item == element) {
      return true;
    }
  }
  return false;
}

void walk(const rapidxml::xml_node<>*, int indent = 0);
