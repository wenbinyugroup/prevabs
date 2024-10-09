#pragma once

// #include "declarations.hpp"
// #include "PDCEL.hpp"
// #include "PDCELHalfEdge.hpp"

// #include "gmsh/GVertex.h"
// #include "gmsh/SPoint2.h"
// #include "gmsh/SPoint3.h"

#include <iostream>
#include <string>

// class PDCEL;
// class PDCELHalfEdge;

template <typename P3> class PDCELVertex {
private:
  std::string _s_name;
  // SPoint3 _point;
  P3 _point;
  // PDCEL *_dcel; // Indicating if the vertex has been added to the dcel
  // PDCELHalfEdge *_incident_edge;
  // GVertex *_gvertex;
  // bool _gbuild;
  // int _degenerated;

public:
  PDCELVertex() = default;
  PDCELVertex(double c1, double c2, double c3) : _point(c1, c2, c3) {}
  PDCELVertex(P3 p) : _point(p) {}
  // PDCELVertex()
  //     : _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
  //       _degenerated(0) {}
  // PDCELVertex(double x, double y, double z)
  //     : _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
  //       _degenerated(0) {}
  // PDCELVertex(double x, double y, double z, bool build)
  //     : _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr),
  //       _gbuild(build), _degenerated(0) {}
  // PDCELVertex(SPoint3 point)
  //     : _point(point), _dcel(nullptr), _incident_edge(nullptr), _gbuild(true),
  //       _degenerated(0) {}
  // PDCELVertex(std::string name, double x, double y, double z)
  //     : _s_name(name), _point(x, y, z), _dcel(nullptr), _incident_edge(nullptr),
  //       _gbuild(true), _degenerated(0) {}
  // PDCELVertex(std::string name, SPoint3 point)
  //     : _s_name(name), _point(point), _dcel(nullptr), _incident_edge(nullptr),
  //       _gbuild(true), _degenerated(0) {}

  std::string name() const { return _s_name; }
  P3 point() const { return _point; }
  // SPoint3 point() { return _point; }
  // SPoint2 point2() { return SPoint2(_point[1], _point[2]); }
  // inline double x() { return _point.x(); }
  // inline double y() { return _point.y(); }
  // inline double z() { return _point.z(); }

  // void printBasepoint();
  // friend std::ostream &operator<<(std::ostream &, PDCELVertex<T> *);
  friend std::ostream &operator<<(std::ostream &out, const PDCELVertex<P3> &v) {
    out << "(" << v._point.x() << ", " << v._point.y() << ", " << v._point.z()
        << ")";
    return out;
  }
  friend std::ostream &operator<<(std::ostream &out, const PDCELVertex<P3> *v) {
    out << "(" << v->_point.x() << ", " << v->_point.y() << ", " << v->_point.z()
        << ")";
    return out;
  }
  void print();
  // std::string printString();
  // void printWithAddress();
  // void printAllLeavingHalfEdges(const int &direction = 1);

  friend bool operator==(PDCELVertex<P3> &, PDCELVertex<P3> &);
  friend bool operator!=(PDCELVertex<P3> &, PDCELVertex<P3> &);
  friend bool operator<(PDCELVertex<P3> &, PDCELVertex<P3> &);
  friend bool operator>(PDCELVertex<P3> &, PDCELVertex<P3> &);
  friend bool operator<=(PDCELVertex<P3> &, PDCELVertex<P3> &);
  friend bool operator>=(PDCELVertex<P3> &, PDCELVertex<P3> &);


  // void translate(double, double, double);
  // void scale(double);
  // void rotate(double);

  // bool isFinite();

  // PDCEL *dcel() { return _dcel; }
  // PDCELHalfEdge *edge() { return _incident_edge; }
  // int degree();
  // PDCELHalfEdge *getEdgeTo(PDCELVertex *);

  // bool gbuild() { return _gbuild; }
  // GVertex *gvertex() { return _gvertex; }

  // int &degenerated() { return _degenerated; }

  // void setPosition(double, double, double);
  // void setPoint(SPoint3 &);

  // void setDCEL(PDCEL *dcel) { _dcel = dcel; }
  // void setIncidentEdge(PDCELHalfEdge *);

  // void setGBuild(bool build) { _gbuild = build; }
  // void setGVertex(GVertex *);
};

// template <typename T>
// std::ostream &operator<<(std::ostream &, const PDCELVertex<T> *);

// template <typename T>
// std::ostream &operator<<(std::ostream &out, PDCELVertex<T> *v) {
//   out << "(" << v->point().x() << ", " << v->point().y() << ", " << v->point().z()
//       << ")";
//   return out;
// }

// bool compareVertices(PDCELVertex *v1, PDCELVertex *v2);
