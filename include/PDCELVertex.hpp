#pragma once

// Domain fields (_link_to, _p_on_line) have been moved to PDCELVertexData,
// owned by PModel.  PDCELVertex now holds only topology + identity.

#include "declarations.hpp"
#include "PDCELHalfEdge.hpp"

#include "geo_types.hpp"

#include <iostream>
#include <string>

class PDCEL;
class PDCELHalfEdge;

/** @ingroup geo
 * A DCEL vertex class.
 */
class PDCELVertex {
private:
  std::string    _s_name;
  SPoint3        _point;
  bool           _registered = false; // true once added to a DCEL
  PDCELHalfEdge *_incident_edge = nullptr;

public:
  PDCELVertex() = default;
  explicit PDCELVertex(std::string name)
      : _s_name(name) {}
  PDCELVertex(double x, double y, double z)
      : _point(x, y, z) {}
  // build parameter kept for call-site compatibility; use isFinite() instead
  PDCELVertex(double x, double y, double z, bool /*build*/)
      : _point(x, y, z) {}
  explicit PDCELVertex(SPoint3 point)
      : _point(point) {}
  PDCELVertex(std::string name, double x, double y, double z)
      : _s_name(name), _point(x, y, z) {}
  PDCELVertex(std::string name, SPoint3 point)
      : _s_name(name), _point(point) {}

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

  void translate(double, double, double);
  void scale(double);
  void rotate(double);

  bool isFinite();

  bool isRegistered() const { return _registered; }
  PDCELHalfEdge *edge() { return _incident_edge; }
  int degree();
  PDCELHalfEdge *getEdgeTo(PDCELVertex *);

  void setName(const std::string &name) { _s_name = name; }

  void setPosition(double, double, double);
  void setPoint(SPoint3 &);

  void setRegistered(bool r) { _registered = r; }
  void setIncidentEdge(PDCELHalfEdge *);
};

bool compareVertices(PDCELVertex *v1, PDCELVertex *v2);
