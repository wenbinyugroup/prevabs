#pragma once

// geo_types.hpp
// Single consolidated header replacing include/gmsh_mod/{SPoint2,SPoint3,SVector3,STensor3}.h
//
// SVector3 vector math (dot, cross, length, normalize) is delegated to linalg.h v2.2.
// STensor3 matrix math (inverse, determinant, transpose, mat-vec multiply) likewise.
// SPoint2 and SPoint3 are plain C++ structs with no library dependency.

#include "linalg.h"
#include <cmath>
#include <cstdlib>

// ============================================================
// SPoint2 — 2-D point
// ============================================================
struct SPoint2 {
  double P[2];

  SPoint2() : P{0., 0.} {}
  SPoint2(double x, double y) : P{x, y} {}
  SPoint2(const double* p) : P{p[0], p[1]} {}
  SPoint2(const SPoint2&) = default;
  SPoint2& operator=(const SPoint2&) = default;

  double  x() const { return P[0]; }
  double  y() const { return P[1]; }

  double& operator[](int i)       { return P[i]; }
  double  operator[](int i) const { return P[i]; }

  double distance(const SPoint2& o) const {
    double dx = P[0] - o.P[0], dy = P[1] - o.P[1];
    return std::sqrt(dx * dx + dy * dy);
  }

  SPoint2& operator+=(const SPoint2& o) { P[0] += o.P[0]; P[1] += o.P[1]; return *this; }
  SPoint2& operator-=(const SPoint2& o) { P[0] -= o.P[0]; P[1] -= o.P[1]; return *this; }
  SPoint2& operator*=(double s)         { P[0] *= s; P[1] *= s; return *this; }
  SPoint2  operator*(double s) const    { return SPoint2(P[0] * s, P[1] * s); }

  bool operator<(const SPoint2& o) const {
    if (P[0] != o.P[0]) return P[0] < o.P[0];
    return P[1] < o.P[1];
  }
};

inline SPoint2 operator+(const SPoint2& a, const SPoint2& b) {
  return SPoint2(a.P[0] + b.P[0], a.P[1] + b.P[1]);
}
inline SPoint2 operator-(const SPoint2& a, const SPoint2& b) {
  return SPoint2(a.P[0] - b.P[0], a.P[1] - b.P[1]);
}

// ============================================================
// SPoint3 — 3-D point
// ============================================================
struct SPoint3 {
  double P[3];

  SPoint3() : P{0., 0., 0.} {}
  SPoint3(double x, double y, double z) : P{x, y, z} {}
  SPoint3(const double* p) : P{p[0], p[1], p[2]} {}
  SPoint3(const SPoint3&) = default;
  SPoint3& operator=(const SPoint3&) = default;

  double  x() const { return P[0]; }
  double  y() const { return P[1]; }
  double  z() const { return P[2]; }

  double& operator[](int i)       { return P[i]; }
  double  operator[](int i) const { return P[i]; }

  const double* data() const { return P; }

  void setPosition(double x, double y, double z) { P[0] = x; P[1] = y; P[2] = z; }

  double distance(const SPoint3& o) const {
    double dx = P[0]-o.P[0], dy = P[1]-o.P[1], dz = P[2]-o.P[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
  }

  SPoint3& operator+=(const SPoint3& o) { P[0]+=o.P[0]; P[1]+=o.P[1]; P[2]+=o.P[2]; return *this; }
  SPoint3& operator-=(const SPoint3& o) { P[0]-=o.P[0]; P[1]-=o.P[1]; P[2]-=o.P[2]; return *this; }
  SPoint3& operator*=(double s) { P[0]*=s; P[1]*=s; P[2]*=s; return *this; }
  SPoint3& operator/=(double s) { P[0]/=s; P[1]/=s; P[2]/=s; return *this; }
  SPoint3  operator*(double s) const { return SPoint3(P[0]*s, P[1]*s, P[2]*s); }

  bool operator==(const SPoint3& o) const { return P[0]==o.P[0] && P[1]==o.P[1] && P[2]==o.P[2]; }
  bool operator!=(const SPoint3& o) const { return !(*this == o); }
  bool operator<(const SPoint3& o) const {
    if (P[0] != o.P[0]) return P[0] < o.P[0];
    if (P[1] != o.P[1]) return P[1] < o.P[1];
    return P[2] < o.P[2];
  }
  bool operator>(const SPoint3& o)  const { return o < *this; }
  bool operator<=(const SPoint3& o) const { return !(o < *this); }
  bool operator>=(const SPoint3& o) const { return !(*this < o); }
};

inline SPoint3 operator+(const SPoint3& a, const SPoint3& b) {
  return SPoint3(a.P[0]+b.P[0], a.P[1]+b.P[1], a.P[2]+b.P[2]);
}
inline SPoint3 operator-(const SPoint3& a, const SPoint3& b) {
  return SPoint3(a.P[0]-b.P[0], a.P[1]-b.P[1], a.P[2]-b.P[2]);
}
inline SPoint3 operator*(double s, const SPoint3& p) {
  return SPoint3(s*p.P[0], s*p.P[1], s*p.P[2]);
}

// ============================================================
// SVector3 — 3-D vector backed by linalg::vec<double,3>
// ============================================================
struct SVector3 {
  linalg::vec<double, 3> _v;

  SVector3() : _v{0., 0., 0.} {}
  SVector3(double x, double y, double z) : _v{x, y, z} {}
  // Uniform-value constructor: SVector3(s) → (s, s, s)
  explicit SVector3(double s) : _v{s, s, s} {}
  SVector3(const double* a) : _v{a[0], a[1], a[2]} {}
  // Construct from 3-D point (origin → p)
  SVector3(const SPoint3& p) : _v{p.P[0], p.P[1], p.P[2]} {}
  // Construct from two 3-D points (vector p1 → p2)
  SVector3(const SPoint3& p1, const SPoint3& p2)
    : _v{p2.P[0]-p1.P[0], p2.P[1]-p1.P[1], p2.P[2]-p1.P[2]} {}
  SVector3(const SVector3&) = default;
  SVector3& operator=(const SVector3&) = default;
  SVector3& operator=(double s) { _v.x = _v.y = _v.z = s; return *this; }

  // Coordinate access
  double  x() const { return _v.x; }
  double  y() const { return _v.y; }
  double  z() const { return _v.z; }

  double& operator[](int i)       { return (&_v.x)[i]; }
  double  operator[](int i) const { return (&_v.x)[i]; }
  double& operator()(int i)       { return (&_v.x)[i]; }
  double  operator()(int i) const { return (&_v.x)[i]; }

  double*       data()       { return &_v.x; }
  const double* data() const { return &_v.x; }

  // Convert back to a 3-D point
  SPoint3 point() const { return SPoint3(_v.x, _v.y, _v.z); }

  // Magnitude
  double norm()   const { return linalg::length(_v); }
  double normSq() const { return linalg::length2(_v); }

  // Normalize in-place; returns original magnitude.
  double normalize() {
    double n = linalg::length(_v);
    if (n > 0.) _v = _v * (1.0 / n);
    return n;
  }

  // Return a normalized copy; returns zero vector if already zero.
  SVector3 unit() const {
    double n = linalg::length(_v);
    return n > 0. ? SVector3(_v * (1.0 / n)) : SVector3(0., 0., 0.);
  }

  void negate() { _v.x = -_v.x; _v.y = -_v.y; _v.z = -_v.z; }

  // this += a * y
  void axpy(double a, const SVector3& y) {
    _v.x += a * y._v.x;
    _v.y += a * y._v.y;
    _v.z += a * y._v.z;
  }

  // Returns index of largest-magnitude component; sets val to that magnitude.
  int getMaxValue(double& val) const {
    int idx = 0;
    val = std::abs(_v.x);
    if (std::abs(_v.y) > val) { val = std::abs(_v.y); idx = 1; }
    if (std::abs(_v.z) > val) { val = std::abs(_v.z); idx = 2; }
    return idx;
  }

  SVector3& operator+=(const SVector3& o) { _v.x+=o._v.x; _v.y+=o._v.y; _v.z+=o._v.z; return *this; }
  SVector3& operator-=(const SVector3& o) { _v.x-=o._v.x; _v.y-=o._v.y; _v.z-=o._v.z; return *this; }
  SVector3& operator*=(double s)          { _v.x*=s; _v.y*=s; _v.z*=s; return *this; }
  SVector3& operator*=(const SVector3& o) { _v.x*=o._v.x; _v.y*=o._v.y; _v.z*=o._v.z; return *this; }

  // Internal: construct from raw linalg vec (used by free functions below)
  explicit SVector3(linalg::vec<double, 3> v) : _v(v) {}
};

// --- SVector3 free functions ---

inline double   dot     (const SVector3& a, const SVector3& b) { return linalg::dot(a._v, b._v); }
inline double   norm    (const SVector3& v)                    { return linalg::length(v._v); }
inline double   normSq  (const SVector3& v)                    { return linalg::length2(v._v); }
inline SVector3 crossprod(const SVector3& a, const SVector3& b){ return SVector3(linalg::cross(a._v, b._v)); }

inline double angle(const SVector3& a, const SVector3& b) {
  double cp = linalg::length(linalg::cross(a._v, b._v));
  double dp = linalg::dot(a._v, b._v);
  return std::atan2(cp, dp);
}

inline SVector3 operator+(const SVector3& a, const SVector3& b) { return SVector3(a._v.x+b._v.x, a._v.y+b._v.y, a._v.z+b._v.z); }
inline SVector3 operator-(const SVector3& a, const SVector3& b) { return SVector3(a._v.x-b._v.x, a._v.y-b._v.y, a._v.z-b._v.z); }
inline SVector3 operator-(const SVector3& a)                    { return SVector3(-a._v.x, -a._v.y, -a._v.z); }
inline SVector3 operator*(double s, const SVector3& v)          { return SVector3(s*v._v.x, s*v._v.y, s*v._v.z); }
inline SVector3 operator*(const SVector3& v, double s)          { return SVector3(v._v.x*s, v._v.y*s, v._v.z*s); }
// Component-wise vector multiply
inline SVector3 operator*(const SVector3& a, const SVector3& b) { return SVector3(a._v.x*b._v.x, a._v.y*b._v.y, a._v.z*b._v.z); }

// ============================================================
// STensor3 — general 3×3 matrix backed by linalg::mat<double,3,3>
//
// linalg stores matrices in column-major order:
//   _m.x = column 0, _m.y = column 1, _m.z = column 2
//   Element (row i, col j) = _m[j][i]
//
// Named accessors use 1-based indices (m_ij = row i, col j).
// operator()(i,j) uses 0-based indices.
// ============================================================
struct STensor3 {
  linalg::mat<double, 3, 3> _m;  // zero-initialized by default

  // Default: zero matrix.  STensor3(v): diagonal = v, off-diagonal = 0.
  STensor3(double v = 0.0) : _m{} {
    if (v != 0.0) { _m.x.x = _m.y.y = _m.z.z = v; }
  }
  STensor3(const STensor3&) = default;
  STensor3& operator=(const STensor3&) = default;

  // 0-based element access: operator()(row, col)
  double& operator()(int i, int j)       { return _m[j][i]; }
  double  operator()(int i, int j) const { return _m[j][i]; }

  // Linear row-major access: t[k] = element at row k/3, col k%3
  double& operator[](int k)       { return _m[k%3][k/3]; }
  double  operator[](int k) const { return _m[k%3][k/3]; }

  // 1-based named setters / getters
  void set_m11(double v) { _m.x.x = v; }  double get_m11() const { return _m.x.x; }
  void set_m12(double v) { _m.y.x = v; }  double get_m12() const { return _m.y.x; }
  void set_m13(double v) { _m.z.x = v; }  double get_m13() const { return _m.z.x; }
  void set_m21(double v) { _m.x.y = v; }  double get_m21() const { return _m.x.y; }
  void set_m22(double v) { _m.y.y = v; }  double get_m22() const { return _m.y.y; }
  void set_m23(double v) { _m.z.y = v; }  double get_m23() const { return _m.z.y; }
  void set_m31(double v) { _m.x.z = v; }  double get_m31() const { return _m.x.z; }
  void set_m32(double v) { _m.y.z = v; }  double get_m32() const { return _m.y.z; }
  void set_m33(double v) { _m.z.z = v; }  double get_m33() const { return _m.z.z; }

  // Matrix operations (return new matrix)
  STensor3 invert() const {
    STensor3 r;
    r._m = linalg::inverse(_m);
    return r;
  }
  STensor3 transpose() const {
    STensor3 r;
    r._m = linalg::transpose(_m);
    return r;
  }
  double determinant() const { return linalg::determinant(_m); }

  // Arithmetic (element-wise)
  STensor3& operator+=(const STensor3& o) { _m.x+=o._m.x; _m.y+=o._m.y; _m.z+=o._m.z; return *this; }
  STensor3& operator-=(const STensor3& o) { _m.x-=o._m.x; _m.y-=o._m.y; _m.z-=o._m.z; return *this; }
  STensor3& operator*=(double s) {
    for (int c = 0; c < 3; ++c) _m[c].x *= s, _m[c].y *= s, _m[c].z *= s;
    return *this;
  }
  // Element-wise (Hadamard) multiply — matches original STensor3 behaviour
  STensor3& operator*=(const STensor3& o) {
    for (int c = 0; c < 3; ++c) {
      _m[c].x *= o._m[c].x;
      _m[c].y *= o._m[c].y;
      _m[c].z *= o._m[c].z;
    }
    return *this;
  }
};

// --- STensor3 free functions ---

// Matrix × vector (standard matrix-vector product)
inline SVector3 operator*(const STensor3& t, const SVector3& v) {
  linalg::vec<double, 3> r = linalg::mul(t._m, v._v);
  return SVector3(r.x, r.y, r.z);
}
// Scalar multiply
inline STensor3 operator*(const STensor3& t, double s) {
  STensor3 r(t);
  r *= s;
  return r;
}
inline STensor3 operator*(double s, const STensor3& t) { return t * s; }
