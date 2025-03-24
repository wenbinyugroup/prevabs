#include "PArea.hpp"

#include "Material.hpp"
#include "PDCELFace.hpp"
#include "PGeoClasses.hpp"
#include "PModelIO.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh_mod/SVector3.h"

#include <cstdio>
#include <list>
#include <string>
#include <vector>

PArea::PArea() {
  _pmodel = nullptr;
  _segment = nullptr;
  _base = nullptr;
  _opposite = nullptr;
  _y2 = SVector3(0, 1, 0);
  _y3 = SVector3(0, 0, 1);
  _face = nullptr;
  _line_segment_base = nullptr;
}

PArea::PArea(PModel *pmodel, Segment *segment) {
  _pmodel = pmodel;
  _segment = segment;
  _base = nullptr;
  _opposite = nullptr;
  _y2 = SVector3(0, 1, 0);
  _y3 = SVector3(0, 0, 1);
  _face = nullptr;
  _line_segment_base = nullptr;
}

void PArea::print() {
  std::cout << "prev bound vector: " << _prev_bound << std::endl;
  std::cout << "prev bound vertices: " << std::endl;
  for (auto v : _prev_bound_vertices) {
    std::cout << " " << v << std::endl;
  }

  std::cout << "next bound vector: " << _next_bound << std::endl;
  std::cout << "next bound vertices: " << std::endl;
  for (auto v : _next_bound_vertices) {
    std::cout << " " << v << std::endl;
  }

  std::cout << std::endl;
}

SVector3 PArea::localy3() {
  return crossprod(SVector3(1, 0, 0), _y2);
}

void PArea::setSegment(Segment *segment) {
  _segment = segment;
}

void PArea::addFace(PDCELFace *face) {
  _faces.push_back(face);
}

// void PArea::setLocaly2(SVector3 y2) {
//   _y2 = y2;
// }

// void PArea::setLocaly3(SVector3 y3) {
//   _y3 = y3;
// }

void PArea::setPrevBound(SVector3 &prev) {
  _prev_bound = prev;
}

void PArea::setNextBound(SVector3 &next) {
  _next_bound = next;
}

void PArea::setPrevBoundVertices(std::vector<PDCELVertex *> vertices) {
  _prev_bound_vertices = vertices;
}

void PArea::setNextBoundVertices(std::vector<PDCELVertex *> vertices) {
  _next_bound_vertices = vertices;
}

void PArea::addPrevBoundVertex(PDCELVertex *v) {
  _prev_bound_vertices.push_back(v);
}

void PArea::addNextBoundVertex(PDCELVertex *v) {
  _next_bound_vertices.push_back(v);
}

//
//
//
//
//

void PArea::buildLayers(Message *pmessage) {

  PLOG(debug) << pmessage->message("building layers for area: " + _face->name());

  Layup *layup = _segment->getLayup();
  std::string lside = _segment->getLayupside();

  std::list<PDCELFace *> fnew;
  Layer layer;
  std::string area_name = _face->name();
  
  // For the paired vertices in both bounds
  // simply connect them

  PLOG(debug) << "prev wall vertices:\n"
              << vertices_to_string(_prev_wall_vertices);
  PLOG(debug) << "next wall vertices:\n"
              << vertices_to_string(_next_wall_vertices);

  for (int i = 1; i < _prev_wall_vertices.size() - 1; ++i) {
    PLOG(debug) << "layer " << i;

    layer = layup->getLayers()[i - 1];

    // Split the face
    fnew = _segment->pmodel()->dcel()->splitFace(
      _face, _prev_wall_vertices[i], _next_wall_vertices[i]);

    if (lside == "left") {
      _faces.push_back(fnew.back());
      _face = fnew.front();
    } else {
      _faces.push_back(fnew.front());
      _face = fnew.back();
    }

    _faces.back()->setName(area_name + "_layer_" + std::to_string(i));
    _faces.back()->setMaterial(layer.getLamina()->getMaterial());
    _faces.back()->setTheta3(layer.getAngle());
    _faces.back()->setLayerType(layer.getLayerType());
    _faces.back()->setLocaly1(_y1);
    _faces.back()->setLocaly2(_y2);

    if (config.analysis_tool == 1) {
      _faces.back()->setTheta1(_faces.back()->calcTheta1Fromy2(_y2));
    }

    if (config.debug) {
      // fprintf(config.fdeb, "        new layer face:\n");
      // writeFace(config.fdeb, _faces.back());
    }
  }

  // The last or the only layer
  PLOG(debug) << "layer " << _prev_wall_vertices.size() - 1;

  _faces.push_back(_face);

  layer = layup->getLayers().back();

  _faces.back()->setName(area_name + "_layer_" + std::to_string(layup->getLayers().size()));
  _faces.back()->setMaterial(layer.getLamina()->getMaterial());
  _faces.back()->setTheta3(layer.getAngle());
  _faces.back()->setLayerType(layer.getLayerType());
  _faces.back()->setLocaly1(_y1);
  _faces.back()->setLocaly2(_y2);

  if (config.analysis_tool == 1) {
    // SwiftComp
    _faces.back()->setTheta1(_faces.back()->calcTheta1Fromy2(_y2));
  }

  if (config.debug) {
    // fprintf(config.fdeb, "        new layer face:\n");
    // writeFace(config.fdeb, _faces.back());
  }

  // pmessage->decreaseIndent();

  PLOG(debug) << pmessage->message("done");

  return;
}
