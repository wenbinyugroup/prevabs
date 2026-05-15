#include "PArea.hpp"

#include "CurveFrameLookup.hpp"
#include "Material.hpp"
#include "PBaseLine.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "geo_types.hpp"

#include <cstdio>
#include <list>
#include <string>
#include <vector>

namespace {

// Average of the source vertices along the face's outer boundary loop,
// projected onto the cross-section yz plane (cf. PreVABS convention).
// Returns false when the loop is degenerate (no vertices traversed).
bool computeFaceCentroid2D(PDCELFace *face, SPoint2 &out) {
  if (!face) return false;
  PDCELHalfEdge *start = face->outer();
  if (!start) return false;

  double sy = 0.0, sz = 0.0;
  int n = 0;
  PDCELHalfEdge *he = start;
  do {
    PDCELVertex *v = he->source();
    if (v) {
      sy += v->y();
      sz += v->z();
      ++n;
    }
    he = he->next();
    if (!he) return false;
  } while (he != start);

  if (n == 0) return false;
  out = SPoint2(sy / n, sz / n);
  return true;
}

}  // namespace

PArea::PArea() {
  _segment = nullptr;
  _base = nullptr;
  _opposite = nullptr;
  _y2 = SVector3(0, 1, 0);
  _y3 = SVector3(0, 0, 1);
  _face = nullptr;
  _line_segment_base = nullptr;
}

PArea::PArea(Segment *segment) {
  _segment = segment;
  _base = nullptr;
  _opposite = nullptr;
  _y2 = SVector3(0, 1, 0);
  _y3 = SVector3(0, 0, 1);
  _face = nullptr;
  _line_segment_base = nullptr;
}

PArea::~PArea() {
  delete _line_segment_base;
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

void PArea::buildLayers(const BuilderConfig &bcfg) {
  // std::cout << std::endl;
  // std::cout << "- building layers for area: " << _face->name() << std::endl;
  // if (bcfg.debug_level >= DebugLevel::join) {
  //   // fprintf(config.fdeb, "- building area layers: %s\n", _face->name().c_str());
  // }
    if (bcfg.debug_level >= DebugLevel::join) PLOG(debug) << "building layers for area: " + bcfg.model->faceData(_face).name;

  // std::cout << "        area face:" << std::endl;
  // _face->print();

  // std::cout << "        prev bound vertices:" << std::endl;
  // for (auto v : _prev_bound_vertices) {
  //   std::cout << "        " << v << std::endl;
  // }

  // std::cout << "        next bound vertices:" << std::endl;
  // for (auto v : _next_bound_vertices) {
  //   std::cout << "        " << v << std::endl;
  // }
  
  // std::cout << "        layup:" << std::endl;
  // _segment->getLayup()->printLayup();

  Layup *layup = _segment->getLayup();
  std::string lside = _segment->getLayupside();

  std::list<PDCELFace *> fnew;
  // PGeoLineSegment *ls_offset, *ls_prev;
  Layer layer;
  // double thk;
  std::string area_name = bcfg.model->faceData(_face).name;

  // std::cout << "        line segment _line_segment_base: " << _line_segment_base << std::endl;

  
  // For the paired vertices in both bounds
  // simply connect them

  for (int i = 0; i < _prev_bound_vertices.size(); ++i) {
    // std::cout << std::endl;
    // std::cout << "[debug] layer " << i + 1 << std::endl;
    if (bcfg.debug_level >= DebugLevel::join) {
      // fprintf(config.fdeb, "        layer %d\n", i + 1);
    }

    layer = layup->getLayers()[i];
    // std::cout << "        layer type: " << layer.getLayerType() << std::endl;

    // Split the face
    // fnew = _segment->pmodel()->dcel()->splitFace(_face, ls_offset);
    fnew = bcfg.dcel->splitFace(_face, _prev_bound_vertices[i], _next_bound_vertices[i]);
    // std::cout << "        new faces:" << std::endl;
    // fnew.front()->print();
    // fnew.back()->print();

    if (lside == "left") {
      _faces.push_back(fnew.back());
      _face = fnew.front();
    } else {
      _faces.push_back(fnew.front());
      _face = fnew.back();
    }

    // std::cout << "        setting new layer face properties" << std::endl;
    // std::cout << "        name" << std::endl;
    bcfg.model->setFaceName(
        _faces.back(), area_name + "_layer_" + std::to_string(i + 1));
    // std::cout << "        material" << std::endl;
    _faces.back()->setMaterial(layer.getLamina()->getMaterial());
    // std::cout << "        theta3" << std::endl;
    _faces.back()->setTheta3(layer.getAngle());
    // std::cout << "        layer type" << std::endl;
    _faces.back()->setLayerType(layer.getLayerType());
    // std::cout << "        local y2" << std::endl;
    _faces.back()->setLocaly1(_y1);
    _faces.back()->setLocaly2(_y2);

    if (bcfg.tool == AnalysisTool::VABS) {
      _faces.back()->setTheta1(_faces.back()->calcTheta1Fromy2(_y2));
    }

    // if (_segment->getName() == "sgm_18") {
    //   std::cout << "        new layer face:" << std::endl;
    //   _faces.back()->print();
    // }
    if (bcfg.debug_level >= DebugLevel::join) {
      // fprintf(config.fdeb, "        new layer face:\n");
      // writeFace(config.fdeb, _faces.back());
    }
  }

  // The last or the only layer
  // std::cout << "[debug] layer " << layup->getLayers().size() << std::endl;
  _faces.push_back(_face);

  layer = layup->getLayers().back();
  // layer = layup->getLayers()[i];
  // std::cout << "        layer type: " << layer.getLayerType() << std::endl;
  bcfg.model->setFaceName(
      _faces.back(),
      area_name + "_layer_" + std::to_string(layup->getLayers().size()));
  _faces.back()->setMaterial(layer.getLamina()->getMaterial());
  _faces.back()->setTheta3(layer.getAngle());
  _faces.back()->setLayerType(layer.getLayerType());
  _faces.back()->setLocaly1(_y1);
  _faces.back()->setLocaly2(_y2);
  if (bcfg.tool == AnalysisTool::VABS) {
    _faces.back()->setTheta1(_faces.back()->calcTheta1Fromy2(_y2));
  }
  // if (_segment->getName() == "sgm_18") {
  //   std::cout << "        new layer face:" << std::endl;
  //   _faces.back()->print();
  // }
  if (bcfg.debug_level >= DebugLevel::join) {
    // fprintf(config.fdeb, "        new layer face:\n");
    // writeFace(config.fdeb, _faces.back());
  }

  // Phase B (plan-20260514-decouple-local-frame-from-map.md):
  // recompute per-face local frame from the base curve via nearest-segment
  // lookup. Replaces the implicit "every face shares the area's _y1/_y2"
  // assumption when the segment's mat-orient selector is "baseline".
  applyFrameFromBaseCurve(bcfg);

  return;
}

void PArea::applyFrameFromBaseCurve(const BuilderConfig &bcfg) {
  if (!_segment) return;

  const std::string &e1 = _segment->getMatOrient1();
  const std::string &e2 = _segment->getMatOrient2();
  const bool e1_baseline = (e1 == "baseline");
  const bool e2_baseline = (e2 == "baseline");
  if (!e1_baseline && !e2_baseline) return;

  Baseline *base = _segment->curveBase();
  if (!base || base->vertices().size() < 2) return;

  // Build a 2-D polyline from the base curve in the yz cross-section plane.
  const auto &verts = base->vertices();
  std::vector<SPoint2> poly;
  poly.reserve(verts.size());
  for (auto *v : verts) {
    poly.emplace_back(v->y(), v->z());
  }
  const bool closed = base->isClosed();
  // For a closed Baseline the front and back entries are the same vertex;
  // drop the duplicate so CurveFrameLookup's implicit closing segment
  // (when constructed with closed=true) is non-degenerate.
  if (closed && poly.size() > 1
      && poly.front().x() == poly.back().x()
      && poly.front().y() == poly.back().y()) {
    poly.pop_back();
  }
  if (poly.size() < 2) return;

  CurveFrameLookup lookup(poly, closed);

  for (auto *face : _faces) {
    SPoint2 c;
    if (!computeFaceCentroid2D(face, c)) continue;
    const SVector3 t = lookup.yAxisAt(c);
    if (e1_baseline) {
      face->setLocaly1(t);
    }
    if (e2_baseline) {
      face->setLocaly2(t);
      if (bcfg.tool == AnalysisTool::VABS) {
        face->setTheta1(face->calcTheta1Fromy2(t));
      }
    }
  }
}
