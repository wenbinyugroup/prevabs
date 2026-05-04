#include "DCELSweepLine.hpp"

#include "PDCEL.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "PGeoClasses.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"

#include <cmath>
#include <map>

std::list<PGeoLineSegment *> findLineSegmentsAtSweepLine(
    const PDCEL &dcel, PDCELVertex *v,
    std::vector<PGeoLineSegment *> &temp_segs) {
  std::list<PGeoLineSegment *> ls_list;
  std::map<PDCELHalfEdge *, PGeoLineSegment *> temp_map;

  for (PDCELVertex *vi : dcel._vertex_tree) {
    if (vi == v) {
      break;
    }

    PDCELHalfEdge *he = vi->edge();
    do {
      if (vi->point() < he->target()->point()) {
        PGeoLineSegment *ls = he->lineSegment();
        if (ls == nullptr) {
          std::map<PDCELHalfEdge *, PGeoLineSegment *>::const_iterator it =
              temp_map.find(he);
          if (it != temp_map.end()) {
            ls = it->second;
          } else {
            ls = he->toLineSegment();
            temp_map[he] = ls;
            temp_map[he->twin()] = ls;
            temp_segs.push_back(ls);
          }
        }
        ls_list.push_back(ls);
      } else {
        PGeoLineSegment *ls = he->lineSegment();
        if (ls == nullptr) {
          std::map<PDCELHalfEdge *, PGeoLineSegment *>::const_iterator it =
              temp_map.find(he);
          if (it != temp_map.end()) {
            ls = it->second;
          }
        }
        if (ls != nullptr) {
          ls_list.remove(ls);
        }
      }
      he = he->twin()->next();
    } while (he != nullptr && he != vi->edge());
  }

  return ls_list;
}

PDCELHalfEdge *findHalfEdgeBelowVertex(const PDCEL &dcel, PDCELVertex *v) {
  std::vector<PGeoLineSegment *> temp_segs;
  std::list<PGeoLineSegment *> ls_list =
      findLineSegmentsAtSweepLine(dcel, v, temp_segs);

  PDCELVertex *vt = new PDCELVertex(v->x(), v->y(), v->z() + 1);
  PGeoLineSegment *ls_tmp = new PGeoLineSegment(v, vt);

  PGeoLineSegment *ls_below = nullptr;
  double u1 = INF, u2, u1_tmp;
  double best_dist_sq = INF;

  for (std::list<PGeoLineSegment *>::const_iterator it = ls_list.begin();
       it != ls_list.end(); ++it) {
    bool is_intersect =
        calcLineIntersection2D(ls_tmp, *it, u1_tmp, u2, TOLERANCE);

    if (is_intersect) {
      if (u1_tmp < 0 && (ls_below == nullptr || std::fabs(u1_tmp) < std::fabs(u1))) {
        u1 = u1_tmp;
        ls_below = *it;
        PDCELVertex *pt = (*it)->getParametricVertex(u2);
        best_dist_sq = calcDistanceSquared(v, pt);
        if (pt != (*it)->v1() && pt != (*it)->v2()) {
          delete pt;
        }
      }
    } else {
      if (!isCollinear(ls_tmp, *it)) {
        continue;
      }

      if ((*it)->vout()->point() < v->point()) {
        double d = calcDistanceSquared(v, (*it)->vout());
        if (ls_below == nullptr || d < best_dist_sq) {
          ls_below = *it;
          best_dist_sq = d;
          u1 = ls_tmp->getParametricLocation((*it)->vout());
        }
      }
    }
  }

  delete ls_tmp;
  delete vt;

  PDCELHalfEdge *he_below = nullptr;
  if (ls_below != nullptr) {
    he_below = dcel.findHalfEdgeBetween(ls_below->vin(), ls_below->vout());
  }

  for (std::vector<PGeoLineSegment *>::const_iterator it = temp_segs.begin();
       it != temp_segs.end(); ++it) {
    delete *it;
  }

  return he_below;
}
