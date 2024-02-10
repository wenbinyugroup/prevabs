#include "overloadOperator.hpp"

#include "Material.hpp"
#include "PDCELVertex.hpp"
#include "PSegment.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"

#include "gmsh/SPoint2.h"
#include "gmsh/SPoint3.h"
#include "gmsh/SVector3.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

// template <typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<double> &v) {
  for (auto n : v) {
    out << n << " ";
  }
  return out;
}

// ===================================================================
//                                                     Overloading I/O
// std::ostream &operator<<(std::ostream &out, PDCELVertex *v) {
//   out << "(" << v->x() << ", " << v->y() << ", " << v->z() << ")";
//   return out;
// }

std::ostream &operator<<(std::ostream &out, const SPoint2 &p) {
  out << "(" << p.x() << ", " << p.y() << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const SPoint3 &p) {
  out << "(" << p.x() << ", " << p.y() << ", " << p.z() << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const SVector3 &v) {
  out << "(" << v.x() << ", " << v.y() << ", " << v.z() << ")";
  return out;
}

// std::ostream &operator<<(std::ostream &out, const Point2 &p) {
//   if (scientific_format) {
//     out << std::scientific;
//   }
//   out << std::setw(16) << p.x() << std::setw(16) << p.y();
//   // out << "(" << p.x() << ", " << p.y() << ")";
//   return out;
// }

// std::ostream &operator<<(std::ostream &out, const Vector2 &v) {
//   out << "(" << v.x() << ", " << v.y() << ")";
//   return out;
// }

// std::ostream &operator<<(std::ostream &out, Basepoint &bp) {
//   if (scientific_format) {
//     out << std::scientific;
//   }
//   out << std::setw(16) << bp.getName() << bp.getCoordinate();
//   // out << bp.getName() << ": " << bp.getCoordinate();
//   return out;
// }

// std::ostream &operator<<(std::ostream &out, Basepoint *p_bp) {
//   if (scientific_format) {
//     out << std::scientific;
//   }
//   out << p_bp->getName() << ": " << p_bp->getCoordinate();
//   return out;
// }

std::ostream &operator<<(std::ostream &out, LayerType &lt) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << lt.id() << std::setw(16) << lt.material()->id()
      << std::setw(16) << lt.angle() << std::setw(16)
      << lt.material()->getName();
  return out;
}

// std::ostream &operator<<(std::ostream &out, LayerType *lt) {
//   out << std::scientific;
//   out << std::setw(16) << lt->id()
//       << std::setw(16) << lt->material()->id()
//       << std::setw(16) << lt->angle()
//       << std::setw(16) << lt->material()->getName();
//   return out;
// }

std::ostream &operator<<(std::ostream &out, Layer &layer) {
  out << std::setw(32) << layer.getLamina()->getMaterial()->getName()
      << std::setw(16) << layer.getLamina()->getThickness() * layer.getStack()
      << std::setw(8) << layer.getAngle() << std::setw(8) << layer.getStack();
  return out;
}

std::ostream &operator<<(std::ostream &out, Ply &ply) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << ply.getMaterial()->getName() << std::setw(16)
      << ply.getThickness() << std::setw(16) << ply.getAngle();
  return out;
}

std::ostream &operator<<(std::ostream &out, Segment &s) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << s.getName() << std::setw(16)
      << s.getBaseline()->getName() << std::setw(32) << s.getLayup()->getName()
      << std::setw(16) << s.getLayupside() << std::setw(8) << s.getLevel();
  return out;
}

// std::ostream &operator<<(std::ostream &out, Connection &c) {
//   out << std::setw(16) << c.getName()
//       << std::setw(16) << c.getType()
//       << std::setw(16) << c.getSegments()[0]
//       << std::setw(16) << c.getSegments()[1];
//   return out;
// }

std::ostream &operator<<(std::ostream &out, Filling &f) {
  out << std::setw(16) << f.getName() << std::setw(16)
      << f.getBaseline()->getName() << std::setw(16)
      << f.getMaterial()->getName() << std::setw(16) << f.getFillSide();
  return out;
}










// ===================================================================
//                                              Overloading Comparison
// bool operator==(const Point2 &p1, const Point2 &p2) {
//   return (std::abs(p1.x() - p2.x()) <= TOLERANCE &&
//           std::abs(p1.y() - p2.y()) <= TOLERANCE);
// }

// bool operator!=(const Point2 &p1, const Point2 &p2) { return (p1 == p2); }

// bool operator==(const Vector2 &v1, const Vector2 &v2) {
//   return (std::abs(v1.x() - v2.x()) <= TOLERANCE &&
//           std::abs(v1.y() - v2.y()) <= TOLERANCE);
// }

// bool operator!=(const Vector2 &v1, const Vector2 &v2) { return (v1 == v2); }

bool operator==(const Layup &l1, const Layup &l2) {
  std::vector<Layer> l1_layers = l1.getLayers();
  std::vector<Layer> l2_layers = l2.getLayers();

  if (l1_layers.size() != l2_layers.size()) return false;

  for (auto i = 0; i < l1_layers.size(); i++) {
    if (
      l1_layers[i].getLamina()->getName() != l2_layers[i].getLamina()->getName() ||
      l1_layers[i].getAngle() != l2_layers[i].getAngle() ||
      l1_layers[i].getStack() != l2_layers[i].getStack()
    ) return false;
  }

  return true;
}


// bool operator==(const Layup *l1, const Layup *l2) {
//   std::vector<Layer> l1_layers = l1->getLayers();
//   std::vector<Layer> l2_layers = l2->getLayers();

//   if (l1_layers.size() != l2_layers.size()) return false;

//   for (auto i = 0; i < l1_layers.size(); i++) {
//     if (
//       l1_layers[i].getLamina()->getName() != l2_layers[i].getLamina()->getName() ||
//       l1_layers[i].getAngle() != l2_layers[i].getAngle() ||
//       l1_layers[i].getStack() != l2_layers[i].getStack()
//     ) return false;
//   }

//   return true;
// }









// ===================================================================
//                                              Overloading Arithmetic
// Vector2 operator-(const Point2 &e, const Point2 &s) { return Vector2{s, e}; }

// Point2 operator+(const Point2 &s, const Vector2 &v) {
//   return Point2{s.x() + v.x(), s.y() + v.y()};
// }

// Vector2 operator*(const Matrix2 &m, const Vector2 &v) {
//   double v1, v2;
//   v1 = m[0][0] * v[0] + m[0][1] * v[1];
//   v2 = m[1][0] * v[0] + m[1][1] * v[1];
//   return Vector2{v1, v2};
// }
