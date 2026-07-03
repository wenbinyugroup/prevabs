#pragma once

#include <list>
#include <vector>

class PGeoLineSegment;

namespace dcel {
class PDCEL;
class PDCELHalfEdge;
class PDCELVertex;

std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
    const PDCEL &dcel, PDCELVertex *v,
    std::vector<PGeoLineSegment *> &temp_segs);

PDCELHalfEdge *findHalfEdgeBelowVertex(const PDCEL &dcel, PDCELVertex *v);
}  // namespace dcel
