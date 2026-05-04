#pragma once

#include "PDCELVertex.hpp"
#include "geo_common.hpp"

#include <string>
#include <vector>

double calcPolylineLength(const std::vector<PDCELVertex *> &);

PDCELVertex *findPolylinePointAtParam(
  const std::vector<PDCELVertex *> &,
  const double &, bool &, int &, const double &
);

PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &, const std::string ,
  const double ,   double ,double &,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &, const std::string,
  const double , double ,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

double findPolylineParamByCoordinate(
  const std::vector<PDCELVertex *> &,
  const double ,   double ,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

int trimCurveAtVertex(std::vector<PDCELVertex *> &c, PDCELVertex *v,
                      CurveEnd remove);
