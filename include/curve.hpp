#pragma once

#include "PBaseLine.hpp"
#include "dcel/PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo_common.hpp"

#include <list>
#include <vector>

Baseline *joinCurves(std::list<Baseline *>);
int joinCurves(Baseline *, std::list<Baseline *>);

int adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, CurveEnd end);

int mergeSortedVertexLists(const std::vector<dcel::PDCELVertex *> &,
                           const std::vector<dcel::PDCELVertex *> &,
                           std::vector<int> &, std::vector<int> &,
                           std::vector<dcel::PDCELVertex *> &);
