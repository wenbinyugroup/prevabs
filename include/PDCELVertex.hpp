#pragma once

#include "declarations.hpp"
#include "PBaseLine.hpp"
#include "PDCEL.hpp"
#include "PDCELHalfEdge.hpp"

// #include "gmsh/GVertex.h"
#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SPoint3.h"

#include <iostream>
#include <string>

class PDCEL;
class PDCELHalfEdge;

/** @ingroup geo
 * A DCEL vertex class.
 */
class PDCELVertex {
private:
  std::string _s_name;
  SPoint3 _point;
  PDCEL *_dcel; // Indicating if the vertex has been added to the dcel
  PDCELHalfEdge *_incident_edge;
  int _degenerated;
  PDCELVertex *_link_to = nullptr;
  Baseline *_p_on_line = nullptr;
  bool _on_joint = false;

  bool _gbuild;
  // GVertex *_gvertex;
  int _gvertex_tag = 0;

public:
  PDCELVertex()
      : _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
        _degenerated(0) {}
  PDCELVertex(std::string name)
      : _s_name(name), _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
        _degenerated(0) {}
  PDCELVertex(double x, double y, double z)
      : _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
        _degenerated(0) {}
  PDCELVertex(double x, double y, double z, bool build)
      : _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr),
        _gbuild(build), _degenerated(0) {}
  PDCELVertex(SPoint3 point)
      : _point(point), _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
        _degenerated(0) {}
  PDCELVertex(std::string name, double x, double y, double z)
      : _s_name(name), _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr),
        _gbuild(true), _degenerated(0) {}
  PDCELVertex(std::string name, SPoint3 point)
      : _s_name(name), _point(point), _dcel(nullptr), _incident_edge(nullptr),
        _gbuild(true), _degenerated(0) {}

  // void printBasepoint();
  friend std::ostream &operator<<(std::ostream &, PDCELVertex *);
  void print();
  std::string printString();
  void printWithAddress();
  void printAllLeavingHalfEdges(const int &direction = 1);

  friend bool operator==(PDCELVertex &, PDCELVertex &);
  friend bool operator!=(PDCELVertex &, PDCELVertex &);
  friend bool operator<(PDCELVertex &, PDCELVertex &);
  friend bool operator>(PDCELVertex &, PDCELVertex &);
  friend bool operator<=(PDCELVertex &, PDCELVertex &);
  friend bool operator>=(PDCELVertex &, PDCELVertex &);

  std::string name() const { return _s_name; }
  SPoint3 point() const { return _point; }
  SPoint2 point2() const { return SPoint2(_point[1], _point[2]); }
  inline double x() const { return _point.x(); }
  inline double y() const { return _point.y(); }
  inline double z() const { return _point.z(); }

  PDCELVertex *getLinkToVertex() { return _link_to; }
  Baseline *getOnLine() { return _p_on_line; }

  void translate(double, double, double);
  void scale(double);
  void rotate(double);

  bool isFinite();
  bool onJoint() { return _on_joint; }

  PDCEL *dcel() { return _dcel; }
  PDCELHalfEdge *edge() { return _incident_edge; }
  int degree();
  PDCELHalfEdge *getEdgeTo(PDCELVertex *);

  int &degenerated() { return _degenerated; }

  void setName(const std::string &name) { _s_name = name; }

  void setPosition(double, double, double);
  void setPoint(SPoint3 &);

  void setLinkToVertex(PDCELVertex *v) { _link_to = v; }
  void setOnLine(Baseline *l) { _p_on_line = l; }

  void setDCEL(PDCEL *dcel) { _dcel = dcel; }
  void setIncidentEdge(PDCELHalfEdge *);

  // void setGVertex(GVertex *);
  void setGVertexTag(int tag) { _gvertex_tag = tag; }
  // void resetGVertex() { _gvertex = nullptr; }
  void resetGVertexTag() { _gvertex_tag = 0; }

  void setOnJoint(bool on_joint) { _on_joint = on_joint; }

  bool gbuild() { return _gbuild; }
  void setGBuild(bool build) { _gbuild = build; }
  // GVertex *gvertex() { return _gvertex; }
  int gvertexTag() { return _gvertex_tag; }

  // void setName(std::string name) {_s_name = name; }
};

bool compareVertices(PDCELVertex *v1, PDCELVertex *v2);
