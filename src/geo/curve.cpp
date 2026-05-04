#include "curve.hpp"

#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "geo.hpp"

#include <algorithm>

Baseline *joinCurves(std::list<Baseline *> curves) {
  if (curves.empty()) {
    std::cout << markError << " joinCurves: no curves were provided"
              << std::endl;
    return nullptr;
  }

  Baseline *bl;
  bl = new Baseline(curves.front());
  bl->setName(bl->getName() + "_new");
  curves.pop_front();

  while (!curves.empty()) {
    std::list<Baseline *>::iterator blit;
    for (blit = curves.begin(); blit != curves.end(); ++blit) {
      if ((*blit)->vertices().front() == bl->vertices().front()) {
        bl->join((*blit), 0, true);
        break;
      } else if ((*blit)->vertices().front() == bl->vertices().back()) {
        bl->join((*blit), 1, false);
        break;
      } else if ((*blit)->vertices().back() == bl->vertices().front()) {
        bl->join((*blit), 0, false);
        break;
      } else if ((*blit)->vertices().back() == bl->vertices().back()) {
        bl->join((*blit), 1, true);
        break;
      }
    }
    if (blit == curves.end()) {
      std::cout << markError << " joinCurves: disconnected curve set,"
                << " could not join all segments" << std::endl;
      delete bl;
      return nullptr;
    }
    curves.erase(blit);
  }

  return bl;
}

int joinCurves(Baseline *line, std::list<Baseline *> curves) {
  if (line == nullptr) {
    std::cout << markError << " joinCurves: output baseline must be valid"
              << std::endl;
    return -1;
  }
  if (line->vertices().empty() == false) {
    std::cout << markError << " joinCurves: output baseline must start empty"
              << std::endl;
    return -1;
  }
  if (curves.empty()) {
    std::cout << markError << " joinCurves: no curves were provided"
              << std::endl;
    return -1;
  }

  for (auto v : curves.front()->vertices()) {
    line->addPVertex(v);
  }
  curves.pop_front();

  while (!curves.empty()) {
    std::list<Baseline *>::iterator blit;
    for (blit = curves.begin(); blit != curves.end(); ++blit) {
      if ((*blit)->vertices().front() == line->vertices().front()) {
        line->join((*blit), 0, true);
        break;
      } else if ((*blit)->vertices().front() == line->vertices().back()) {
        line->join((*blit), 1, false);
        break;
      } else if ((*blit)->vertices().back() == line->vertices().front()) {
        line->join((*blit), 0, false);
        break;
      } else if ((*blit)->vertices().back() == line->vertices().back()) {
        line->join((*blit), 1, true);
        break;
      }
    }
    if (blit == curves.end()) {
      std::cout << markError << " joinCurves: disconnected curve set,"
                << " could not join all segments" << std::endl;
      return -1;
    }
    curves.erase(blit);
  }

  return 0;
}

int adjustCurveEnd(Baseline *bl, PGeoLineSegment *ls, CurveEnd end) {
  if (bl == nullptr || ls == nullptr) {
    std::cout << markError
              << " adjustCurveEnd: baseline and tool line must be valid"
              << std::endl;
    return -1;
  }
  if (bl->vertices().size() < 2) {
    std::cout << markError
              << " adjustCurveEnd: baseline needs at least two vertices"
              << std::endl;
    return -1;
  }

  PGeoLineSegment *ls_end;
  if (end == CurveEnd::Begin) {
    ls_end = new PGeoLineSegment(bl->vertices().front(),
                                 bl->getTangentVectorBegin());
  } else {
    ls_end =
        new PGeoLineSegment(bl->vertices().back(), bl->getTangentVectorEnd());
  }

  double u1, u2;
  bool has_intersection = calcLineIntersection2D(ls_end, ls, u1, u2, TOLERANCE);
  if (!has_intersection) {
    std::cout << markError
              << " adjustCurveEnd: failed to intersect baseline tangent with"
              << " tool line" << std::endl;
    delete ls_end;
    return -1;
  }
  PDCELVertex *vnew = ls_end->getParametricVertex(u1);
  delete ls_end;

  if (end == CurveEnd::Begin) {
    bl->vertices()[0] = vnew;
  } else {
    bl->vertices()[bl->vertices().size() - 1] = vnew;
  }
  return 0;
}

int mergeSortedVertexLists(const std::vector<PDCELVertex *> &vl_1,
                           const std::vector<PDCELVertex *> &vl_2,
                           std::vector<int> &vi_1, std::vector<int> &vi_2,
                           std::vector<PDCELVertex *> &vl_c) {
  std::size_t m, n;
  m = vl_1.size();
  n = vl_2.size();
  int k{static_cast<int>(vl_c.size())};

  if (m == 0 && n == 0) {
    return 0;
  }
  if (m == 0) {
    for (std::size_t p = 0; p < n; ++p) {
      vl_c.push_back(vl_2[p]);
      vi_2.push_back(k++);
    }
    return 0;
  }
  if (n == 0) {
    for (std::size_t p = 0; p < m; ++p) {
      vl_c.push_back(vl_1[p]);
      vi_1.push_back(k++);
    }
    return 0;
  }

  int d;
  if (std::abs(vl_1.front()->y() - vl_1.back()->y()) >=
      std::abs(vl_1.front()->z() - vl_1.back()->z()))
    d = 1;
  else
    d = 2;

  std::string order;
  if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) < 0.0)
    order = "asc";
  else
    order = "des";

  bool b_reverse_2;
  if ((vl_1.front()->point()[d] - vl_1.back()->point()[d]) *
          (vl_2.front()->point()[d] - vl_2.back()->point()[d]) >
      0.0)
    b_reverse_2 = false;
  else
    b_reverse_2 = true;
  std::vector<PDCELVertex *> vl_2_ordered = vl_2;
  if (b_reverse_2)
    std::reverse(vl_2_ordered.begin(), vl_2_ordered.end());

  int i{0}, j{0};
  while (i < m && j < n) {
    double diff{vl_1[i]->point()[d] - vl_2_ordered[j]->point()[d]};
    if ((order == "asc" && diff < (-TOLERANCE)) ||
        (order == "des" && diff > TOLERANCE)) {
      vl_c.push_back(vl_1[i]);
      vi_1.push_back(k);
      i++;
    } else if ((order == "asc" && diff > TOLERANCE) ||
               (order == "des" && diff < (-TOLERANCE))) {
      vl_c.push_back(vl_2_ordered[j]);
      vi_2.push_back(k);
      j++;
    } else if (std::abs(diff) <= TOLERANCE) {
      vl_c.push_back(vl_1[i]);
      vi_1.push_back(k);
      vi_2.push_back(k);
      i++;
      j++;
    }
    k++;
  }
  if (i < m) {
    for (int p = i; p < m; ++p) {
      vl_c.push_back(vl_1[p]);
      vi_1.push_back(k);
      k++;
    }
  } else if (j < n) {
    for (int p = j; p < n; ++p) {
      vl_c.push_back(vl_2_ordered[p]);
      vi_2.push_back(k);
      k++;
    }
  }

  if (b_reverse_2)
    std::reverse(vi_2.begin(), vi_2.end());

  return 0;
}
