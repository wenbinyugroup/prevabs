#include "PDCELFace.hpp"

#include "Material.hpp"
#include "PArea.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELUtils.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include "geo_types.hpp"
#include <string>
#include <vector>

PDCELFace::PDCELFace() {
  _outer = nullptr;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _is_bounded = true;
}

PDCELFace::PDCELFace(PDCELHalfEdge *outer) {
  _outer = outer;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _is_bounded = true;
}

PDCELFace::PDCELFace(PDCELHalfEdge *outer, bool build) {
  _outer = outer;
  _area = nullptr;
  _material = nullptr;
  _layertype = nullptr;

  _y2 = SVector3(0, 1, 0);

  _is_bounded = build;
}

void PDCELFace::print() {

  PDCELHalfEdge *he;
  // Print the outer boundary
  if (_outer == nullptr) {
    PLOG(debug) << "unbounded face.";
  } else {
    PLOG(debug) << "outer boundary: ";
    walkLoopWithLimit(_outer, [](PDCELHalfEdge *he) {
      PLOG(debug) << he->printString();
    });

    for (auto _inner : _inners) {
      PLOG(debug) << "inner boundary: ";
      walkLoopWithLimit(_inner, [](PDCELHalfEdge *he) {
        PLOG(debug) << he->printString();
      });
    }
  }

  if (_layertype != nullptr) {
    PLOG(debug) << "layer type: " << _layertype;
  }

  std::cout << std::endl;
}

PDCELHalfEdge *PDCELFace::getOuterHalfEdgeWithSource(PDCELVertex *v) {
  PDCELHalfEdge *he = _outer;
  int _iter = 0;
  do {
    if (++_iter > kDCELLoopHardCap) {
      throw std::runtime_error(
          "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
          " iterations in getOuterHalfEdgeWithSource at " +
          _outer->printString());
    }
    if (he->source() == v) {
      return he;
    }
    he = he->next();
  } while (he != _outer);

  return nullptr;
}

PDCELHalfEdge *PDCELFace::getOuterHalfEdgeWithTarget(PDCELVertex *v) {
  PDCELHalfEdge *he = _outer;
  int _iter = 0;
  do {
    if (++_iter > kDCELLoopHardCap) {
      throw std::runtime_error(
          "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
          " iterations in getOuterHalfEdgeWithTarget at " +
          _outer->printString());
    }
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
    int _iter = 0;
    do {
      if (++_iter > kDCELLoopHardCap) {
        throw std::runtime_error(
            "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
            " iterations in getInnerHalfEdgeWithSource at " +
            inner->printString());
      }
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
    int _iter = 0;
    do {
      if (++_iter > kDCELLoopHardCap) {
        throw std::runtime_error(
            "DCEL loop walk exceeded " + std::to_string(kDCELLoopHardCap) +
            " iterations in getInnerHalfEdgeWithTarget at " +
            inner->printString());
      }
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
