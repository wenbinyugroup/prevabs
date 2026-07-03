#pragma once

#include "dcel/PDCELVertex.hpp"
#include "geo_common.hpp"

#include <string>
#include <vector>

double calcPolylineLength(const std::vector<dcel::PDCELVertex *> &);

dcel::PDCELVertex *findPolylinePointAtParam(
  const std::vector<dcel::PDCELVertex *> &,
  const double &, bool &, int &, const double &
);

dcel::PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<dcel::PDCELVertex *> &, const std::string ,
  const double ,   double ,double &,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

dcel::PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<dcel::PDCELVertex *> &, const std::string,
  const double , double ,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

double findPolylineParamByCoordinate(
  const std::vector<dcel::PDCELVertex *> &,
  const double ,   double ,
  const int count = 1, const PolylineAxis axis = PolylineAxis::X2
);

int trimCurveAtVertex(std::vector<dcel::PDCELVertex *> &c, dcel::PDCELVertex *v,
                      CurveEnd remove);
