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

#include "gmsh/GModel.h"
#include "gmsh/MTriangle.h"
#include "gmsh/MVertex.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"
#include "gmsh_mod/StringUtils.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cmath>
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

int readPointsFromFile(const std::string &filenameBasepoints, PModel *pmodel,
                   double scale, Message *pmessage) {
  std::string line;
  std::ifstream fileBasepoints{filenameBasepoints};
  if (!fileBasepoints.is_open()) {
    std::cout << "Unable to open file: " << filenameBasepoints << std::endl;
    return 1;
  } else {
    printInfo(i_indent, "reading basepoints file: " + filenameBasepoints);
  }

  if (fileBasepoints.is_open()) {
    while (getline(fileBasepoints, line)) {
      std::stringstream ss(line);
      std::string label;
      double x, y;
      ss >> label >> x >> y;

      // std::cout << "[debug] reading point: " << label << " (" << x << ", " <<
      // y << ")" << std::endl;
      PDCELVertex *pv = new PDCELVertex{label, 0, x * scale, y * scale};
      // pmodel->_vertices.push_back(pv);
      pmodel->addVertex(pv);
    }
    fileBasepoints.close();
  } else {
    std::cout << "Unable to open file." << std::endl;
  }

  return 0;
}






PDCELVertex *readXMLElementPoint(const xml_node<> *p_xn_point, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage) {
  double tol = 1e-6;
  
  PDCELVertex *pv;

  std::string label{p_xn_point->first_attribute("name")->value()};

  std::string s_constraint{"none"};
  xml_attribute<> *p_xa_constraint{p_xn_point->first_attribute("constraint")};
  if (p_xa_constraint) {
    s_constraint = p_xa_constraint->value();
  }




  // Point defined on a line
  if (p_xn_point->first_attribute("on")) {

    std::string bsl_name = p_xn_point->first_attribute("on")->value();
    Baseline *p_bsl;
    p_bsl = findLineByName(bsl_name, p_xn_geo, pmodel, pmessage);
    if (!p_bsl) {
      // TODO: raise error 'cannot find line' and exit.
      std::cout << "cannot find base line: " << bsl_name << std::endl;
    }
    // std::cout << "find base line: " << bsl_name << std::endl;

    std::string by{"curve"};
    if (p_xn_point->first_attribute("by")) {
      by = p_xn_point->first_attribute("by")->value();
    }

    double loc = atof(p_xn_point->value());

    std::vector<PDCELVertex *> p_bsl_vertices;
    p_bsl_vertices = p_bsl->vertices();

    pv = new PDCELVertex(label);
    pv->setOnLine(p_bsl);


    // The number is the non-dimensional curvlinear location
    if (by == "curve") {
      // TODO:
    }


    // The number is the x2 (y) location
    else if (by == "x2" || by == "y") {

      std::string s_which{"top"};
      xml_attribute<> *p_xa_which{p_xn_point->first_attribute("which")};
      if (p_xa_which) {
        s_which = p_xa_which->value();
      }

      // PDCELVertex *_pv_tmp;
      bool _is_new, _is_new_tmp, found{false};
      int count{0};
      double _z, _z_tmp;
      int _id, _id_tmp;

      // Linear interpolation
      for (auto i = 0; i < p_bsl_vertices.size() - 1; i++) {
        if (
          (p_bsl_vertices[i]->y() <= loc && loc <= p_bsl_vertices[i+1]->y()) ||
          (p_bsl_vertices[i]->y() >= loc && loc >= p_bsl_vertices[i+1]->y())
        ) {
          found = true;
          count++;
          // Find the new point
          if (fabs(loc - p_bsl_vertices[i]->y()) < tol) {
            // _pv_tmp->setLinkToVertex(p_bsl_vertices[i]);
            // break;
            _z_tmp = p_bsl_vertices[i]->z();
            _id_tmp = i;
            _is_new_tmp = false;
          }
          else if (fabs(loc - p_bsl_vertices[i+1]->y()) < tol) {
            // _pv_tmp->setLinkToVertex(p_bsl_vertices[i+1]);
            // break;
            _z_tmp = p_bsl_vertices[i+1]->z();
            _id_tmp = i+1;
            _is_new_tmp = false;
          }
          else {
            double dy, dz, loc2;
            dy = p_bsl_vertices[i+1]->y() - p_bsl_vertices[i]->y();
            dz = p_bsl_vertices[i+1]->z() - p_bsl_vertices[i]->z();
            _z_tmp = dz / dy * (loc - p_bsl_vertices[i]->y()) + p_bsl_vertices[i]->z();
            // _pv_tmp->setPosition(0, loc, loc2);
            _id_tmp = i+1;
            _is_new_tmp = true;

            // Insert the new point into the line
            // p_bsl->insertPVertex(i+1, pv);
            // p_bsl->print(pmessage, 9);

            // break;
          }

          if (count > 1) {
            // Compare z
            if (s_which == "top" && _z_tmp > _z || s_which == "bottom" && _z_tmp < _z) {
              _z = _z_tmp;
              _id = _id_tmp;
              _is_new = _is_new_tmp;
            }
          }
          else {
            _z = _z_tmp;
            _id = _id_tmp;
            _is_new = _is_new_tmp;
          }
        }
      }

      if (found) {
        if (_is_new) {
          pv->setPosition(0, loc, _z);
          // Insert the new point into the line
          p_bsl->insertPVertex(_id, pv);
        }
        else {
          pv->setLinkToVertex(p_bsl_vertices[_id]);
        }
      }
    }


    // The number is the x3 (z) location
    else if (by == "x3" || by == "z") {
      // TODO:
    }

    // p_bsl->print(pmessage, 9);
    // std::cout << pv << std::endl;

  }




  // Point defined using explicit coordinates
  else {

    std::stringstream ss{p_xn_point->value()};
    double x2, x3;
    if (s_constraint == "none") {
      ss >> x2 >> x3;
    } else if (s_constraint == "middle") {
      std::string pn1, pn2;
      ss >> pn1 >> pn2;
      PDCELVertex *pv1, *pv2;
      pv1 = pmodel->getPointByName(pn1);
      pv2 = pmodel->getPointByName(pn2);
      x2 = (pv1->y() + pv2->y()) / 2;
      x3 = (pv1->z() + pv2->z()) / 2;
    }
    pv = new PDCELVertex{label, 0, x2, x3};

  }

  return pv;
}









PDCELVertex *findPointByName(
  const std::string &name, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage
  ) {
  PDCELVertex *pv = nullptr;

  pv = pmodel->getPointByName(name);

  if (!pv) {
    for (auto p_xn_bpp = p_xn_geo->first_node("point"); p_xn_bpp;
        p_xn_bpp = p_xn_bpp->next_sibling("point")) {
      if (p_xn_bpp->first_attribute("name")->value() == name) {
        pv = readXMLElementPoint(p_xn_bpp, p_xn_geo, pmodel, pmessage);
        pmodel->addVertex(pv);
        break;
      }
    }

    // Also consider points in <basepoints>
    // Will be deprecated in the future
    for (auto p_xn_bpp = p_xn_geo->first_node("basepoints")->first_node("point"); p_xn_bpp;
        p_xn_bpp = p_xn_bpp->next_sibling("point")) {
      if (p_xn_bpp->first_attribute("name")->value() == name) {
        pv = readXMLElementPoint(p_xn_bpp, p_xn_geo, pmodel, pmessage);
        pmodel->addVertex(pv);
        break;
      }
    }
  }

  return pv;
}
