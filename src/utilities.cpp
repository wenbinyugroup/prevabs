#include "utilities.hpp"

#include "version.h"

#include "PComponent.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"

#include "geo_types.hpp"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace rapidxml;

int Message::openFile() {
  m_ofs.open(m_s_file_name);
  return 1;
}

int Message::closeFile() {
  m_ofs.close();
  return 1;
}

void Message::setIndentInc(int i_indent_inc) {
  m_i_indent_inc = i_indent_inc;
  return;
}

void Message::setToScreen(bool b_to_screen) {
  m_b_to_screen = b_to_screen;
  return;
}

void Message::increaseIndent() {
  m_i_indent += m_i_indent_inc;
  return;
}

void Message::decreaseIndent() {
  m_i_indent -= m_i_indent_inc;
  if (m_i_indent < 0) {
    m_i_indent = 0;
  }
  return;
}

std::string Message::message(const std::string &str) {
  std::string s_msg{""};

  s_msg += std::string(m_i_indent, ' ');
  s_msg += "- ";
  s_msg += str;

  return s_msg;
}

void Message::print(const std::string &msg) {
  std::string line = std::string(m_i_indent, ' ') + "- " + msg;
  m_ofs << line << std::endl;
  if (m_b_to_screen) std::cout << line << std::endl;
}

void Message::warn(const std::string &msg) {
  std::string line = std::string(m_i_indent, ' ') + "[warning] " + msg;
  m_ofs << line << std::endl;
  if (m_b_to_screen) std::cout << line << std::endl;
}

void Message::error(const std::string &msg) {
  std::string line = std::string(m_i_indent, ' ') + "[error] " + msg;
  m_ofs << line << std::endl;
  if (m_b_to_screen) std::cout << line << std::endl;
}

void Message::printPrompt(int i_type, int indent_override) {
  std::string s_prepend{""};
  if (indent_override == 0) {
    s_prepend += std::string(m_i_indent, ' ');
  } else {
    s_prepend += std::string(indent_override, ' ');
  }

  switch (i_type) {
    case 1:  // infomation
      s_prepend += "- ";
      break;
    case 2:  // error
      s_prepend += "X [error] ";
      break;
    case 9:  // debug
      s_prepend += "> [debug] ";
      break;
    default:
      s_prepend += "";
  }

  m_ofs << s_prepend;
  if (m_b_to_screen) {
    std::cout << s_prepend;
  }
  return;
}

int Message::print(int i_type, std::string s_message) {
  std::string s_prepend{""};
  s_prepend += std::string(m_i_indent, ' ');

  switch (i_type) {
    case 1:  // infomation
      s_prepend += "- ";
      break;
    case 2:  // error
      s_prepend += "X [error] ";
      break;
    case 9:  // debug
      s_prepend += "> [debug] ";
      break;
    default:
      s_prepend += "";
  }

  m_ofs << s_prepend << s_message << std::endl;
  if (m_b_to_screen) {
    std::cout << s_prepend << s_message << std::endl;
  }
  return 1;
}

int Message::print(int i_type, std::stringstream &ss_message) {
  print(i_type, ss_message.str());
  // std::string s_prepend{""};
  // s_prepend += std::string(m_i_indent, ' ');

  // switch (i_type) {
  //   case 1:  // infomation
  //     s_prepend += "- ";
  //     break;
  //   case 2:  // error
  //     s_prepend += "X [error] ";
  //     break;
  //   case 9:  // debug
  //     s_prepend += "> [debug] ";
  //     break;
  //   default:
  //     s_prepend += "";
  // }

  // m_ofs << s_prepend << ss_message.str() << std::endl;
  // if (m_b_to_screen) {
  //   std::cout << s_prepend << ss_message.str() << std::endl;
  // }
  return 1;
}

void Message::printBlank(int n) {
  for (int i = 0; i < n; i++) {
    print(0, "");
  }
  return;
}

void Message::printDivider(int length, char symbol) {
  std::string div(length, symbol);
  print(0, div);

  return;
}

void Message::printTitle() {
  printBlank();
  printDivider(40, '=');
  printBlank();
  std::string _ver_string{VERSION_STRING};
  print(0, ("  PreVABS " + _ver_string));
  printBlank();
  print(0, ("  (For VABS " + vabs_version + " and SwiftComp " + sc_version + ")"));
  printBlank();
  printDivider(40, '=');
  printBlank();

  return;
}

// int Message::printInfo(std::string s_message) {
//   return 1;
// }

// int Message::printError(std::string s_message) {
//   return 1;
// }

// int Message::printDebug(std::string s_message) {
//   return 1;
// }

void printInfo(int indent, std::string s_message) {
  std::string s_indent(indent * 2, ' ');
  std::cout << s_indent << "- " << s_message << std::endl;
  return;
}

void printWarning(int indent, std::string s_message) {
  std::string s_indent(indent, ' ');
  std::cout << s_indent << markWarning << " " << s_message << std::endl;
  return;
}

void printError(int indent, std::string s_message) {
  std::string s_indent(indent, ' ');
  std::cout << s_indent << markError << " " << s_message << std::endl;
  return;
}

int convertSizeTToInt(size_t value) {
    // Check if the value is within the range of `int`
    if (value > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("size_t value exceeds the maximum value of int");
    }
    
    // Safe conversion since the value is within range
    return static_cast<int>(value);
}

void writeVectorToFile(std::ofstream &ofs, std::vector<double> v) {
  for (auto n : v) {
    ofs << std::setw(16) << n;
  }
  return;
}

void writeVectorToFile(std::ofstream &ofs, std::vector<int> v) {
  for (auto n : v) {
    ofs << std::setw(8) << n;
  }
  return;
}

void writeVectorToFile(std::ofstream &ofs, std::vector<std::string> v) {
  for (auto n : v) {
    ofs << n << " ";
  }
  return;
}

void writeVectorToFile(FILE *file, std::vector<double> v, std::string fmt, bool newline) {
  for (auto n : v) {
    fprintf(file, fmt.c_str(), n);
  }
  if (newline) fprintf(file, "\n");
  return;
}

void printVector(const std::vector<double> &v) {
  for (auto n : v) {
    std::cout << n << " ";
  }
  std::cout << std::endl;
  return;
}

void printVector(const std::vector<int> &v) {
  for (auto n : v) {
    std::cout << n << " ";
  }
  std::cout << std::endl;
  return;
}

int openFile(std::ifstream &ifs, const std::string &file_name) {
  ifs.open(file_name);
  if (!ifs.is_open()) {
    std::cout << "unable to open file: " << file_name << std::endl;
    return 1;
  } else {
    // printInfo(i_indent, "file opened");
  }
  return 0;
}

int closeFile(std::ifstream &ifs) {
  ifs.close();
  // std::cout << "file closed\n";
  // printInfo(i_indent, "file opened");
  return 0;
}

int parseXMLDoc(xml_document<> &xd, std::ifstream &ifs, const std::string &fn) {
  // xml_document<> xd;

  // std::cout << "create buffer" << std::endl;
  std::vector<char> buffer{(std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  // std::cout << &buffer[0] << std::endl;

  // std::cout << "parse doc" << std::endl;
  try {
    xd.parse<0>(&buffer[0]);
  } catch (parse_error &e) {
    std::cout << markError << "unable to parse the file: " << fn
              << std::endl;
    std::cerr << e.what() << std::endl;
    // std::cout << e.where() << std::endl;
  }
  // std::cout << xd.first_node()->name() << std::endl;
  return 0;
}

int parseCSVString(const std::string &fn, std::vector<std::vector<std::string>> &s_data) {
  // std::cout << "readCSVString()...\n";

  std::ifstream ifs;
  // if (!ifs.is_open()) {
  //   std::cout << "unable to open file: " << fn << std::endl;
  //   return 1;
  // } else {
  //   std::cout << "reading csv file " << fn << "...\n";
  // }
  openFile(ifs, fn);

  std::string line;
  while (getline(ifs, line)) {
    std::stringstream ss(line);
    std::string _tmp;
    std::vector<std::string> data_line;
    while (getline(ss, _tmp, ',')) {
      // std::cout << _tmp << "  ";
      data_line.push_back(_tmp);
    }
    // std::cout << "\n";
    s_data.push_back(data_line);
  }
  // ifs.close();
  closeFile(ifs);

  return 0;
}

// xml_node<> *getXMLRoot(std::ifstream &ifs, const std::string &fn) {
//   xml_document<> xd;

//   std::cout << "create buffer" << std::endl;
//   std::vector<char> buffer{(std::istreambuf_iterator<char>(ifs)),
//                            std::istreambuf_iterator<char>()};
//   buffer.push_back('\0');

//   // std::cout << &buffer[0] << std::endl;

//   std::cout << "parse doc" << std::endl;
//   try {
//     xd.parse<0>(&buffer[0]);
//   } catch (parse_error &e) {
//     std::cout << markError << "unable to parse the file: " << fn
//               << std::endl;
//     std::cerr << e.what() << std::endl;
//     // std::cout << e.where() << std::endl;
//   }
//   // std::cout << xd;

//   std::cout << "get root" << std::endl;
//   xml_node<> *xn_root{xd.first_node("baselines")};
//   // xn_root = xd.first_node(rn);
//   return xn_root;
// }

double deg2rad(double degree) { return degree * PI / 180.0; }

double rad2deg(double radian) { return radian * 180.0 / PI; }

// double getDistanceSquare(const Point2 &p1, const Point2 &p2) {
//   return ((p2.x() - p1.x()) * (p2.x() - p1.x()) +
//           (p2.y() - p1.y()) * (p2.y() - p1.y()));
// }

// double innerVV(const Vector2 &v1, const Vector2 &v2) {
//   return (v1[0] * v2[0] + v1[1] * v2[1]);
// }

// Matrix2 outerVV(const Vector2& v1, const Vector2& v2)
// {
//     return Matrix2{ v1[0]*v2[0], v1[0]*v2[1], v1[1]*v2[0], v1[1]*v2[1] };
// }

// Vector2 productMV(const Matrix2 &m, const Vector2 &v) {
//   return Vector2{m[0][0] * v[0] + m[0][1] * v[1],
//                  m[1][0] * v[0] + m[1][1] * v[1]};
// }

// GeoConst getTurnDirection(const Vector2 &v1, const Vector2 &v2) {
//   double cross3;
//   cross3 = v1[0] * v2[1] - v1[1] * v2[0];
//   if (cross3 < 0.0)
//     return RIGHT_TURN;
//   else if (cross3 > 0.0)
//     return LEFT_TURN;
// }

int parseRequiredInt(const std::string &raw, const std::string &context) {
  const std::string value = trim(raw);
  if (value.empty()) {
    throw std::runtime_error("Missing integer value in " + context);
  }
  std::size_t pos = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(value, &pos);
  } catch (const std::exception &) {
    throw std::runtime_error(
      "Invalid integer value '" + value + "' in " + context
    );
  }
  if (pos != value.size()) {
    throw std::runtime_error(
      "Invalid integer value '" + value + "' in " + context
    );
  }
  return parsed;
}

double parseRequiredDouble(const std::string &raw, const std::string &context) {
  const std::string value = trim(raw);
  if (value.empty()) {
    throw std::runtime_error("Missing numeric value in " + context);
  }
  std::size_t pos = 0;
  double parsed = 0.0;
  try {
    parsed = std::stod(value, &pos);
  } catch (const std::exception &) {
    throw std::runtime_error(
      "Invalid numeric value '" + value + "' in " + context
    );
  }
  if (pos != value.size()) {
    throw std::runtime_error(
      "Invalid numeric value '" + value + "' in " + context
    );
  }
  if (!std::isfinite(parsed)) {
    throw std::runtime_error(
      "Invalid numeric value '" + value + "' in " + context
    );
  }
  return parsed;
}

STensor3 getRotationMatrix(double angle, int direction, GeoConst unit) {
  STensor3 rm;

  if (unit == DEGREE)
    angle = deg2rad(angle);

  if (direction == -1)
    angle = -angle;

  rm.set_m22(cos(angle));
  rm.set_m23(-sin(angle));
  rm.set_m32(sin(angle));
  rm.set_m33(cos(angle));

  return rm;
}

std::vector<std::string> splitString(std::string str, char delimiter) {
  std::vector<std::string> vSplit{};
  std::stringstream ss{str};
  std::string token;

  while (getline(ss, token, delimiter)) {
    vSplit.push_back(token);
  }

  return vSplit;
}

std::vector<double> parseNumbersFromString(const std::string &s) {
  std::vector<std::string> vs{splitString(s, ' ')};
  std::vector<double> vd;
  for (int i = 0; i < vs.size(); ++i) {
    if (vs[i] == "")
      continue;
    std::stringstream ss{vs[i]};
    double num;
    ss >> num;
    vd.push_back(num);
  }
  return vd;
}

std::vector<int> parseIntegersFromString(const std::string &s) {
  std::vector<std::string> vs{splitString(s, ' ')};
  std::vector<int> vi;
  for (int i = 0; i < vs.size(); ++i) {
    if (vs[i] == "")
      continue;
    std::stringstream ss{vs[i]};
    int num;
    ss >> num;
    vi.push_back(num);
  }
  return vi;
}

// std::string lowerString(std::string str) {
//   std::locale loc;
//   for (std::string::size_type i = 0; i < str.length(); ++i)
//     str[i] = std::tolower(str[i], loc);
//   return str;
// }
std::string lowerString(const std::string& str) {
    std::string lower_str = str;
    for (auto& ch : lower_str) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lower_str;
}

// std::string upperString(std::string str) {
//   std::locale loc;
//   for (std::string::size_type i = 0; i < str.length(); ++i)
//     str[i] = std::toupper(str[i], loc);
//   return str;
// }
std::string upperString(const std::string& str) {
    std::string upper_str = str;
    for (auto& ch : upper_str) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return upper_str;
}

std::string removeChar(std::string s, char c) {
  s.erase(std::remove(s.begin(), s.end(), c), s.end());
  return s;
}

// Function to trim leading and trailing spaces
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";  // The string is empty or only contains whitespace
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// Split a file path into {directory, base_name, extension}.
// Returns a vector of 3 strings: [path, baseName, extension].
std::vector<std::string> splitFilePath(const std::string& filepath) {
  std::vector<std::string> s;
  s.resize(3);
  if (filepath.size()) {
    int idot   = (filepath.find_last_of('.')   == std::string::npos) ? -1 : (int)filepath.find_last_of('.');
    int islash = (filepath.find_last_of("/\\") == std::string::npos) ? -1 : (int)filepath.find_last_of("/\\");
    if (idot > 0)
      s[2] = filepath.substr(idot);
    if (islash > 0)
      s[0] = filepath.substr(0, islash + 1);
    s[1] = filepath.substr(s[0].size(), filepath.size() - s[0].size() - s[2].size());
  }
  return s;
}

std::vector<double> getDxyFromAngle(double angle, char axis, double increment,
                                    bool reverse) {
  double dx = 0.0, dy = 0.0;
  double slope{tan(deg2rad(angle))};
  if (axis == 'x') {
    dx = increment;
    dy = slope * dx;
  } else if (axis == 'y') {
    dy = increment;
    dx = dy / slope;
  }

  double direction{1.0};
  if (reverse)
    direction = -1.0;

  std::vector<double> dxdy{dx * direction, dy * direction};

  return dxdy;
}

void discretizeArcN(const PGeoArc &arc, int number, Baseline *baseline, PModel *pmodel) {
  baseline->addPVertex(arc.start()); // Store the start point
  
  SVector3 v_center{arc.center()->point()};
  SVector3 v_start{arc.center()->point(), arc.start()->point()};
  STensor3 m_rotate;
  SVector3 v_mid;
  PDCELVertex *pv_new;
  
  for (int count = 1; count < number; ++count) {
    double stepAngle = count * arc.angle() / number;
    // PGeoMatrix2 r{getRotationMatrix(stepAngle, DEGREE, arc.direction())};
    m_rotate = getRotationMatrix(stepAngle, arc.direction(), DEGREE);
    // SVector3 vMid{productMV(r, vStart)};
    v_mid = m_rotate * v_start;
    // Point2 pMid{arc.getCenter() + vMid};
    pv_new = new PDCELVertex((v_center + v_mid).point());
    pmodel->addVertex(pv_new);
    // baseline.addBasepoint(pMid);
    baseline->addPVertex(pv_new);
  }
  
  baseline->addPVertex(arc.end());
}

void discretizeArcA(const PGeoArc &arc, double stepAngle, Baseline *baseline, PModel *pmodel) {
  int number = static_cast<int>(round(arc.angle() / stepAngle));
  discretizeArcN(arc, number, baseline, pmodel);
}

// Basepoint *getBasepointByName(std::string name,
//                               std::vector<Basepoint> &basepoints) {
//   for (auto it = basepoints.begin(); it != basepoints.end(); ++it) {
//     if (it->getName() == name)
//       return &(*it);
//   }
// }

Baseline *getBaselineByName(std::string name,
                            std::vector<Baseline> &baselines) {
  for (auto it = baselines.begin(); it != baselines.end(); ++it) {
    if (it->getName() == name)
      return &(*it);
  }
  return nullptr;
}

Material *getMaterialByName(std::string name,
                            std::vector<Material> &materials) {
  for (auto it = materials.begin(); it != materials.end(); ++it) {
    if (it->getName() == name)
      return &(*it);
  }
  return nullptr;
}

std::vector<double> decodeStackSequence(std::string sscode) {
  std::vector<double> vss{};

  sscode = removeChar(sscode);
  bool symmetry{false};
  int symfolds{0};

  sscode = removeChar(sscode, '[');
  std::vector<std::string> vtemp1{splitString(sscode, ']')};
  sscode = vtemp1[0];
  if (vtemp1.size() > 1) {
    std::string ssym{vtemp1[1]};
    symmetry = true;
    symfolds = 1;
    if (ssym.size() > 1) {
      ssym = removeChar(ssym, 's');
      symfolds = atoi(ssym.c_str());
    }
  }

  bool inbracket{false};
  for (std::size_t i = 0; i < sscode.size(); ++i) {
    if (sscode[i] == '(')
      inbracket = true;
    else if (sscode[i] == ')')
      inbracket = false;
    if (!inbracket && (sscode[i] == '/'))
      sscode[i] = '|';
  }
  std::vector<std::string> vsscode{splitString(sscode, '|')};
  for (auto sas : vsscode) {
    std::vector<std::string> vas{splitString(sas, ':')};
    int stack{1};
    if (vas.size() > 1)
      stack = atoi(vas[1].c_str());
    std::string sa{vas[0]};
    sa = removeChar(sa, '(');
    sa = removeChar(sa, ')');
    std::vector<std::string> va{splitString(sa, '/')};
    for (int si = 0; si < stack; ++si) {
      for (auto a : va) {
        vss.push_back(atof(a.c_str()));
      }
    }
  }

  if (symmetry) {
    for (int i = 0; i < symfolds; ++i) {
      std::vector<double> vtemp{vss};
      for (std::size_t j = vss.size(); j > 0; --j) {
        vtemp.push_back(vss[j - 1]);
      }
      vss = vtemp;
    }
  }

  return vss;
}

// template <class T>
// bool isInContainer(const std::list<T *> &container, T *element) {
//   for (auto item : container) {
//     if (item == element) {
//       return true;
//     }
//   }
//   return false;
// }

void walk(const rapidxml::xml_node<>* node, int indent) {
    const auto ind = std::string(indent * 4, ' ');
    printf("%s", ind.c_str());

    const rapidxml::node_type t = node->type();
    switch(t) {
    case rapidxml::node_element:
        {
            printf("<%.*s", (int)node->name_size(), node->name());
            for(const rapidxml::xml_attribute<>* a = node->first_attribute()
                ; a
                ; a = a->next_attribute()
            ) {
                printf(" %.*s", (int)a->name_size(), a->name());
                printf("='%.*s'", (int)a->value_size(), a->value());
            }
            printf(">\n");

            for(const rapidxml::xml_node<>* n = node->first_node()
                ; n
                ; n = n->next_sibling()
            ) {
                walk(n, indent+1);
            }
            printf("%s</%.*s>\n", ind.c_str(), (int)node->name_size(), node->name());
        }
        break;

    case rapidxml::node_data:
        printf("DATA:[%.*s]\n", (int)node->value_size(), node->value());
        break;

    default:
        printf("NODE-TYPE:%d\n", t);
        break;
    }
}
