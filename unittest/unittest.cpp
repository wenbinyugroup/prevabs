#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

std::vector<PGeoPoint3> readLinePoint2(std::stringstream &ss) {
  double num;
  std::vector<double> numbers;
  while (ss >> num) {
    numbers.push_back(num);
  }
  int np = numbers.size() / 2;  // number of points
  std::vector<PGeoPoint3> ps;
  for (auto i = 0; i < np; ++i) {
    PGeoPoint3 p{0, numbers[i*2], numbers[i*2+1]};
    ps.push_back(p);
  }
  return ps;
}

void testLineSegIntersect(std::stringstream &ss, const double &tol) {
  double l1p1x, l1p1y, l1p2x, l1p2y, l2p1x, l2p1y, l2p2x, l2p2y, rpx, rpy;
  double u1, u2;
  bool risintersect;
  ss >> l1p1x >> l1p1y >> l1p2x >> l1p2y >> l2p1x >> l2p1y >> l2p2x >> l2p2y >> risintersect;
  if (risintersect == 1) {
    ss >> rpx >> rpy;
  }
  PGeoPoint3 pr{0, rpx, rpy};
  PGeoPoint3 l1p1{0, l1p1x, l1p1y};
  PGeoPoint3 l1p2{0, l1p2x, l1p2y};
  PGeoPoint3 l2p1{0, l2p1x, l2p1y};
  PGeoPoint3 l2p2{0, l2p2x, l2p2y};
  std::cout << l1p1 << "-" << l1p2;
  std::cout << " x " << l2p1 << "-" << l2p2;


  bool is_intersect = calcLineIntersection2D(
    l1p1, l1p2, l2p1, l2p2, u1, u2, 0, tol
  );


  if (is_intersect != risintersect) {
    std::cout << " - fail" << std::endl;
    return;
  }
  if (is_intersect) {
    PGeoPoint3 pn = calcPointFromParam(l1p1, l1p2, u1, tol);
    // std::cout << "new point: " << pn << std::endl;
    double diff;
    diff = (pn - pr).normSq();
    // std::cout << "diff: " << diff << std::endl;
    if (fabs(diff) < tol) {
      std::cout << " - success" << std::endl;
      return;
    }
    else {
      std::cout << " - fail" << std::endl;
      return;
    }
  }
  else {
    std::cout << " - success" << std::endl;
    return;
  }
}





void testCalcPolylineLength(std::stringstream &ss, const double &tol) {
  double num;
  std::vector<double> numbers;
  while (ss >> num) {
    numbers.push_back(num);
  }
  int np = (numbers.size() - 1) / 2;  // number of points
  std::vector<PGeoPoint3> ps;
  for (auto i = 0; i < np; ++i) {
    PGeoPoint3 p{0, numbers[i*2], numbers[i*2+1]};
    ps.push_back(p);
    if (i > 0) std::cout << "-";
    std::cout << p;
  }
  double result_len = numbers[numbers.size() - 1];
  double calc_len = calcPolylineLength(ps);
  std::cout << " len = " << calc_len;
  double err = result_len - calc_len;
  if (fabs(err) < tol) {
    std::cout << " - success" << std::endl;
  }
  else {
    std::cout << " - fail" << std::endl;
  }
}










int main() {
  double tol = 1e-9;

  std::cout << "Unit tests" << std::endl;

  // Read polylines
  std::vector< std::vector<PGeoPoint3> > pls;
  std::ifstream ifs_pls{"test_data_polylines.dat"};
  if (ifs_pls.is_open()) {
    std::string sline;
    while (getline(ifs_pls, sline)) {
      std::stringstream ss(sline);
      std::vector<PGeoPoint3> ps = readLinePoint2(ss);
      pls.push_back(ps);
    }
  }

  // Do tests
  std::cout << "\n";
  std::string line;
  // std::ifstream ifs{"test_data_line_seg_intersect.dat"};
  // std::ifstream ifs{"test_data_calc_polyline_length.dat"};
  std::ifstream ifs{"test_data_find_point_from_param.dat"};

  if (ifs.is_open()) {
    while (getline(ifs, line)) {
      std::stringstream ss(line);
      // testLineSegIntersect(ss, tol);
      // testCalcPolylineLength(ss, tol);
      int li;
      double u;
      ss >> li >> u;
      // std::vector<PGeoPoint3> pl = pls[i - 1];
      PGeoPoint3 p;
      p = findParamPointOnPolyline(pls[li-1], u, tol);
      std::cout << p << std::endl;
    }
    ifs.close();
  }



  return 0;
}
