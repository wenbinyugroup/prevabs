#include "PDCELVertex.hpp"

#include "PDCELHalfEdge.hpp"
#include "globalConstants.hpp"
#include "utilities.hpp"

#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/STensor3.h"
#include "gmsh_mod/SVector3.h"
// #include "gmsh/GVertex.h"

#include <sstream>
#include <iostream>

std::string PDCELVertex::printString() {
  std::stringstream ss;
  ss << "( " << _point.x() << " , " << _point.y() << " , " << _point.z()
      << " )";
  return ss.str();
}




std::ostream &operator<<(std::ostream &out, PDCELVertex *v) {
  out << v->printString();
  return out;
}




void PDCELVertex::print() {
  std::cout << printString();
}




void PDCELVertex::printWithAddress() {
  printf("( %f , %f , %f ) | address: %p\n", _point[0], _point[1], _point[2], (void *)this);
}

bool operator==(PDCELVertex &v1, PDCELVertex &v2) {
  return (v1._point.x() == v2._point.x() && v1._point.y() == v2._point.y() &&
          v1._point.z() == v2._point.z());
}

bool operator!=(PDCELVertex &v1, PDCELVertex &v2) { return !(v1 == v2); }

bool operator<(PDCELVertex &v1, PDCELVertex &v2) {
  return (v1._point < v2._point);
}

bool operator>(PDCELVertex &v1, PDCELVertex &v2) {
  return (v2._point < v1._point);
}

bool operator<=(PDCELVertex &v1, PDCELVertex &v2) {
  return !(v1._point > v2._point);
}

bool operator>=(PDCELVertex &v1, PDCELVertex &v2) {
  return !(v1._point < v2._point);
}

void PDCELVertex::printAllLeavingHalfEdges(const int &direction) {
  PDCELHalfEdge *he = _incident_edge;
  do {
    if (he == nullptr) {
      break;
    }
    he->print2();
    if (direction > 0) {
      // counter-clockwise
      if (he->prev() != nullptr) {
        he = he->prev()->twin();
      } else {
        break;
      }
    } else if (direction < 0) {
      // clockwise
      he = he->twin()->next();
    }
  } while (he != _incident_edge);
  std::cout << "stop" << std::endl;
}

int PDCELVertex::degree() {
  PDCELHalfEdge *he = _incident_edge;
  int count = 0;
  while (he != nullptr) {
    count++;
    if (he->twin() == nullptr) {
      break;
    }
    he = he->twin()->next();
    if (he == _incident_edge) {
      break;
    }
  }

  return count;
}

void PDCELVertex::translate(double dx, double dy, double dz) {
  double newx = _point.x() + dx;
  double newy = _point.y() + dy;
  double newz = _point.z() + dz;

  _point = SPoint3(newx, newy, newz);
}

void PDCELVertex::scale(double s) {
  _point *= s;
}

void PDCELVertex::rotate(double a) {
  SVector3 cp{_point}, np;
  STensor3 rm = getRotationMatrix(a, 1);

  np = rm * cp;

  _point = np.point();
}

bool PDCELVertex::isFinite() {
  return (_point[0] > -INF && _point[0] < INF && _point[1] > -INF &&
          _point[1] < INF && _point[2] > -INF && _point[2] < INF);
}

PDCELHalfEdge *PDCELVertex::getEdgeTo(PDCELVertex *v) {
  if (_incident_edge != nullptr) {
    if (_incident_edge->twin()->source() == v) {
      return _incident_edge;
    } else {
      PDCELHalfEdge *he = _incident_edge->twin()->next();
      while (he != _incident_edge) {
        if (he->twin()->source() == v) {
          return he;
        } else {
          he = he->twin()->next();
        }
      }
    }
  }
  return nullptr;
}

void PDCELVertex::setPosition(double x, double y, double z) {
  _point.setPosition(x, y, z);
}

void PDCELVertex::setPoint(SPoint3 &p) { _point = p; }

void PDCELVertex::setIncidentEdge(PDCELHalfEdge *he) { _incident_edge = he; }

// void PDCELVertex::setGVertex(GVertex *gv) { _gvertex = gv; }
// void PDCELVertex::setGVertexTag(int tag) { _gvertex_tag = tag; }

bool compareVertices(PDCELVertex *v1, PDCELVertex *v2) {
  return v1->point() < v2->point();
}
