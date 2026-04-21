#pragma once

#include <list>
#include <vector>

class PDCEL;
class PDCELHalfEdge;
class PDCELVertex;
class PGeoLineSegment;

std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
    const PDCEL &dcel, PDCELVertex *v,
    std::vector<PGeoLineSegment *> &temp_segs);

PDCELHalfEdge *findHalfEdgeBelowVertex(const PDCEL &dcel, PDCELVertex *v);
