#pragma once

#include "declarations.hpp"
#include "Material.hpp"
#include "PArea.hpp"
#include "PDCELFace.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "PModel.hpp"
#include "globalConstants.hpp"
#include "PBaseLine.hpp"
#include "utilities.hpp"

#include "gmsh/SVector3.h"

#include <iostream>
#include <list>
#include <string>
#include <vector>

class Baseline;
class PArea;
class PDCELVertex;
class PDCELFace;
class PGeoLineSegment;
class PModel;

/** @ingroup cs
 * A cross-sectional segment class.
 */
class Segment {
private:
  PModel *_pmodel;
  std::string _name;
  Baseline *_curve_base, *_curve_offset;

  // normalized location of the beginning and ending of the segmnet on the base line
  double _u_begin{0}, _u_end{1};

  Layup *_layup;
  std::vector<PArea *> _areas;
  std::string slayupside;
  int slevel;
  Segment *_prev, *_next;
  SVector3 _prev_bound, _next_bound;
  bool _closed;

  // excluding vertices on the base curve and offset curve
  std::vector<PDCELVertex *> _prev_bound_vertices, _next_bound_vertices;
  std::vector<int> _prev_bound_indices, _next_bound_indices;
  std::list<PDCELVertex *> _vertices_outer;
  PDCELFace *_face;
  int _free; // Free end. 0 (head) or 1 (tail)
  PDCELVertex *_head_vertex_offset, *_tail_vertex_offset;

  // Has the size of base curve - 2 (excluding the two ends)
  std::list<PDCELVertex *> _inner_bounds_end;
  std::list<PGeoLineSegment *> _inner_bounds;
  std::list<PGeoLineSegment *> _inner_bounds_tt;

  std::vector<int> _offset_vertices_link_to;
  std::vector<int> _offset_indices_base_link_to;
  std::vector<std::vector<int> > _base_offset_indices_pairs;

  // Inner bounding line segments in the following index range (w.r.t base line)
  // can create the DCEL edge directly, without trimming
  int _ib_begin, _ib_end;

  // Indicate if each inner bound can be created by direct connecting;
  std::list<bool> _inner_bounds_dc;

public:
  static int count_tmp;

  Segment(){};
  Segment(std::string name, Baseline *p_baseline, Layup *p_layup,
          std::string layupside, int level = 1)
      : _name(name), _curve_base(p_baseline), _curve_offset(nullptr),
        _layup(p_layup), slayupside(layupside), slevel(level),
        _head_vertex_offset(nullptr), _tail_vertex_offset(nullptr) {
    _prev = nullptr;
    _next = nullptr;
    _prev_bound = SVector3(0, 0, 0);
    _next_bound = SVector3(0, 0, 0);
    _ib_begin = 0;
    _ib_end = 0;
  }
  Segment(PModel *pmodel, std::string name, Baseline *p_baseline,
          Layup *p_layup, std::string layupside, int level = 1)
      : _pmodel(pmodel), _name(name), _curve_base(p_baseline),
        _curve_offset(nullptr), _layup(p_layup), slayupside(layupside),
        slevel(level), _head_vertex_offset(nullptr),
        _tail_vertex_offset(nullptr) {
    _prev = nullptr;
    _next = nullptr;
    _prev_bound = SVector3(0, 0, 0);
    _next_bound = SVector3(0, 0, 0);
    _ib_begin = 0;
    _ib_end = 0;
  }

  friend std::ostream &operator<<(std::ostream &, Segment *);
  void print();
  void printBaseOffsetLink();

  PModel *pmodel() { return _pmodel; }
  std::string getName() { return _name; }
  Baseline *getBaseline() { return _curve_base; }
  Baseline *curveBase() { return _curve_base; }
  Baseline *curveOffset() { return _curve_offset; }
  double getUBegin() const { return _u_begin; }
  double getUEnd() const { return _u_end; }
  Layup *getLayup() { return _layup; }
  std::string getLayupside() { return slayupside; }
  int getLevel() { return slevel; }
  int free() { return _free; }
  int layupSide();
  bool closed() { return _closed; }

  PDCELVertex *getBeginVertex();
  PDCELVertex *getEndVertex();

  PDCELVertex *headVertexOffset() { return _head_vertex_offset; }
  PDCELVertex *tailVertexOffset() { return _tail_vertex_offset; }

  SVector3 getBeginTangent();
  SVector3 getEndTangent();

  Segment *prevSegment() { return _prev; }
  Segment *nextSegment() { return _next; }
  SVector3 prevBound() { return _prev_bound; }
  SVector3 nextBound() { return _next_bound; }
  std::vector<PDCELVertex *> &prevBoundVertices() {
    return _prev_bound_vertices;
  }
  std::vector<PDCELVertex *> &nextBoundVertices() {
    return _next_bound_vertices;
  }
  int innerBoundIndexBegin() { return _ib_begin; }
  int innerBoundIndexEnd() { return _ib_end; }
  std::vector<int> &prevBoundIndices() { return _prev_bound_indices; }
  std::vector<int> &nextBoundIndices() { return _next_bound_indices; }
  std::vector<int> &offsetVerticesLinkToList() { return _offset_vertices_link_to; }
  std::vector<int> &baseOffsetIndicesLink() { return _offset_indices_base_link_to; }
  std::vector<std::vector<int>> &baseOffsetIndicesPairs() { return _base_offset_indices_pairs; }

  PDCELFace *face() { return _face; }

  void setClosed(bool t) { _closed = t; }

  void setCurveBase(Baseline *c) { _curve_base = c; }
  void setCurveOffset(Baseline *c) { _curve_offset = c; }

  void setUBegin(double u) { _u_begin = u; }
  void setUEnd(double u) { _u_end = u; }

  void setPModel(PModel *pmodel) { _pmodel = pmodel; }
  void addArea(PArea *);

  void setHeadVertexOffset(PDCELVertex *v) { _head_vertex_offset = v; }
  void setTailVertexOffset(PDCELVertex *v) { _tail_vertex_offset = v; }

  void setLevel(int level) { slevel = level; }
  void setFreeEnd(int fe) { _free = fe; }
  void setPrevSegment(Segment *seg) { _prev = seg; }
  void setNextSegment(Segment *seg) { _next = seg; }
  void setPrevBound(SVector3 &bound) { _prev_bound = bound; }
  void setNextBound(SVector3 &bound) { _next_bound = bound; }
  void setPrevBoundVertices(std::vector<PDCELVertex *>);
  void setNextBoundVertices(std::vector<PDCELVertex *>);
  void setInnerBoundIndexBegin(int i) { _ib_begin = i; }
  void setInnerBoundIndexEnd(int i) { _ib_end = i; }
  void setPrevBoundIndices(std::vector<int> indices) {
    _prev_bound_indices = indices;
  }
  void setNextBoundIndices(std::vector<int> indices) {
    _next_bound_indices = indices;
  }

  void setPDCELFace(PDCELFace *face) { _face = face; }

  void offsetCurveBase(Message *);

  void build(Message *);
  void buildAreas(Message *);
};

// ===================================================================
// class Connection {
// private:
//   std::string cname;
//   std::string ctype;
//   // std::vector<Segment*> p_csegments;
//   std::vector<std::string> csegments;

// public:
//   Connection(std::string name, std::string type,
//              std::vector<std::string> segments)
//       : cname(name), ctype(type), csegments(segments) {}

//   std::string getName() { return cname; }
//   std::string getType() { return ctype; }
//   std::vector<std::string> getSegments() { return csegments; }

//   void setType(std::string);
//   // void setSegments(std::vector<Segment>);
// };

// ===================================================================
class Filling {
private:
  std::string fname;
  Baseline *p_fbaseline;
  Material *p_fmaterial;
  LayerType *p_flayertype;
  std::string ffillside;

public:
  Filling() {}
  Filling(std::string name, Baseline *p_baseline, Material *p_material,
          std::string fillside)
      : fname(name), p_fbaseline(p_baseline), p_fmaterial(p_material),
        ffillside(fillside) {}
  Filling(std::string name, Baseline *p_baseline, Material *p_material,
          std::string fillside, LayerType *p_layertype)
      : fname(name), p_fbaseline(p_baseline), p_fmaterial(p_material),
        ffillside(fillside), p_flayertype(p_layertype) {}

  std::string getName() { return fname; }
  Baseline *getBaseline() { return p_fbaseline; }
  Material *getMaterial() { return p_fmaterial; }
  LayerType *getLayerType() { return p_flayertype; }
  std::string getFillSide() { return ffillside; }

  void setLayerType(LayerType *);
};
