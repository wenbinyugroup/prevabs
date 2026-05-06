#pragma once

// Forward declarations first — before any includes — to break circular
// dependencies in the include chain.
class Baseline;
class LayerType;
class Layup;
class PArea;
class PDCELVertex;
class PDCELFace;
class PGeoLineSegment;
struct BuilderConfig;

#include "geo_types.hpp"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

/** @ingroup cs
 * A cross-sectional segment class.
 */
class Segment {
public:
  // Segment lifecycle is strictly ordered:
  // BaseReady -> OffsetReady -> ShellBuilt -> AreasBuilt.
  // offsetCurveBase() is idempotent while the segment remains OffsetReady;
  // build() and buildAreas() are single-use transitions.
  enum class LifecycleState {
    BaseReady,
    OffsetReady,
    ShellBuilt,
    AreasBuilt
  };

private:
  std::string _name;
  Baseline *_curve_base;
  std::unique_ptr<Baseline> _curve_offset;

  Layup *_layup;
  std::vector<std::unique_ptr<PArea>> _areas;
  // Layup offset side relative to the directed base curve:
  // "left" means positive offset from base start -> end,
  // "right" means negative offset.
  std::string _layupside;
  int _level;
  Segment *_prev, *_next;
  SVector3 _prev_bound, _next_bound;

  // Local material-axis selectors used when each PArea assigns face axes.
  // e1/e2 are interpreted relative to the area geometry created from this
  // segment:
  // - "baseline": tangent of the local base-side segment of the area
  // - "layup":    through-thickness connector from base side to offset side
  // - "normal":   cross-section normal (the global x1 direction)
  std::string _mat_orient_e1{"normal"};
  std::string _mat_orient_e2{"baseline"};

  // excluding vertices on the base curve and offset curve
  std::vector<PDCELVertex *> _prev_bound_vertices, _next_bound_vertices;
  std::vector<int> _prev_bound_indices, _next_bound_indices;
  PDCELFace *_face;
  int _free; // Free end. 0 (head) or 1 (tail)
  PDCELVertex *_head_vertex_offset, *_tail_vertex_offset;
  BaseOffsetMap _base_offset_indices_pairs;
  LifecycleState _state{LifecycleState::BaseReady};

public:
  static int count_tmp;

  Segment(){};
  ~Segment();
  Segment(std::string name, Baseline *p_baseline, Layup *p_layup,
          std::string layupside, int level = 1)
      : _name(name), _curve_base(p_baseline),
        _layup(p_layup), _layupside(layupside), _level(level),
        _head_vertex_offset(nullptr), _tail_vertex_offset(nullptr) {
    _prev = nullptr;
    _next = nullptr;
    _prev_bound = SVector3(0, 0, 0);
    _next_bound = SVector3(0, 0, 0);
  }
  Segment(const Segment &) = delete;
  Segment &operator=(const Segment &) = delete;
  Segment(Segment &&other) noexcept;
  Segment &operator=(Segment &&other) noexcept;
  friend std::ostream &operator<<(std::ostream &, Segment *);
  void print();
  void printBaseOffsetLink();
  void printBaseOffsetPairs();

  std::string getName() { return _name; }
  Baseline *curveBase() { return _curve_base; }
  Baseline *curveOffset() { return _curve_offset.get(); }
  Layup *getLayup() { return _layup; }
  std::string getLayupside() { return _layupside; }
  int getLevel() { return _level; }
  int free() { return _free; }
  // Returns +1 for "left" and -1 for "right" with respect to the directed
  // base curve. Invalid values assert via requireValidLayupSide().
  int layupSide();
  bool closed() const;

  std::string getMatOrient1() { return _mat_orient_e1; }
  std::string getMatOrient2() { return _mat_orient_e2; }

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
  std::vector<int> &prevBoundIndices() { return _prev_bound_indices; }
  std::vector<int> &nextBoundIndices() { return _next_bound_indices; }
  BaseOffsetMap &baseOffsetIndicesPairs() { return _base_offset_indices_pairs; }

  PDCELFace *face() { return _face; }
  std::size_t areaCount() const { return _areas.size(); }
  std::size_t layerCount() const;

  // Material orientation selectors consumed during area construction.
  // Typical values are:
  // - e1: "normal" or "baseline"
  // - e2: "baseline" or "layup"
  void setMatOrient1(std::string orient) { _mat_orient_e1 = orient; }
  void setMatOrient2(std::string orient) { _mat_orient_e2 = orient; }

  void setCurveBase(Baseline *c) { _curve_base = c; }
  void setCurveOffset(Baseline *c);

  void addArea(PArea *);

  void setHeadVertexOffset(PDCELVertex *v) { _head_vertex_offset = v; }
  void setTailVertexOffset(PDCELVertex *v) { _tail_vertex_offset = v; }

  void setLevel(int level) { _level = level; }
  void setFreeEnd(int fe) { _free = fe; }
  void setPrevSegment(Segment *seg) { _prev = seg; }
  void setNextSegment(Segment *seg) { _next = seg; }
  void setPrevBound(SVector3 &bound) { _prev_bound = bound; }
  void setNextBound(SVector3 &bound) { _next_bound = bound; }
  void setPrevBound(double x1, double x2, double x3) { _prev_bound = SVector3(x1, x2, x3); }
  void setNextBound(double x1, double x2, double x3) { _next_bound = SVector3(x1, x2, x3); }
  void setPrevBoundVertices(std::vector<PDCELVertex *>);
  void setNextBoundVertices(std::vector<PDCELVertex *>);
  void setPrevBoundIndices(std::vector<int> indices) {
    _prev_bound_indices = indices;
  }
  void setNextBoundIndices(std::vector<int> indices) {
    _next_bound_indices = indices;
  }

  void setPDCELFace(PDCELFace *face) { _face = face; }

  // Build the offset curve once. Repeated calls before build() are no-ops.
  void offsetCurveBase();

  void build(const BuilderConfig &);
  void buildAreas(const BuilderConfig &);

private:
  bool requireBaseDefinition(const char *caller) const;
  int requireValidLayupSide(const char *caller) const;
  bool requireOffsetCurve(const char *caller) const;
  bool requireStateAtLeast(
      LifecycleState minimum_state, const char *caller) const;
  bool requireExactState(
      LifecycleState expected_state, const char *caller) const;
  bool validateStateInvariants(const char *caller) const;
  void releaseOwnedResources();

  // Split the bound edge [vb, vo] parametrically by layup into layer vertices.
  // Splits DCEL edges in place. Returns the new intermediate vertices.
  std::vector<PDCELVertex *> splitBoundByLayup(
      PDCELVertex *vb, PDCELVertex *vo, const BuilderConfig &bcfg);

  // Search face boundary edges from v_prev for the intersection with ls_offset.
  // go_prev controls traversal direction (true = prev(), false = next()).
  // stop_vertex is the offset-bound endpoint that terminates this local
  // traversal (head uses offset.front(), tail uses offset.back()).
  // May split an edge. Returns the intersection vertex, or nullptr if not found.
  PDCELVertex *findLayerIntersectionOnFace(
      PDCELVertex *v_prev, PDCELFace *face,
      PGeoLineSegment *ls_offset, bool go_prev, PDCELVertex *stop_vertex,
      const BuilderConfig &bcfg);

  // Build layer vertices on an open bound by intersecting offset line segments
  // with the face boundary. v_start is the seed vertex; go_prev controls
  // traversal direction; stop_vertex selects the current offset-side boundary
  // endpoint. Returns the new layer vertices.
  std::vector<PDCELVertex *> buildOpenBoundLayerVertices(
      PDCELVertex *v_start, PGeoLineSegment *ls_base,
      bool go_prev, PDCELVertex *stop_vertex, const BuilderConfig &bcfg);

  // Section 1: compute beginning-bound layer vertices.
  // For closed segments, also fills first_bound_vertices.
  std::vector<PDCELVertex *> buildBeginningBound(
      std::vector<PDCELVertex *> &first_bound_vertices,
      const BuilderConfig &bcfg);

  // Section 2: create all intermediate areas and update prev_bound_vertices
  // and count.
  void createIntermediateAreas(
      std::vector<PDCELVertex *> &prev_bound_vertices,
      int &count, const BuilderConfig &bcfg);

  // Section 3: create and append the last (or only) area.
  void buildLastArea(
      const std::vector<PDCELVertex *> &prev_bound_vertices,
      const std::vector<PDCELVertex *> &first_bound_vertices,
      int count, const BuilderConfig &bcfg);
};
