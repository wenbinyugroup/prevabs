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

#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"
#include "gmsh_mod/StringUtils.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
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
                  const std::string &filePath, double dx, double dy, double dz, double s, double r, Message *pmessage) {

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
        pscale = atof(s_pscale.c_str());
      }

      // if (debug)
      //   std::cout << "Extract Basepoints file name" << std::endl;
      std::string filenameBasepoints{p_xn_bpi->value()};
      filenameBasepoints = filePath + filenameBasepoints + ".dat";
      // if (debug)
      // std::cout << "- reading base points" << std::endl;
      readPointsFromFile(filenameBasepoints, pmodel, pscale, pmessage);
      // if (config.debug) {
      //   // std::cout << "- reading base points -- done" << std::endl;
      //   pmessage->print(9, "base points");
      //   for (auto bp : pmodel->vertices()) {
      //     // std::cout << bp << std::endl;
      //     std::stringstream ss;
      //     ss << bp;
      //     pmessage->print(9, ss);
      //   }
      // }
    }
      // Also consider points in <basepoints>
      // Will be deprecated in the future
    for (auto p_xn_bpp = p_xn_basepoints->first_node("point"); p_xn_bpp;
        p_xn_bpp = p_xn_bpp->next_sibling("point")) {
      PDCELVertex *p_vertex;
      p_vertex = pmodel->getPointByName(p_xn_bpp->first_attribute("name")->value());
      if (!p_vertex) {
        p_vertex = readXMLElementPoint(p_xn_bpp, nodeBaselines, pmodel, pmessage);
        pmodel->addVertex(p_vertex);
      }
    }
  }

  // Read points and lines sequentially
  for (auto p_xn_prim = nodeBaselines->first_node(); p_xn_prim;
       p_xn_prim = p_xn_prim->next_sibling()) {

    std::string _name{p_xn_prim->name()};
    // std::cout << p_xn_prim->name() << std::endl;

    if (_name == "point") {
      PDCELVertex *p_vertex;
      p_vertex = readXMLElementPoint(p_xn_prim, nodeBaselines, pmodel, pmessage);
      pmodel->addVertex(p_vertex);
      // p_vertex = pmodel->getPointByName(p_xn_prim->first_attribute("name")->value());
      // if (!p_vertex) {
      //   p_vertex = readXMLElementPoint(p_xn_prim, nodeBaselines, pmodel, pmessage);
      //   pmodel->addVertex(p_vertex);
      // }
    }

    else if (_name == "line" || _name == "baseline") {
      Baseline *p_line;
      p_line = readXMLElementLine(p_xn_prim, nodeBaselines, pmodel, pmessage);
      pmodel->addBaseline(p_line);
      // p_line = pmodel->getBaselineByName(p_xn_prim->first_attribute("name")->value());
      // if (!p_line) {
      //   p_line = readXMLElementLine(p_xn_prim, nodeBaselines, pmodel, pmessage);
      //   pmodel->addBaseline(p_line);
      // }
      p_line->print(pmessage);
    }

  }

  // Read basepoints from list
  // for (auto p_xn_bpp = nodeBaselines->first_node("point"); p_xn_bpp;
  //      p_xn_bpp = p_xn_bpp->next_sibling("point")) {
  //   PDCELVertex *p_vertex;
  //   p_vertex = pmodel->getPointByName(p_xn_bpp->first_attribute("name")->value());
  //   if (!p_vertex) {
  //     p_vertex = readXMLElementPoint(p_xn_bpp, nodeBaselines, pmodel, pmessage);
  //     pmodel->addVertex(p_vertex);
  //   }
  // }

  // if (config.debug) {
  //   pmessage->printBlank();
  //   pmessage->print(9, "summary of base points");
  //   for (auto bp : pmodel->vertices()) {
  //     std::stringstream ss;
  //     ss << bp;
  //     pmessage->print(9, ss);
  //   }
  // }


  // for (xml_node<> *nodeBaseline = nodeBaselines->first_node("line");
  //      nodeBaseline; nodeBaseline = nodeBaseline->next_sibling("line")) {
  //   Baseline *p_line;
  //   p_line = pmodel->getBaselineByName(nodeBaseline->first_attribute("name")->value());
  //   if (!p_line) {
  //     p_line = readXMLElementLine(nodeBaseline, nodeBaselines, pmodel, pmessage);
  //     pmodel->addBaseline(p_line);
  //   }
  // }




  // for (xml_node<> *nodeBaseline = nodeBaselines->first_node("baseline");
  //      nodeBaseline; nodeBaseline = nodeBaseline->next_sibling("baseline")) {
  //   Baseline *p_line;
  //   p_line = pmodel->getBaselineByName(nodeBaseline->first_attribute("name")->value());
  //   if (!p_line) {
  //     p_line = readXMLElementLine(nodeBaseline, nodeBaselines, pmodel, pmessage);
  //     pmodel->addBaseline(p_line);
  //   }
  // }


  return 0;
}









Baseline *readXMLElementLine(const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();
  
  std::string baselineName{p_xn_line->first_attribute("name")->value()};

  PLOG(debug) << pmessage->message("reading line: " + baselineName);

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
      readLineTypeStraight(line, p_xn_line, p_xn_geo, pmodel, pmessage);
    }
    // TYPE 2: A complete circle
    else if (baselineType == "circle") {
      readLineTypeCircle(line, p_xn_line, p_xn_geo, pmodel, pmessage);
    }
    // TYPE 3: Arc (part of a circle)
    else if (baselineType == "arc") { // Discretize arc
      readLineTypeArc(line, p_xn_line, p_xn_geo, pmodel, pmessage);
    }
    // TYPE 4: Airfoil
    else if (baselineType == "airfoil") { // Discretize arc
      readLineTypeAirfoil(line, p_xn_line, p_xn_geo, pmodel, pmessage, 1e-12);
    }

  }

  else if (method == "join") {

    line->setName(baselineName);
    readLineByJoin(line, p_xn_line, p_xn_geo, pmodel, pmessage);
    // line->print(pmessage, 1);

  }

  // pmodel->addBaseline(baseline);

  pmessage->decreaseIndent();

  return line;
}









void readLineTypeStraight(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage) {
  //   if (debug)
  //     std::cout << "Straight Baseline" << std::endl;
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


      // A single point
      if (vBeginEnd.size() == 1) {
        std::string basepointLabel{vBeginEnd[0]};

        PDCELVertex *pv;
        pv = findPointByName(basepointLabel, p_xn_geo, pmodel, pmessage);
        if (!pv) {
          // TODO: raise error 'cannot find point' and exit.
        }

        if (pv->getLinkToVertex()) {
          pv = pv->getLinkToVertex();
        }

        line->addPVertex(pv);
      }


      // A series of points with begin and end
      else if (vBeginEnd.size() == 2) {
        std::string basepointLabelBegin{vBeginEnd[0]};
        std::string basepointLabelEnd{vBeginEnd[1]};

        PDCELVertex *pvbegin, *pvend;
        pvbegin = findPointByName(basepointLabelBegin, p_xn_geo, pmodel, pmessage);
        if (!pvbegin) {
          // TODO: raise error 'cannot find point' and exit.
        }
        // std::cout << "pvbegin: " << pvbegin << std::endl;
        pvend = findPointByName(basepointLabelEnd, p_xn_geo, pmodel, pmessage);
        if (!pvend) {
          // TODO: raise error 'cannot find point' and exit.
        }
        // std::cout << "pvend: " << pvend << std::endl;

        bool addPoint{false};

        // Points belong to another line
        // The new line is a segment of the existing line
        // std::cout << "is pvbegin on line: " << pvbegin->getOnLine() << std::endl;
        // std::cout << "is pvend on line: " << pvend->getOnLine() << std::endl;
        if (pvbegin->getOnLine() || pvend->getOnLine()) {
          Baseline *p_on_bsl = pvbegin->getOnLine();
          if (!p_on_bsl) p_on_bsl = pvend->getOnLine();

          // std::cout << "\non line: " << p_on_bsl->getName() << std::endl;
          // p_on_bsl->print(pmessage, 9);

          if (pvbegin->getLinkToVertex())
            basepointLabelBegin = pvbegin->getLinkToVertex()->name();
          if (pvend->getLinkToVertex())
            basepointLabelEnd = pvend->getLinkToVertex()->name();

          int _id{0};
          while (1) {
            PDCELVertex *_pv_tmp = p_on_bsl->vertices()[_id];
            if (_pv_tmp->name() == basepointLabelBegin) {
              addPoint = true;
            }
            if (addPoint) {
              line->addPVertex(_pv_tmp);
            }
            if (addPoint && _pv_tmp->name() == basepointLabelEnd) {
              break;
            }
            if (p_on_bsl->isClosed() && _id == (p_on_bsl->vertices().size()-2)) {
              _id = 0;
            }
            else {
              _id++;
            }
          }
          // for (auto it = p_on_bsl->vertices().begin();
          //   it != p_on_bsl->vertices().end(); it++) {
          //   if ((*it)->name() == basepointLabelBegin)
          //     addPoint = true;
          //   if (addPoint)
          //     line->addPVertex(*it);
          //   if ((*it)->name() == basepointLabelEnd)
          //     break;
          // }
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

    double angle{atof(na->value())};

    PDCELVertex *pvMid, *pvStart, *pvEnd;
    // pvMid = pmodel->getPointByName(basepointLabel);
    pvMid = findPointByName(basepointLabel, p_xn_geo, pmodel, pmessage);
    if (!pvMid) {
      // TODO: raise error 'cannot find point' and exit.
    }

    SVector3 dir = getVectorFromAngle(angle, 0);
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

  return;
}









void readLineTypeCircle(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage) {
  // Need to discretize the circle,
  // create new basepoints and store
  // them into the baseline
  //   if (debug)
  //     std::cout << "Circular Baseline" << std::endl;
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
    if (discreteBy == "number")
      discreteNumber = atoi(v.c_str());
    else if (discreteBy == "angle")
      discreteAngle = atof(v.c_str());
  }

  PDCELVertex *pCenter;
  PDCELVertex *pStart;


  // CASE 1: Given center and radius
  if (nc && nr) {
    std::string centerLabel{nc->value()};
    double radius{atof(nr->value())};

    // pCenter = pmodel->getPointByName(centerLabel);
    pCenter = findPointByName(centerLabel, p_xn_geo, pmodel, pmessage);
    if (!pCenter) {
      // TODO: raise error 'cannot find point' and exit.
    }

    SVector3 vStart{0.0, radius, 0.0};
    pStart = new PDCELVertex(pCenter->point() + vStart.point());
    pmodel->addVertex(pStart);
  }
  
  
  // CASE 2: Given center and a point
  else if (nc && np) {
    std::string centerName{nc->value()};
    // pCenter = pmodel->getPointByName(centerName);
    pCenter = findPointByName(centerName, p_xn_geo, pmodel, pmessage);
    if (!pCenter) {
      // TODO: raise error 'cannot find point' and exit.
    }

    std::string startName{np->value()};
    // pStart = pmodel->getPointByName(startName);
    pStart = findPointByName(startName, p_xn_geo, pmodel, pmessage);
    if (!pStart) {
      // TODO: raise error 'cannot find point' and exit.
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
  PGeoArc arc{pCenter, pStart, direction};
  if (discreteBy == "number") {
    discretizeArcN(arc, discreteNumber, line, pmodel);
  } else if (discreteBy == "angle") {
    discretizeArcA(arc, discreteAngle, line, pmodel);
  }

  return;
}









void readLineTypeArc(Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage) {
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
    if (discreteBy == "number")
      discreteNumber = atoi(v.c_str());
    else if (discreteBy == "angle")
      discreteAngle = atof(v.c_str());
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

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_start) {
      // TODO: raise error 'cannot find point' and exit.
    }

    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_end) {
      // TODO: raise error 'cannot find point' and exit.
    }

    double r, k;

    if (p_xn_radius) {
      std::string r_str{p_xn_radius->value()};
      r = atof(r_str.c_str());
      k = 1.0 / r;
    }
    else if (p_xn_curv) {
      std::string k_str{p_xn_curv->value()};
      k = atof(k_str.c_str());
      if (k != 0) {
        r = 1.0 / fabs(k);
      }
    }

    // std::cout << "k = " << k << std::endl;


    if (k == 0.0) {
      // 0 curvature, straight line
      line->addPVertex(pv_start);
      SPoint3 sp_mid = (pv_start->point() + pv_end->point()) * 0.5;
      // std::cout << sp_mid << std::endl;
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

      // std::cout << "k = " << k << std::endl;


      SVector3 sv_n{side, 0, 0};

      // Calculate the center
      SVector3 sv_se{pv_start->point(), pv_end->point()};
      // std::cout << "sv_se = " << sv_se << std::endl;
      SVector3 sv_sm = sv_se * 0.5;
      // std::cout << "sv_sm = " << sv_sm << std::endl;

      double s = sv_se.norm();
      double p = sqrt(r*r - (s/2)*(s/2));
      // std::cout << "p = " << p << std::endl;

      SVector3 sv_mc = crossprod(sv_n, sv_se);
      // std::cout << "sv_mc = " << sv_mc << std::endl;
      sv_mc.normalize();
      // std::cout << "sv_mc = " << sv_mc << std::endl;
      sv_mc = sv_mc * p;
      // std::cout << "sv_mc = " << sv_mc << std::endl;
      SPoint3 sp_c = pv_start->point() + (sv_sm + sv_mc).point();
      // std::cout << "sp_c = " << sp_c << std::endl;
      pv_center = new PDCELVertex(sp_c);

      // std::cout << pv_center << std::endl;

      arc = PGeoArc{pv_center, pv_start, pv_end, direction};
    }
  }


  // CASE 1: Given center, start, end and angle
  else if (nc && ns && ne && na) {
    // std::string centerName{nc->value()};
    // pv_center = pmodel->getPointByName(centerName);
    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_center) {
      // TODO: raise error 'cannot find point' and exit.
    }

    // std::string startName{ns->value()};
    // pv_start = pmodel->getPointByName(startName);
    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_start) {
      // TODO: raise error 'cannot find point' and exit.
    }

    // std::string endName{ne->value()};
    // pv_end = pmodel->getPointByName(endName);
    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_end) {
      // TODO: raise error 'cannot find point' and exit.
    }

    double angle{atof(na->value())};
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

    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_center) {
      // TODO: raise error 'cannot find point' and exit.
    }

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_start) {
      // TODO: raise error 'cannot find point' and exit.
    }

    pv_end = findPointByName(ne->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_end) {
      // TODO: raise error 'cannot find point' and exit.
    }

    arc = PGeoArc{pv_center, pv_start, pv_end, direction};
  }


  // CASE 3: Given center, start and angle
  else if (nc && ns && na) {
    // std::string centerName{nc->value()};
    // std::string startName{ns->value()};
    // pv_center = pmodel->getPointByName(centerName);
    // pv_start = pmodel->getPointByName(startName);
    pv_center = findPointByName(nc->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_center) {
      // TODO: raise error 'cannot find point' and exit.
    }

    pv_start = findPointByName(ns->value(), p_xn_geo, pmodel, pmessage);
    if (!pv_start) {
      // TODO: raise error 'cannot find point' and exit.
    }

    double angle{atof(na->value())};
    arc = PGeoArc{pv_center, pv_start, angle, direction};

    pmodel->addVertex(arc.end());
  }

  if (discreteBy == "number")
    discretizeArcN(arc, discreteNumber, line, pmodel);
  else if (discreteBy == "angle")
    discretizeArcA(arc, discreteAngle, line, pmodel);

  return;
}









int readLineTypeAirfoil(
  Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo,
  PModel *pmodel, Message *pmessage, const double tol) {
  // std::cout << "reading airfoil type line\n";
  //
  xml_node<> *p_xn_pts{p_xn_line->first_node("points")};

  std::string data_form{"file"};
  xml_attribute<> *p_xa_data{p_xn_pts->first_attribute("data")};
  if (p_xa_data) {data_form = p_xa_data->value();}

  std::string format{"2"};
  xml_attribute<> *p_xa_format{p_xn_pts->first_attribute("format")};
  if (p_xa_format) {format = p_xa_format->value();}

  int head_rows{0};
  xml_attribute<> *p_xa_header{p_xn_pts->first_attribute("header")};
  if (p_xa_header) {
    std::string ss{p_xa_header->value()};
    if (ss[0] != '\0') {head_rows = atoi(ss.c_str());}
  }

  int flip{0};
  xml_node<> *p_xn_flip{p_xn_line->first_node("flip")};
  if (p_xn_flip) {
    std::string _ss{p_xn_flip->value()};
    if (_ss[0] != '\0') {flip = atoi(_ss.c_str());}
  }

  int reverse{0};
  xml_node<> *p_xn_reverse{p_xn_line->first_node("reverse")};
  if (p_xn_reverse) {
    std::string _ss{p_xn_reverse->value()};
    if (_ss[0] != '\0') {reverse = atoi(_ss.c_str());}
  }


  // Baseline *line_up = new Baseline(line->getName()+"_up", "straight");
  // Baseline *line_low = new Baseline(line->getName()+"_low", "straight");
  PDCELVertex *pv_le = new PDCELVertex("ple", 0, 0, 0);
  PDCELVertex *pv_te = new PDCELVertex("pte", 0, 1, 0);
  // PDCELVertex *pv_le_up = new PDCELVertex("pleup");
  // PDCELVertex *pv_le_low = new PDCELVertex("plelow");
  // PDCELVertex *pv_te_up = new PDCELVertex("pteup");
  // PDCELVertex *pv_te_low = new PDCELVertex("ptelow");

  if (data_form == "file") {
    std::string s_fn{p_xn_pts->value()};
    s_fn = config.file_directory + s_fn;
    std::ifstream _ifs{s_fn};
    if (!_ifs.is_open()) {
      PLOG(error) << pmessage->message("unable to open file: " + s_fn);
      return 1;
    }
    else {
      PLOG(info) << pmessage->message("reading airfoil data file: " + s_fn);
    }

    // Read data
    std::string data_line;
    int counter{0};
    // int which{1}; // 1: upper, -1: lower
    std::string _pv_tmp_name{"_tmp"};

    // Selig format
    // TE -> upper/lower surface -> LE -> lower/upper surface -> TE
    if (format == "1" || format[0] == 's' || format[0] == 'S') {

      int direction{1};  // 1: ccw, -1: cw
      xml_attribute<> *p_xa_direction{p_xn_pts->first_attribute("direction")};
      if (p_xa_direction) {
        std::string ss{p_xa_direction->value()};
        if (ss[0] != '\0') {direction = atoi(ss.c_str());}
      }
      // if (direction == -1) {which = -1;}  // Start from the lower surface

      // Add the point (1, 0), i.e., trailing edge
      // if (direction == 1) {
      //   line_up->addPVertex(pv_te);
      // }
      // else if (direction == -1) {
      //   line_low->addPVertex(pv_te);
      // }
      line->addPVertex(pv_te);

      // Add points in the file
      while (getline(_ifs, data_line)) {
        counter++;

        std::cout << "line " << counter << ": " << data_line << std::endl;

        // if (data_line.length() == 0) {continue;}

        // if (counter <= head_rows) {continue;}

        std::string trimmed_line = trim(data_line);

        if (trimmed_line.empty()) {continue;}


        PDCELVertex *_pv_tmp;
        std::stringstream _ss(trimmed_line);
        double x, y;
        _ss >> x >> y;

        std::cout << "(" << x << ", " << y << ")\n";

        // Skip the point (1, 0), i.e., trailing edge
        if (fabs(x-1) <= tol && fabs(y) <= tol) {
          continue;
        }

        // Add the point (0, 0), i.e., leading edge
        if (fabs(x) <= tol && fabs(y) <= tol) {
          line->addPVertex(pv_le);
          continue;
        }

        _pv_tmp = new PDCELVertex(_pv_tmp_name, 0, x, y);
        pmodel->addVertex(_pv_tmp);
        line->addPVertex(_pv_tmp);

      }

      // Add the point (1, 0), i.e., trailing edge
      line->addPVertex(pv_te);  // Closed airfoil

      // Rename key points
      // if (direction == 1) {
      //   line_up->vertices()[1]->setName("pteup");
      //   line_up->vertices()[line_up->vertices().size()-2]->setName("pleup");
      //   line_low->vertices()[1]->setName("plelow");
      //   line_low->vertices()[line_low->vertices().size()-2]->setName("ptelow");
      // }
      // else if (direction == -1) {
      //   line_up->vertices()[1]->setName("pleup");
      //   line_up->vertices()[line_up->vertices().size()-2]->setName("pteup");
      //   line_low->vertices()[1]->setName("ptelow");
      //   line_low->vertices()[line_low->vertices().size()-2]->setName("plelow");
      // }
    }
    // Lednicer format
    // LE -> upper surface -> TE
    // LE -> lower surface -> TE
    else if (format == "2" || format[0] == 'l' || format[0] == 'L') {
    }


    _ifs.close();
  }

  // Transformation
  if (flip) {
    std::string axis{"2"};
    double loc{0.5};
    double _x, _y, _z;

    // unsigned int _l{line->vertices().size()};
    std::size_t _l{line->vertices().size()};
    if (line->isClosed()) {_l = _l - 1;}

    for (int i = 0; i < _l; i++) {
      _x = line->vertices()[i]->x();
      if (axis == "2") {
        _y = 2 * loc - line->vertices()[i]->y();
        _z = line->vertices()[i]->z();
      }
      else if (axis == "3") {
        _y = line->vertices()[i]->y();
        _z = 2 * loc - line->vertices()[i]->z();
      }
      line->vertices()[i]->setPosition(_x, _y, _z);
    }
  }

  if (reverse) {
    std::reverse(line->vertices().begin(), line->vertices().end());
  }

  // line_up->print(pmessage, 9);
  // line_low->print(pmessage, 9);

  pmodel->addVertex(pv_le);
  pmodel->addVertex(pv_te);
  // pmodel->addVertex(pv_le_up);
  // pmodel->addVertex(pv_te_up);
  // pmodel->addVertex(pv_le_low);
  // pmodel->addVertex(pv_te_low);

  // pmodel->addBaseline(line_up);
  // pmodel->addBaseline(line_low);

  return 0;
}









int readLineByJoin(
  Baseline *line, const xml_node<> *p_xn_line, const xml_node<> *p_xn_geo,
  PModel *pmodel, Message *pmessage) {

  std::list<Baseline *> sublines;
  for (auto p_xn_subline = p_xn_line->first_node(); p_xn_subline;
       p_xn_subline = p_xn_subline->next_sibling()) {

    std::string subline_name = p_xn_subline->value();
    Baseline *subline = findLineByName(subline_name, p_xn_geo, pmodel, pmessage);
    subline->print(pmessage);
    sublines.push_back(subline);

  }

  joinCurves(line, sublines);
  // line->print(pmessage, 1);

  return 0;
}









PDCELVertex *getIntersectionWithWeb(double x, double angle, PDCELVertex *left, PDCELVertex *right){
  // Find the intersection of small segment with a straight web
  // std::cout << "\n[debug] function: getIntersectionWithWeb\n";

  SVector3 dir = getVectorFromAngle(angle, 0);
  double dir_x = dir.point().y();
  double dir_y = dir.point().z();
  double l_x = left->point().y();
  double l_y = left->point().z();
  double r_x = right->point().y();
  double r_y = right->point().z();

  double ratio = (x - l_x + l_y / dir_y * dir_x) / (r_x - l_x - (r_y - l_y) / dir_y * dir_x);
  
  SPoint3 interP = left->point() +  (right->point() - left->point()) * ratio;

  if (ratio > 0 && ratio < 1) {
    return new PDCELVertex(interP);
  } else {
    return nullptr;
  }
}









Baseline *findLineByName(
  const std::string &name, const xml_node<> *p_xn_geo, PModel *pmodel, Message *pmessage
  ) {
  Baseline *p_line = nullptr;

  p_line = pmodel->getBaselineByName(name);

  if (!p_line) {
    for (auto p_xn_bsl = p_xn_geo->first_node("line");
       p_xn_bsl; p_xn_bsl = p_xn_bsl->next_sibling("line")) {
      if (p_xn_bsl->first_attribute("name")->value() == name) {
        p_line = readXMLElementLine(p_xn_bsl, p_xn_geo, pmodel, pmessage);
        pmodel->addBaseline(p_line);
        break;
      }
    }
  }

  if (!p_line) {
    for (auto p_xn_bsl = p_xn_geo->first_node("baseline");
       p_xn_bsl; p_xn_bsl = p_xn_bsl->next_sibling("baseline")) {
      if (p_xn_bsl->first_attribute("name")->value() == name) {
        p_line = readXMLElementLine(p_xn_bsl, p_xn_geo, pmodel, pmessage);
        pmodel->addBaseline(p_line);
        break;
      }
    }
  }

  return p_line;

}




double getWebEnd(const xml_node<> *nodeIncludeBaseline, const std::string &filePath, double w_x, bool top) {
  std::string filenameAirfoil{nodeIncludeBaseline->value()};
  filenameAirfoil = filePath + filenameAirfoil + ".dat";
  std::ifstream fileAirfoils{filenameAirfoil};
  if (!fileAirfoils.is_open()) {
    std::cout << "Unable to open file: " << filenameAirfoil << std::endl;
    return 1;
  }
  std::string line;
  double l_x, l_y, r_x, r_y;
  double w_y;
  getline(fileAirfoils, line);
  std::stringstream ss(line);
  ss >> r_x >> r_y;
  while(getline(fileAirfoils, line)) {
    std::stringstream ss(line);
    ss >> l_x >> l_y;
    if ((w_x - l_x) * (w_x - r_x) <= 0) {
      if (top) {
        w_y = l_y + (r_y - l_y) / (r_x - l_x) * (w_x - l_x);
        return w_y;
      } else {
        top = true;
      }
    }
    r_x = l_x;
    r_y = l_y;
  }
  std::cout << "Web ends cannot be located on the airfoil " << w_x << std::endl;
  exit(1);
}



int addBaselinesFromAirfoil(const xml_node<> *nodeIncludeBaseline, PModel *pmodel,
                  const std::string &filePath, std::vector<std::pair<double, double>> websArray,
                  double dx, double dy, double dz, double s, double r, Message *pmessage) {
  // Use airfoil points to define baselines
  // No baselines xml needed

  // std::cout << "\n[debug] function: addBaselinesFromAirfoil\n";

  // Build vertex
  std::string filenameAirfoil{nodeIncludeBaseline->value()};
  filenameAirfoil = filePath + filenameAirfoil + ".dat";
  std::ifstream fileAirfoils{filenameAirfoil};
  if (!fileAirfoils.is_open()) {
    std::cout << "Unable to open file: " << filenameAirfoil << std::endl;
    return 1;
  } else {
    printInfo(i_indent, "reading airfoil definition: " + filenameAirfoil);
  }

  int counter = 1;
  std::string line;
  bool firstRun{true};
  while (getline(fileAirfoils, line)) {
    std::stringstream ss(line);
    std::string label=std::to_string(counter);
    double x, y;
    ss >> x >> y;

    if (debug) {
      std::cout << "[debug] reading point: " << label << " (" << x << ", " <<
      y << ")" << std::endl;
    }
    PDCELVertex *pv = new PDCELVertex{label, 0, x, y};
    for (auto it = websArray.begin(); it!= websArray.end(); ++it) {
      if (!firstRun) {
        PDCELVertex *intersectionPoint = getIntersectionWithWeb((*it).first, (*it).second, pmodel->vertices().back(), pv);
        if (intersectionPoint) {
          intersectionPoint->setName("int_" + std::to_string(counter));
          pmodel->addVertex(intersectionPoint);
          counter = counter + 1;
        }
      }
    }
    firstRun = false;
    pmodel->addVertex(pv);
    counter = counter + 1;
  }
  fileAirfoils.close();

  // Build baselines
  std::vector<std::string> baselineNames{"head"};
  for (int n = 1; n < websArray.size(); n++) {
    baselineNames.push_back("bot_" + std::to_string(n) + "_" + std::to_string(n+1));
    baselineNames.insert(baselineNames.begin(), "top_" + std::to_string(n) + "_" + std::to_string(n+1));
  }
  baselineNames.push_back("tail_bot");
  baselineNames.insert(baselineNames.begin(), "tail_top");
  std::vector<Baseline*> baselines;
  for (auto it = baselineNames.begin(); it != baselineNames.end(); ++it) {
    baselines.push_back(new Baseline(*(it), "straight"));
  }

  std::vector<Baseline*>::iterator baseline = baselines.begin();

  bool _webFlag{false};
  for (auto it = pmodel->vertices().begin(); 
            it != pmodel->vertices().end(); ++it) {
      if (_webFlag || (*it)->name().find("Pweb") == std::string::npos) {
        _webFlag = true;
        (*baseline)->addPVertex(*it);

        if ((*it)->name().find("int") != std::string::npos) {
          ++baseline;
          (*baseline)->addPVertex(*it);
        }
      }
  }
  baseline = baselines.begin();
  Baseline *curve = baselines.back();
  (*baseline)->join(curve, 0, false);
  (*baseline)->setName("tail");
  for (auto it = baselines.begin(); it != std::prev(baselines.end()); ++it) {
    pmodel->addBaseline(*it);
  }

  return 0;
}









int addBaselineByPointAndAngle(PModel *pmodel, std::string name, PDCELVertex *pvMid, double angle) {
  // Given a point and an angle
  // std::cout << "\n[debug] function: addBaselineByPointAndAngle\n";

  PDCELVertex *pvStart, *pvEnd;
  Baseline *baseline = new Baseline(name, "straight");

  SVector3 dir = getVectorFromAngle(angle, 0);
  pvStart = new PDCELVertex( "P" + name + "_start", pvMid->point() - dir.point());
  pvEnd = new PDCELVertex("P" + name + "_end", pvMid->point() + dir.point());

  // std::cout << "pvStart = " << pvStart << std::endl;
  // std::cout << "pvMid = " << pvMid << std::endl;
  // std::cout << "pvEnd = " << pvEnd << std::endl;

  pmodel->addVertex(pvStart);
  pmodel->addVertex(pvMid);
  pmodel->addVertex(pvEnd);
  baseline->addPVertex(pvStart);
  baseline->addPVertex(pvMid);
  baseline->addPVertex(pvEnd);

  pmodel->addBaseline(baseline);

  return 0;
}
