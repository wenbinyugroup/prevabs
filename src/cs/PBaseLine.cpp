#include "PBaseLine.hpp"

#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "PDCELVertex.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "geo_types.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

Baseline::Baseline(Baseline *bl) {
  blname = bl->getName();
  bltype = bl->getType();
  for (auto v : bl->vertices()) {
    _pvertices.push_back(v);
  }
}

void Baseline::print() {
  if (config.debug_level < DebugLevel::join) {
    return;
  }

  std::string msg;
  PLOG(debug) << "name: " + blname;
  PLOG(debug) << "type: " + bltype;
  PLOG(debug) << "point: (x, y, z)";
  for (int i = 0; i < _pvertices.size(); i++) {
    msg = std::to_string(i+1) + " [" + _pvertices[i]->name() + "]: " + _pvertices[i]->printString();
    PLOG(debug) << msg;
  }

  return;
}

// void Baseline::printBaseline() {
//   std::cout << doubleLine80 << std::endl;
//   std::cout << std::setw(16) << "BASELINE" << std::setw(16) << blname << std::endl;
//   std::cout << std::setw(16) << "Type" << std::setw(16) << bltype << std::endl;
//   std::cout << std::setw(16) << "Label" << std::setw(16) << "X" << std::setw(16) << "Y" << std::endl;
//   std::cout << singleLine80 << std::endl;
// }

SVector3 Baseline::getTangentVectorBegin() {
  return SVector3(_pvertices[0]->point(), _pvertices[1]->point());
}

SVector3 Baseline::getTangentVectorEnd() {
  return SVector3(
      _pvertices[_pvertices.size() - 2]->point(),
      _pvertices[_pvertices.size() - 1]->point()
    );
}

bool Baseline::findMinimumInteriorAngle(
    double &min_angle_rad, int &vertex_index) const {
  if (_pvertices.size() < 4 || _pvertices.front() != _pvertices.back()) {
    return false;
  }

  const int unique_count = static_cast<int>(_pvertices.size()) - 1;
  bool found = false;
  min_angle_rad = 0.0;
  vertex_index = -1;

  for (int i = 0; i < unique_count; ++i) {
    const int prev_i = (i == 0) ? (unique_count - 1) : (i - 1);
    const int next_i = (i + 1) % unique_count;

    PDCELVertex *prev = _pvertices[prev_i];
    PDCELVertex *curr = _pvertices[i];
    PDCELVertex *next = _pvertices[next_i];
    if (prev == nullptr || curr == nullptr || next == nullptr) {
      continue;
    }

    const SVector3 vin(curr->point(), prev->point());
    const SVector3 vout(curr->point(), next->point());
    if (vin.normSq() <= TOLERANCE * TOLERANCE
        || vout.normSq() <= TOLERANCE * TOLERANCE) {
      continue;
    }

    const double theta = angle(vin, vout);
    if (!found || theta < min_angle_rad) {
      found = true;
      min_angle_rad = theta;
      vertex_index = i;
    }
  }

  return found;
}

int Baseline::topology() {
  if (_pvertices.front() == _pvertices.back()) {
    return 0;
  } else {
    return 1;
  }
}

void Baseline::reverse() {
  std::vector<PDCELVertex *> temp;
  std::size_t n = _pvertices.size();
  for (int i = 0; i < n; ++i) {
    temp.push_back(_pvertices[n - 1 - i]);
  }
  _pvertices.swap(temp);
}

void Baseline::addPVertex(PDCELVertex *vertex) {
  _pvertices.push_back(vertex);
}

void Baseline::insertPVertex(const int &loc, PDCELVertex *vertex) {
  _pvertices.insert(_pvertices.begin()+loc, vertex);
}

void Baseline::join(Baseline *curve, int end, bool reverse) {
  if (reverse) {
    curve->reverse();
  }

  if (end == 1) {
    // Append the curve
    for (auto v : curve->vertices()) {
      if (v == _pvertices.back()) {
        continue;
      }
      _pvertices.push_back(v);
    }
  } else if (end == 0) {
    // Prepend the curve
    std::vector<PDCELVertex *> temp;
    for (auto v : curve->vertices()) {
      temp.push_back(v);
    }
    for (auto v : _pvertices) {
      if (v == temp.back()) {
        continue;
      }
      temp.push_back(v);
    }
    _pvertices.swap(temp);
  }
}
