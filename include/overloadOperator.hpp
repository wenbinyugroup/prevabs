#pragma once

#include "Material.hpp"
#include "PDCELVertex.hpp"
#include "PSegment.hpp"
#include "PBaseLine.hpp"

#include "gmsh_mod/SPoint2.h"
#include "gmsh_mod/SPoint3.h"
#include "gmsh_mod/SVector3.h"

#include <iostream>

// template <typename T>
std::ostream &operator<<(std::ostream &, const std::vector<double> &);

// ===================================================================
//                                                     Overloading I/O
// std::ostream &operator<<(std::ostream &, PDCELVertex *);
std::ostream &operator<<(std::ostream &, const SPoint2 &);
std::ostream &operator<<(std::ostream &, const SPoint3 &);
std::ostream &operator<<(std::ostream &, const SVector3 &);

// std::ostream &operator<<(std::ostream &, const Point2 &);
// std::ostream &operator<<(std::ostream &, const Vector2 &);
// std::ostream &operator<<(std::ostream &, Basepoint &);
// std::ostream &operator<<(std::ostream &, Basepoint *);
std::ostream &operator<<(std::ostream &, LayerType &);
// std::ostream &operator<<(std::ostream &, LayerType *);
std::ostream &operator<<(std::ostream &, Layer &);
std::ostream &operator<<(std::ostream &, Ply &);
std::ostream &operator<<(std::ostream &, Segment &);
// std::ostream &operator<<(std::ostream &, Connection &);
std::ostream &operator<<(std::ostream &, Filling &);

// ===================================================================
//                                              Overloading Comparison
// bool operator==(const Point2 &, const Point2 &);
// bool operator!=(const Point2 &, const Point2 &);
// bool operator==(const Vector2 &, const Vector2 &);
// bool operator!=(const Vector2 &, const Vector2 &);
bool operator==(const Layup &, const Layup &);
// bool operator==(const Layup *, const Layup *);

// ===================================================================
//                                              Overloading Arithmetic
// Vector2 operator-(const Point2 &, const Point2 &);
// Point2 operator+(const Point2 &, const Vector2 &);
// Vector2 operator*(const Matrix2 &, const Vector2 &);
