#include "PModelIO.hpp"

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
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

#ifdef __linux__
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#elif _WIN32
#include <tchar.h>
#include <windows.h>
#endif

int readBaselines(const xml_node<> *nodeBaselines, PModel *pmodel,
                  const std::string &filePath, double /*dx*/, double /*dy*/, double /*dz*/, double /*s*/, double /*r*/) {
  xml_node<> *p_xn_basepoints{nodeBaselines->first_node("basepoints")};
  if (p_xn_basepoints){
    // Read basepoints from file
    for (auto p_xn_bpi = p_xn_basepoints->first_node("include"); p_xn_bpi;
        p_xn_bpi = p_xn_bpi->next_sibling("include")) {
      double pscale = 1.0;
      std::string s_pscale;
      xml_attribute<> *p_xa_scaling{p_xn_bpi->first_attribute("scale")};
      if (p_xa_scaling) {
        s_pscale = p_xa_scaling->value();
        pscale = parseRequiredDouble(s_pscale, "<include scale>");
      }

      std::string filenameBasepoints{p_xn_bpi->value()};
      filenameBasepoints = filePath + filenameBasepoints + ".dat";
      readPointsFromFile(filenameBasepoints, pmodel, pscale);
    }
      // Also consider points in <basepoints>
      // Will be deprecated in the future
    for (auto p_xn_bpp = p_xn_basepoints->first_node("point"); p_xn_bpp;
        p_xn_bpp = p_xn_bpp->next_sibling("point")) {
      PDCELVertex *p_vertex;
      p_vertex = pmodel->getPointByName(
        requireAttr(p_xn_bpp, "name", "<point>")->value()
      );
      if (!p_vertex) {
        p_vertex = readXMLElementPoint(p_xn_bpp, nodeBaselines, pmodel);
        pmodel->addVertex(p_vertex);
      }
    }
  }

  // Read points and lines sequentially
  for (auto p_xn_prim = nodeBaselines->first_node(); p_xn_prim;
       p_xn_prim = p_xn_prim->next_sibling()) {

    std::string _name{p_xn_prim->name()};
    if (_name == "point") {
      PDCELVertex *p_vertex;
      p_vertex = readXMLElementPoint(p_xn_prim, nodeBaselines, pmodel);
      pmodel->addVertex(p_vertex);
    }

    else if (_name == "line" || _name == "baseline") {
      Baseline *p_line;
      p_line = readXMLElementLine(p_xn_prim, nodeBaselines, pmodel);
      if (!p_line) {
        xml_attribute<> *p_xa_name = p_xn_prim->first_attribute("name");
        std::string node_name = p_xa_name ? p_xa_name->value() : "<unnamed>";
        PLOG(error) << "readBaselines: failed to read baseline '" << node_name
                    << "', skipping registration";
        continue;
      }
      pmodel->addBaseline(p_line);
      if (config.debug) {
        p_line->print();
      }
    }

  }

  return 0;
}

Baseline *readXMLElementLine(const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel) {

  std::string baselineName{
    requireAttr(p_xn_line, "name", "<line>/<baseline>")->value()
  };

    PLOG(debug) << "reading line: " + baselineName;

  Baseline *line = new Baseline();

  std::string method{"direct"};
  xml_attribute<> *p_xa_method{p_xn_line->first_attribute("method")};
  if (p_xa_method) {
    method = p_xa_method->value();
  }

  if (method == "direct") {

    line->setName(baselineName);

    // std::string baselineType{p_xn_line->first_attribute("type")->value()};
    std::string baselineType{"straight"};
    xml_attribute<> *p_xa_type{p_xn_line->first_attribute("type")};
    if (p_xa_type) {
      baselineType = p_xa_type->value();
    }

    line->setType(baselineType);

    // TYPE 1: Straight line / polyline
    if (baselineType == "straight") {
      readLineTypeStraight(line, p_xn_line, p_xn_geo, pmodel);
    }
    // TYPE 2: A complete circle
    else if (baselineType == "circle") {
      readLineTypeCircle(line, p_xn_line, p_xn_geo, pmodel);
    }
    // TYPE 3: Arc (part of a circle)
    else if (baselineType == "arc") { // Discretize arc
      readLineTypeArc(line, p_xn_line, p_xn_geo, pmodel);
    }
    // TYPE 4: Airfoil
    else if (baselineType == "airfoil") { // Discretize arc
      readLineTypeAirfoil(line, p_xn_line, p_xn_geo, pmodel, 1e-12);
    }

    else {
      throw std::runtime_error(
        "baseline '" + baselineName + "': unknown type '" + baselineType + "'"
      );
    }

  }

  else if (method == "join") {

    line->setName(baselineName);
    int join_status = readLineByJoin(line, p_xn_line, p_xn_geo, pmodel);
    if (join_status != 0) {
      PLOG(error) << "failed to join baseline '" << baselineName << "'";
      delete line;
      return nullptr;
    }
    // line->print(pmessage, 1);

  }

  // pmodel->addBaseline(baseline);

  return line;
}

void readLineTypeStraight(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel) {
  xml_node<> *nps{p_xn_line->first_node("points")};
  xml_node<> *np{p_xn_line->first_node("point")};
  xml_node<> *na{p_xn_line->first_node("angle")};

  // CASE 1: Given a series of points
  if (nps) {
    std::string basepointLabels{nps->value()};

    // First split the string by commas ','
    std::vector<std::string> vLabels;
    vLabels = splitString(basepointLabels, ',');

    for (auto s : vLabels) {
      // Then for each substring, split it by colon ':'
      std::vector<std::string> vBeginEnd;
      vBeginEnd = splitString(s, ':');

      if (vBeginEnd.size() > 2) {
        throw std::runtime_error(
          "baseline '" + line->getName()
          + "': malformed point expression '" + s
          + "' (too many ':' separators)"
        );
      }

      // A single point
      if (vBeginEnd.size() == 1) {
        std::string basepointLabel{vBeginEnd[0]};

        PDCELVertex *pv;
        pv = findPointByName(basepointLabel, p_xn_geo, pmodel);
        if (!pv) {
          throw std::runtime_error(
            "cannot find point '" + basepointLabel
            + "' in baseline '" + line->getName() + "'"
          );
        }

        if (pmodel->vertexData(pv).link_to) {
          pv = pmodel->vertexData(pv).link_to;
        }

        line->addPVertex(pv);
      }

      // A series of points with begin and end
      else if (vBeginEnd.size() == 2) {
        std::string basepointLabelBegin{vBeginEnd[0]};
        std::string basepointLabelEnd{vBeginEnd[1]};

        PDCELVertex *pvbegin, *pvend;
        pvbegin = findPointByName(basepointLabelBegin, p_xn_geo, pmodel);
        if (!pvbegin) {
          throw std::runtime_error(
            "cannot find point '" + basepointLabelBegin
            + "' in baseline '" + line->getName() + "'"
          );
        }
        pvend = findPointByName(basepointLabelEnd, p_xn_geo, pmodel);
        if (!pvend) {
          throw std::runtime_error(
            "cannot find point '" + basepointLabelEnd
            + "' in baseline '" + line->getName() + "'"
          );
        }
        bool addPoint{false};

        // Points belong to another line
        // The new line is a segment of the existing line
        if (pmodel->vertexData(pvbegin).on_line || pmodel->vertexData(pvend).on_line) {
          Baseline *p_on_bsl = pmodel->vertexData(pvbegin).on_line;
          if (!p_on_bsl) p_on_bsl = pmodel->vertexData(pvend).on_line;

          if (pmodel->vertexData(pvbegin).link_to)
            basepointLabelBegin = pmodel->vertexData(pvbegin).link_to->name();
          if (pmodel->vertexData(pvend).link_to)
            basepointLabelEnd = pmodel->vertexData(pvend).link_to->name();

          int _id{0};
          const int _n = static_cast<int>(p_on_bsl->vertices().size());
          bool _found_end{false};
          for (int _iter = 0; _iter < 2 * _n; ++_iter) {
            if (_id >= _n) { break; }  // open baseline walked off the end
            PDCELVertex *_pv_tmp = p_on_bsl->vertices()[_id];
            if (_pv_tmp->name() == basepointLabelBegin) {
              addPoint = true;
            }
            if (addPoint) {
              line->addPVertex(_pv_tmp);
            }
            if (addPoint && _pv_tmp->name() == basepointLabelEnd) {
              _found_end = true;
              break;
            }
            if (p_on_bsl->isClosed() && _id == _n - 2) {
              _id = 0;
            } else {
              _id++;
            }
          }
          if (!_found_end) {
            throw std::runtime_error(
              "readLineTypeStraight: endpoint '" + basepointLabelEnd
              + "' not found on baseline '" + p_on_bsl->getName()
              + "' while building baseline '" + line->getName() + "'"
            );
          }
        }

        // Completely new line
        else {
          for (auto it = pmodel->vertices().begin();
                it != pmodel->vertices().end(); ++it) {
            if ((*it)->name() == basepointLabelBegin)
              addPoint = true;
            if (addPoint)
              line->addPVertex(*it);
            if ((*it)->name() == basepointLabelEnd)
              break;
          }
        }

      }
    }
  }

  // CASE 2: Given a point and an angle
  else if (np && na) {
    std::string basepointLabel{np->value()};
    std::string loc{"inner"};
    if (np->first_attribute("loc")) {
      loc = np->first_attribute("loc")->value();
    }

    double angle{parseRequiredDouble(na->value(),
      "<angle> in straight baseline '" + line->getName() + "'")};

    PDCELVertex *pvMid, *pvStart, *pvEnd;
    // pvMid = pmodel->getPointByName(basepointLabel);
    pvMid = findPointByName(basepointLabel, p_xn_geo, pmodel);
    if (!pvMid) {
      throw std::runtime_error(
        "cannot find point '" + basepointLabel
        + "' in baseline '" + line->getName() + "'"
      );
    }

    SVector3 dir = getVectorFromAngle(angle, AnglePlane::YZ);
    pvStart = new PDCELVertex(pvMid->point() - dir.point());
    pvEnd = new PDCELVertex(pvMid->point() + dir.point());

    pmodel->addVertex(pvStart);
    pmodel->addVertex(pvEnd);

    if (loc == "inner") {
      line->addPVertex(pvStart);
    }
    line->addPVertex(pvMid);
    line->addPVertex(pvEnd);

    line->setRefVertex(pvMid);
  }

  else {
    throw std::runtime_error(
      "baseline '" + line->getName()
      + "' (straight): missing definition — use <points> or <point>+<angle>"
    );
  }
}

void readLineTypeCircle(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel) {
  xml_node<> *nc{p_xn_line->first_node("center")};
  xml_node<> *nr{p_xn_line->first_node("radius")};
  xml_node<> *np{p_xn_line->first_node("point")};
  xml_node<> *nt{p_xn_line->first_node("discrete")};
  xml_node<> *nd{p_xn_line->first_node("direction")};

  std::string discreteBy{"angle"};
  int discreteNumber{120};
  double discreteAngle{3.0};

  if (nt) {
    std::string v{nt->value()};
    xml_attribute<> *ab{nt->first_attribute("by")};
    if (ab)
      discreteBy = ab->value();
    if (discreteBy == "number") {
      discreteNumber = parseRequiredInt(v,
        "<discrete by='number'> in circle baseline '" + line->getName() + "'");
      if (discreteNumber <= 0) {
        throw std::runtime_error(
          "<discrete by='number'> must be positive in circle baseline '"
          + line->getName() + "', got " + v
        );
      }
    }
    else if (discreteBy == "angle") {
      discreteAngle = parseRequiredDouble(v,
        "<discrete by='angle'> in circle baseline '" + line->getName() + "'");
      if (discreteAngle <= 0.0) {
        throw std::runtime_error(
          "<discrete by='angle'> must be positive in circle baseline '"
          + line->getName() + "', got " + v
        );
      }
    }
  }

  PDCELVertex *pCenter = nullptr;
  PDCELVertex *pStart = nullptr;

  // CASE 1: Given center and radius
  if (nc && nr) {
    std::string centerLabel{nc->value()};
    double radius{parseRequiredDouble(nr->value(),
      "<radius> in circle baseline '" + line->getName() + "'")};

    // pCenter = pmodel->getPointByName(centerLabel);
    pCenter = findPointByName(centerLabel, p_xn_geo, pmodel);
    if (!pCenter) {
      throw std::runtime_error(
        "cannot find center point '" + centerLabel
        + "' in baseline '" + line->getName() + "'"
      );
    }

    SVector3 vStart{0.0, radius, 0.0};
    pStart = new PDCELVertex(pCenter->point() + vStart.point());
    pmodel->addVertex(pStart);
  }
  
  
  // CASE 2: Given center and a point
  else if (nc && np) {
    std::string centerName{nc->value()};
    // pCenter = pmodel->getPointByName(centerName);
    pCenter = findPointByName(centerName, p_xn_geo, pmodel);
    if (!pCenter) {
      throw std::runtime_error(
        "cannot find center point '" + centerName
        + "' in baseline '" + line->getName() + "'"
      );
    }

    std::string startName{np->value()};
    // pStart = pmodel->getPointByName(startName);
    pStart = findPointByName(startName, p_xn_geo, pmodel);
    if (!pStart) {
      throw std::runtime_error(
        "cannot find start point '" + startName
        + "' in baseline '" + line->getName() + "'"
      );
    }
  }

  else {
    throw std::runtime_error(
      "baseline '" + line->getName()
      + "' (circle): missing definition — use <center>+<radius> or <center>+<point>"
    );
  }

  int direction = 1;
  std::string s_drt;
  if (nd) {
    s_drt = nd->value();
    if (s_drt == "cw") {
      direction = -1;
    }
  }
  PGeoArc arc{pCenter, pStart, direction};
  if (discreteBy == "number") {
    discretizeArcN(arc, discreteNumber, line, pmodel);
  } else if (discreteBy == "angle") {
    discretizeArcA(arc, discreteAngle, line, pmodel);
  }

  return;
}

void readLineTypeArc(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel) {
  xml_node<> *nc{p_xn_line->first_node("center")};
  xml_node<> *ns{p_xn_line->first_node("start")};
  xml_node<> *ne{p_xn_line->first_node("end")};
  xml_node<> *na{p_xn_line->first_node("angle")};
  xml_node<> *nt{p_xn_line->first_node("discrete")};
  xml_node<> *p_xn_radius{p_xn_line->first_node("radius")};
  xml_node<> *p_xn_curv{p_xn_line->first_node("curvature")};
  xml_node<> *p_xn_side{p_xn_line->first_node("side")};

  xml_node<> *nd{p_xn_line->first_node("direction")};

  std::string discreteBy{"angle"};
  int discreteNumber{120};
  double discreteAngle{3.0};

  if (nt) {
    std::string v{nt->value()};
    xml_attribute<> *ab{nt->first_attribute("by")};
    if (ab)
      discreteBy = ab->value();
    if (discreteBy == "number") {
      discreteNumber = parseRequiredInt(v,
        "<discrete by='number'> in arc baseline '" + line->getName() + "'");
      if (discreteNumber <= 0) {
        throw std::runtime_error(
          "<discrete by='number'> must be positive in arc baseline '"
          + line->getName() + "', got " + v
        );
      }
    }
    else if (discreteBy == "angle") {
      discreteAngle = parseRequiredDouble(v,
        "<discrete by='angle'> in arc baseline '" + line->getName() + "'");
      if (discreteAngle <= 0.0) {
        throw std::runtime_error(
          "<discrete by='angle'> must be positive in arc baseline '"
          + line->getName() + "', got " + v
        );
      }
    }
  }

  int direction = 1;
  std::string s_drt;
  if (nd) {
    s_drt = nd->value();
    if (s_drt == "cw") {
      direction = -1;
    }
  }

  PGeoArc arc;

  PDCELVertex *pv_center, *pv_start, *pv_end;

  // Case with start, end, radius
  if (ns && ne && (p_xn_radius || p_xn_curv)) {
    // TODO: sub-case for half circle

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel);
    if (!pv_start) {
      throw std::runtime_error(
        "cannot find start point '" + std::string(ns->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel);
    if (!pv_end) {
      throw std::runtime_error(
        "cannot find end point '" + std::string(ne->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    double r = 0.0, k = 0.0;

    if (p_xn_radius) {
      std::string r_str{p_xn_radius->value()};
      r = parseRequiredDouble(r_str,
        "<radius> in arc baseline '" + line->getName() + "'");
      k = 1.0 / r;
    }
    else if (p_xn_curv) {
      std::string k_str{p_xn_curv->value()};
      k = parseRequiredDouble(k_str,
        "<curvature> in arc baseline '" + line->getName() + "'");
      if (k != 0) {
        r = 1.0 / fabs(k);
      }
    }

    if (k == 0.0) {
      // 0 curvature, straight line
      line->addPVertex(pv_start);
      SPoint3 sp_mid = (pv_start->point() + pv_end->point()) * 0.5;
      PDCELVertex *pv_mid = new PDCELVertex(sp_mid);
      pmodel->addVertex(pv_mid);
      line->addPVertex(pv_mid);
      line->addPVertex(pv_end);
      return;
    }

    else {
      // with respect to the vector ns -> ne
      double side = 1;  // arc bend to the right, center on the left
      if (p_xn_side) {
        std::string s_side;
        s_side = p_xn_side->value();
        if (s_side == "left") {
          side = -1;
        }
      }
      if (k > 0) {
        side = 1;
      }
      else if (k < 0) {
        side = -1;
        k = fabs(k);
        direction = -direction;
      }

      SVector3 sv_n{side, 0, 0};

      // Calculate the center
      SVector3 sv_se{pv_start->point(), pv_end->point()};
      SVector3 sv_sm = sv_se * 0.5;

      double s = sv_se.norm();
      double p = sqrt(r*r - (s/2)*(s/2));

      SVector3 sv_mc = crossprod(sv_n, sv_se);
      sv_mc.normalize();
      sv_mc = sv_mc * p;
      SPoint3 sp_c = pv_start->point() + (sv_sm + sv_mc).point();
      pv_center = new PDCELVertex(sp_c);

      arc = PGeoArc{pv_center, pv_start, pv_end, direction};
    }
  }

  // CASE 1: Given center, start, end and angle
  else if (nc && ns && ne && na) {
    // std::string centerName{nc->value()};
    // pv_center = pmodel->getPointByName(centerName);
    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel);
    if (!pv_center) {
      throw std::runtime_error(
        "cannot find center point '" + std::string(nc->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    // std::string startName{ns->value()};
    // pv_start = pmodel->getPointByName(startName);
    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel);
    if (!pv_start) {
      throw std::runtime_error(
        "cannot find start point '" + std::string(ns->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    // std::string endName{ne->value()};
    // pv_end = pmodel->getPointByName(endName);
    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel);
    if (!pv_end) {
      throw std::runtime_error(
        "cannot find end point '" + std::string(ne->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    double angle{parseRequiredDouble(na->value(),
      "<angle> in arc baseline '" + line->getName() + "'")};
    arc = PGeoArc{pv_center, pv_start, pv_end, angle, direction};
  }

  // CASE 2: Given center, start and end
  else if (nc && ns && ne) {
    // std::string centerName{nc->value()};
    // std::string startName{ns->value()};
    // std::string endName{ne->value()};
    // pv_center = pmodel->getPointByName(centerName);
    // pv_start = pmodel->getPointByName(startName);
    // pv_end = pmodel->getPointByName(endName);

    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel);
    if (!pv_center) {
      throw std::runtime_error(
        "cannot find center point '" + std::string(nc->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel);
    if (!pv_start) {
      throw std::runtime_error(
        "cannot find start point '" + std::string(ns->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel);
    if (!pv_end) {
      throw std::runtime_error(
        "cannot find end point '" + std::string(ne->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    arc = PGeoArc{pv_center, pv_start, pv_end, direction};
  }

  // CASE 3: Given center, start and angle
  else if (nc && ns && na) {
    // std::string centerName{nc->value()};
    // std::string startName{ns->value()};
    // pv_center = pmodel->getPointByName(centerName);
    // pv_start = pmodel->getPointByName(startName);
    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel);
    if (!pv_center) {
      throw std::runtime_error(
        "cannot find center point '" + std::string(nc->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel);
    if (!pv_start) {
      throw std::runtime_error(
        "cannot find start point '" + std::string(ns->value())
        + "' in baseline '" + line->getName() + "'"
      );
    }

    double angle{parseRequiredDouble(na->value(),
      "<angle> in arc baseline '" + line->getName() + "'")};
    arc = PGeoArc{pv_center, pv_start, angle, direction};

    pmodel->addVertex(arc.end());
  }

  else {
    throw std::runtime_error(
      "baseline '" + line->getName()
      + "' (arc): unrecognized configuration"
      " — provide <center>+<start>+<end>[+<angle>], or <start>+<end>+<radius>"
    );
  }

  if (discreteBy == "number")
    discretizeArcN(arc, discreteNumber, line, pmodel);
  else if (discreteBy == "angle")
    discretizeArcA(arc, discreteAngle, line, pmodel);

  return;
}

int readLineByJoin(
  Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo,
  PModel *pmodel) {

  std::list<Baseline *> sublines;
  for (auto p_xn_subline = p_xn_line->first_node(); p_xn_subline;
       p_xn_subline = p_xn_subline->next_sibling()) {

    std::string subline_name = p_xn_subline->value();
    Baseline *subline = findLineByName(subline_name, p_xn_geo, pmodel);
    if (!subline) {
      PLOG(error) << "readLineByJoin: subline '" << subline_name
                  << "' not found for baseline '" << line->getName() << "'";
      return -1;
    }
    if (config.debug) {
      subline->print();
    }
    sublines.push_back(subline);

  }

  int join_status = joinCurves(line, sublines);
  if (join_status != 0) {
    PLOG(error) << "readLineByJoin: failed to join sublines for baseline '"
                << line->getName() << "'";
    return -1;
  }
  // line->print(pmessage, 1);

  return 0;
}

Baseline *findLineByName(
  const std::string &name, const xml_node<> *p_xn_geo, PModel *pmodel
  ) {
  Baseline *p_line = nullptr;

  p_line = pmodel->getBaselineByName(name);

  if (!p_line) {
    for (auto p_xn_bsl = p_xn_geo->first_node("line");
       p_xn_bsl; p_xn_bsl = p_xn_bsl->next_sibling("line")) {
      if (requireAttr(p_xn_bsl, "name", "<line>/<baseline>")->value() == name) {
        p_line = readXMLElementLine(p_xn_bsl, p_xn_geo, pmodel);
        if (p_line) { pmodel->addBaseline(p_line); }
        break;
      }
    }
  }

  if (!p_line) {
    for (auto p_xn_bsl = p_xn_geo->first_node("baseline");
       p_xn_bsl; p_xn_bsl = p_xn_bsl->next_sibling("baseline")) {
      if (requireAttr(p_xn_bsl, "name", "<line>/<baseline>")->value() == name) {
        p_line = readXMLElementLine(p_xn_bsl, p_xn_geo, pmodel);
        if (p_line) { pmodel->addBaseline(p_line); }
        break;
      }
    }
  }

  return p_line;

}
