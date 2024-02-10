#include "PDCELFace.hpp"

#include "Material.hpp"
#include "PArea.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include "gmsh/GFace.h"
#include "gmsh/SVector3.h"

#include <string>
#include <vector>

PDCELFace::PDCELFace() {
  std::string _name = "null";

  _outer = nullptr;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _gbuild = true;
  _gface = nullptr;
}

PDCELFace::PDCELFace(PDCELHalfEdge *outer) {
  std::string _name = "null";

  _outer = outer;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _gbuild = true;
  _gface = nullptr;
}

PDCELFace::PDCELFace(PDCELHalfEdge *outer, bool build) {
  std::string _name = "null";

  _outer = outer;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _gbuild = build;
  _gface = nullptr;
}









void PDCELFace::print() {
  PLOG(debug) << "name: " << _name;


  PDCELHalfEdge *he;
  // Print the outer boundary
  if (_outer == nullptr) {
    PLOG(debug) << "unbounded face.";
  } else {
    PLOG(debug) << "outer boundary: ";
    he = _outer;
    do {
      PLOG(debug) << he->printString();
      he = he->next();
    } while (he != _outer);

    for (auto _inner : _inners) {
      PLOG(debug) << "inner boundary: ";
      he = _inner;
      do {
        PLOG(debug) << he->printString();
        he = he->next();
      } while (he != _inner);
    }
  }

  if (_layertype != nullptr) {
    PLOG(debug) << "layer type: " << _layertype;
  }

  std::cout << std::endl;
}









PDCELHalfEdge *PDCELFace::getOuterHalfEdgeWithSource(PDCELVertex *v) {
  PDCELHalfEdge *he = _outer;
  do {
    if (he->source() == v) {
      return he;
    }
    he = he->next();
  } while (he != _outer);

  return nullptr;
}









PDCELHalfEdge *PDCELFace::getOuterHalfEdgeWithTarget(PDCELVertex *v) {
  PDCELHalfEdge *he = _outer;
  do {
    if (he->source() == v) {
      return he->prev();
    }
    he = he->next();
  } while (he != _outer);

  return nullptr;
}









PDCELHalfEdge *PDCELFace::getInnerHalfEdgeWithSource(PDCELVertex *v) {
  for (auto inner : _inners) {
    PDCELHalfEdge *he = inner;
    do {
      if (he->source() == v) {
        return he;
      }
      he = he->next();
    } while (he != inner);
  }

  return nullptr;
}









PDCELHalfEdge *PDCELFace::getInnerHalfEdgeWithTarget(PDCELVertex *v) {
  for (auto inner : _inners) {
    PDCELHalfEdge *he = inner;
    do {
      if (he->source() == v) {
        return he->prev();
      }
      he = he->next();
    } while (he != inner);
  }

  return nullptr;
}




double PDCELFace::calcTheta1Fromy2(SVector3 v, bool deg) {
  double theta1 = atan2(v[2], v[1]);
  if (deg) {
    theta1 = rad2deg(theta1);
  }
  return theta1;
}




SVector3 PDCELFace::calcy2FromTheta1(double theta1, bool deg) {
  if (deg) {
    theta1 = deg2rad(theta1);
  }
  return SVector3(0, cos(theta1), sin(theta1));
}
