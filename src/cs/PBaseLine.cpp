#include "PBaseLine.hpp"

#include "globalConstants.hpp"
#include "overloadOperator.hpp"
#include "PDCELVertex.hpp"
#include "utilities.hpp"
#include "plog.hpp"
#include "gmsh_mod/SVector3.h"

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





void Baseline::print(Message *pmessage) {
  pmessage->increaseIndent();

  std::string msg;
  // pmessage->print(i_type, "name: " + blname);
  PLOG(debug) << pmessage->message("name: " + blname);
  // pmessage->print(i_type, "type: " + bltype);
  PLOG(debug) << pmessage->message("type: " + bltype);
  // pmessage->print(i_type, "point: (x, y, z)");
  PLOG(debug) << pmessage->message("point: (x, y, z)");
  for (int i = 0; i < _pvertices.size(); i++) {
    msg = std::to_string(i+1) + " [" + _pvertices[i]->name() + "]: " + _pvertices[i]->printString();
    // pmessage->print(i_type, msg);
    PLOG(debug) << pmessage->message(msg);
  }

  pmessage->decreaseIndent();

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

