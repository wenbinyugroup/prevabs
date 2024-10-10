#pragma once

// #include "declarations.hpp"
// #include "PDCELHalfEdge.hpp"
// #include "PDCELVertex.hpp"

// #include "gmsh/SVector3.h"

#include <iostream>

// class PDCELHalfEdge;
// class PDCELVertex;
class PGeoPoint3;
class PGeoVector3;

class PGeoPoint2 {
private:
  double _c1, _c2;

public:
  PGeoPoint2() = default;
  PGeoPoint2(double c1, double c2) : _c1(c1), _c2(c2) {}
  PGeoPoint2(const PGeoPoint2 &p) : _c1(p.c1()), _c2(p.c2()) {}
  PGeoPoint2(const PGeoPoint3 &, const int &);
  virtual ~PGeoPoint2() {}

  //
  double c1() const { return _c1; }
  double c2() const { return _c2; }

  double x() const { return _c1; }
  double y() const { return _c2; }

  inline double &operator[](const int &i) {
    if (i == 0) return _c1;
    else if (i == 1) return _c2;
    // TODO: else raise error
  }
  inline double operator[](const int &i) const {
    if (i == 0) return _c1;
    else if (i == 1) return _c2;
    // TODO: else raise error
  }

  //
  friend std::ostream &operator<<(std::ostream &out, const PGeoPoint2 &p) {
    out << "(" << p._c1 << ", " << p._c2 << ")";
    return out;
  }

  //
  PGeoPoint2 &operator=(const PGeoPoint2 &);
};

bool operator<(const PGeoPoint2 &, const PGeoPoint2 &);
bool operator>(const PGeoPoint2 &, const PGeoPoint2 &);
PGeoPoint2 operator+(const PGeoPoint2 &, const PGeoPoint2 &);
PGeoPoint2 operator-(const PGeoPoint2 &, const PGeoPoint2 &);










class PGeoPoint3 {
private:
  double _c1, _c2, _c3;

public:
  PGeoPoint3() = default;
  PGeoPoint3(double c1, double c2, double c3) : _c1(c1), _c2(c2), _c3{c3} {}
  PGeoPoint3(const PGeoPoint3 &p) : _c1(p.c1()), _c2(p.c2()), _c3(p.c3()) {}
  virtual ~PGeoPoint3() {}

  //
  double c1() const { return _c1; }
  double c2() const { return _c2; }
  double c3() const { return _c3; }

  double x() const { return _c1; }
  double y() const { return _c2; }
  double z() const { return _c3; }

  inline double &operator[](const int &i) {
    if (i == 0) return _c1;
    else if (i == 1) return _c2;
    else if (i == 2) return _c3;
    // TODO: else raise error
  }

  inline double operator[](const int &i) const {
    if (i == 0) return _c1;
    else if (i == 1) return _c2;
    else if (i == 2) return _c3;
    // TODO: else raise error
  }

  //
  friend std::ostream &operator<<(std::ostream &out, const PGeoPoint3 &p) {
    out << "(" << p._c1 << ", " << p._c2 << ", " << p._c3 << ")";
    return out;
  }

  //
  void setCoord(double c1, double c2, double c3) {
    _c1 = c1;
    _c2 = c2;
    _c3 = c3;
    return;
  }

  //
  PGeoPoint3 &operator=(const PGeoPoint3 &);
};

bool operator<(const PGeoPoint3 &, const PGeoPoint3 &);
bool operator>(const PGeoPoint3 &, const PGeoPoint3 &);
PGeoPoint3 operator+(const PGeoPoint3 &, const PGeoPoint3 &);
PGeoPoint3 operator+(const PGeoPoint3 &, const PGeoVector3 &);
// PGeoPoint3 operator-(const PGeoPoint3 &, const PGeoPoint3 &);










class PGeoVector3 {
private:
  PGeoPoint3 _p;

public:
  PGeoVector3() : _p() {}
  // PGeoVector3(const PGeoPoint3 &p1, const PGeoPoint3 &p2) : _p(p2 - p1) {}
  PGeoVector3(const PGeoPoint3 &p) : _p(p) {}
  PGeoVector3(double c1, double c2, double c3) : _p(c1, c2, c3) {}
  PGeoVector3(const PGeoVector3 &v) : _p(v._p) {}

  //
  double c1() const { return _p[0]; }
  double c2() const { return _p[1]; }
  double c3() const { return _p[2]; }

  double x() const { return _p[0]; }
  double y() const { return _p[1]; }
  double z() const { return _p[2]; }

  inline double &operator[](const int &i) { return _p[i]; }
  inline double operator[](const int &i) const { return _p[i]; }

  //
  friend std::ostream &operator<<(std::ostream &out, const PGeoVector3 &v) {
    out << v._p;
    return out;
  }

  //
  PGeoVector3 &operator=(const PGeoVector3 &);

  //
  double normSq() const { return _p[0]*_p[0] + _p[1]*_p[1] + _p[2]*_p[2]; }
};

PGeoVector3 operator-(const PGeoPoint3 &, const PGeoPoint3 &);
PGeoVector3 operator*(const double &, const PGeoVector3 &);

// ===================================================================
// class PGeoMatrix2 {
// private:
//   std::vector<double> mr1, mr2;

// public:
//   PGeoMatrix2() {}
//   // Matrix2(double a11, double a12, double a21, double a22)
//   //     : mr1(std::vector<double>(a11, a12)), mr2(std::vector<double>(a21, a22)) {
//   // }
//   PGeoMatrix2(double a11, double a12, double a21, double a22) {
//     mr1.push_back(a11);
//     mr1.push_back(a12);
//     mr2.push_back(a21);
//     mr2.push_back(a22);
//   }

//   std::vector<double> operator[](int) const;
// };

// class PGeoLineSegment {
// private:
//   PDCELVertex *_v1;
//   PDCELVertex *_v2;
//   PDCELHalfEdge *_he12;
//   PDCELHalfEdge *_he21;

// public:
//   PGeoLineSegment()
//       : _v1(nullptr), _v2(nullptr), _he12(nullptr), _he21(nullptr) {}
//   PGeoLineSegment(PDCELVertex *, PDCELVertex *);
//   PGeoLineSegment(PDCELVertex *, SVector3);
//   PGeoLineSegment(PGeoLineSegment *);

//   friend std::ostream &operator<<(std::ostream &, PGeoLineSegment *);

//   void print2();

//   PDCELVertex *v1() { return _v1; }
//   PDCELVertex *v2() { return _v2; }
//   PDCELHalfEdge *he12() { return _he12; }
//   PDCELHalfEdge *he21() { return _he21; }
//   PDCELVertex *vin(); // The left bottom vertex
//   PDCELVertex *vout(); // The right top vertex

//   void setV1(PDCELVertex *v) { _v1 = v; }
//   void setV2(PDCELVertex *v) { _v2 = v; }

//   SVector3 toVector();

//   PDCELVertex *getParametricVertex(double);
//   double getParametricLocation(PDCELVertex *);

//   PDCELHalfEdge *getHalfEdgeWithSource(PDCELVertex *);

//   void setHalfEdge(PDCELHalfEdge *);
// };



// class PGeoPolyline {

// };

// // ===================================================================
// class PGeoArc {
// private:
//   PDCELVertex *_center, *_start, *_end;
//   double _radius2; // radius square
//   double _angle;   // angle in degree
//   int _direction;
//   // std::string amethod; // method of constructing the arc

// public:
//   PGeoArc() {}
//   PGeoArc(PDCELVertex *, PDCELVertex *, int); // Circle
//   PGeoArc(PDCELVertex *, PDCELVertex *, PDCELVertex *, int);
//   PGeoArc(PDCELVertex *, PDCELVertex *, double, int);
//   PGeoArc(PDCELVertex *, PDCELVertex *, PDCELVertex *, double, int);

//   PDCELVertex *center() const { return _center; }
//   PDCELVertex *start() const { return _start; }
//   PDCELVertex *end() const { return _end; }
//   double radiusSquare() const { return _radius2; }
//   double angle() const { return _angle; }
//   int direction() const { return _direction; }
//   // std::string getMethod() const { return amethod; }
// };
