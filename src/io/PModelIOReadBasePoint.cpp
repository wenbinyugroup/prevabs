#include "PModelIO.hpp"

#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PBaseLine.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "rapidxml/rapidxml.hpp"

#include <cmath>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void readPointsFromFile(const std::string &filenameBasepoints, PModel *pmodel,
                   double scale) {
  std::string line;
  std::ifstream fileBasepoints{filenameBasepoints};
  if (!fileBasepoints.is_open()) {
    throw std::runtime_error("unable to open basepoints file: " + filenameBasepoints);
  }
  PLOG(info) << "reading basepoints file: " << filenameBasepoints;

  while (getline(fileBasepoints, line)) {
    std::stringstream ss(line);
    std::string label;
    double x, y;
    ss >> label >> x >> y;
    if (!ss) {
      throw std::runtime_error("malformed line in basepoints file: " + line);
    }

    PDCELVertex *pv = new PDCELVertex{label, 0, x * scale, y * scale};
    pmodel->addVertex(pv);
  }
}




PDCELVertex *readXMLElementPoint(
  const xml_node<> *p_xn_point, const xml_node<> *p_xn_geo,
  PModel *pmodel
  ) {

  MESSAGE_SCOPE(g_msg);

  PDCELVertex *pv = nullptr;

  std::string label{requireAttr(p_xn_point, "name", "<point>")->value()};

  PLOG(debug) << "reading point: " + label;

  std::string s_constraint{"none"};
  xml_attribute<> *p_xa_constraint{p_xn_point->first_attribute("constraint")};
  if (p_xa_constraint) {
    s_constraint = p_xa_constraint->value();
  }

  // Point defined on a line
  if (p_xn_point->first_attribute("on")) {

    std::string bsl_name = p_xn_point->first_attribute("on")->value();
    Baseline *p_bsl;
    p_bsl = findLineByName(bsl_name, p_xn_geo, pmodel);
    if (!p_bsl) {
      throw std::runtime_error(
        "cannot find baseline '" + bsl_name
        + "' referenced by point '" + label + "'"
      );
    }
    std::string by{"curve"};
    if (p_xn_point->first_attribute("by")) {
      by = p_xn_point->first_attribute("by")->value();
    }

    double loc;
    try {
      loc = std::stod(p_xn_point->value());
    } catch (const std::exception &) {
      throw std::runtime_error(
        "point '" + label + "': invalid location value '"
        + std::string(p_xn_point->value()) + "'"
      );
    }

    std::vector<PDCELVertex *> p_bsl_vertices;
    p_bsl_vertices = p_bsl->vertices();

    pv = new PDCELVertex(label);
    pmodel->vertexData(pv).on_line = p_bsl;

    // The number is the non-dimensional curvlinear location
    if (by == "curve") {
      throw std::runtime_error(
        "point '" + label + "': by='curve' is not implemented"
      );
    }

    // The number is the x2 (y) location
    else if (by == "x2" || by == "y") {

      std::string s_which{"top"};
      xml_attribute<> *p_xa_which{p_xn_point->first_attribute("which")};
      if (p_xa_which) {
        s_which = p_xa_which->value();
      }

      bool is_new = false, is_new_tmp = false, found = false;
      int count{0};
      double z = 0.0, z_tmp = 0.0;
      int id = 0, id_tmp = 0;

      // Linear interpolation
      for (int i = 0; i < static_cast<int>(p_bsl_vertices.size()) - 1; i++) {

        if (
          (p_bsl_vertices[i]->y() <= loc && loc <= p_bsl_vertices[i+1]->y()) ||
          (p_bsl_vertices[i]->y() >= loc && loc >= p_bsl_vertices[i+1]->y())
          ) {
          found = true;
          count++;

          // The point is close to the starting point of the segment
          if (fabs(loc - p_bsl_vertices[i]->y()) < TOLERANCE) {
            PLOG(debug) << "found point close to the starting point of the segment";
            z_tmp = p_bsl_vertices[i]->z();
            id_tmp = i;
            is_new_tmp = false;
          }

          // The point is close to the ending point of the segment
          else if (fabs(loc - p_bsl_vertices[i+1]->y()) < TOLERANCE) {
            PLOG(debug) << "found point close to the ending point of the segment";
            z_tmp = p_bsl_vertices[i+1]->z();
            id_tmp = i+1;
            is_new_tmp = false;
          }

          // The point is in the middle of the segment
          else {
            PLOG(debug) << "found point in the middle of the segment";
            double dy, dz;
            dy = p_bsl_vertices[i+1]->y() - p_bsl_vertices[i]->y();
            dz = p_bsl_vertices[i+1]->z() - p_bsl_vertices[i]->z();
            z_tmp = dz / dy * (loc - p_bsl_vertices[i]->y()) + p_bsl_vertices[i]->z();
            id_tmp = i+1;
            is_new_tmp = true;
          }

          PLOG(debug) << "z_tmp = " + std::to_string(z_tmp);
          PLOG(debug) << "id_tmp = " + std::to_string(id_tmp);
          PLOG(debug) << "is_new_tmp = " + std::to_string(is_new_tmp);

          // If found more than one point, keep the one matching 'which'
          if (count > 1) {
            if ((s_which == "top" && z_tmp > z) || (s_which == "bottom" && z_tmp < z)) {
              z = z_tmp;
              id = id_tmp;
              is_new = is_new_tmp;
            }
          }
          else {
            z = z_tmp;
            id = id_tmp;
            is_new = is_new_tmp;
          }
        }
      }

      if (found) {
        if (is_new) {
          pv->setPosition(0, loc, z);
          // Insert the new point into the line
          p_bsl->insertPVertex(id, pv);
        }
        else {
          pmodel->vertexData(pv).link_to = p_bsl_vertices[id];
        }
      }
      else {
        throw std::runtime_error(
          "point '" + label + "': x2=" + std::to_string(loc)
          + " not found on baseline '" + bsl_name + "'"
        );
      }
    }

    // The number is the x3 (z) location
    else if (by == "x3" || by == "z") {
      throw std::runtime_error(
        "point '" + label + "': by='x3'/'z' is not implemented"
      );
    }

  }

  // Point defined using explicit coordinates
  else {

    std::stringstream ss{p_xn_point->value()};
    double x2 = 0.0, x3 = 0.0;
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
  const std::string &name, const xml_node<> *p_xn_geo, PModel *pmodel
  ) {
  PDCELVertex *pv = nullptr;

  pv = pmodel->getPointByName(name);

  if (!pv) {
    for (auto p_xn_bpp = p_xn_geo->first_node("point"); p_xn_bpp;
        p_xn_bpp = p_xn_bpp->next_sibling("point")) {
      if (requireAttr(p_xn_bpp, "name", "<point>")->value() == name) {
        pv = readXMLElementPoint(p_xn_bpp, p_xn_geo, pmodel);
        pmodel->addVertex(pv);
        break;
      }
    }

    // Also consider points in <basepoints> (deprecated, will be removed)
    xml_node<> *p_xn_basepoints = p_xn_geo->first_node("basepoints");
    if (p_xn_basepoints) {
      PLOG(warning) << "<basepoints> is deprecated; move points to <geo> directly";
      for (auto p_xn_bpp = p_xn_basepoints->first_node("point"); p_xn_bpp;
          p_xn_bpp = p_xn_bpp->next_sibling("point")) {
        if (requireAttr(p_xn_bpp, "name", "<point>")->value() == name) {
          pv = readXMLElementPoint(p_xn_bpp, p_xn_geo, pmodel);
          pmodel->addVertex(pv);
          break;
        }
      }
    }
  }

  return pv;
}
