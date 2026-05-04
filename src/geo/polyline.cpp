#include "polyline.hpp"

#include "globalVariables.hpp"
#include "utilities.hpp"

#include <limits>

namespace {

const char *toAxisString(PolylineAxis axis) {
  return axis == PolylineAxis::X2 ? "x2" : "x3";
}

}  // namespace

double calcPolylineLength(const std::vector<PDCELVertex *> &ps) {
  double len = 0;
  for (auto i = 1; i < ps.size(); ++i) {
    len += ps[i - 1]->point().distance(ps[i]->point());
  }
  return len;
}

PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc, double tol, double &param,
  const int count, const PolylineAxis axis
) {
  param = 0.0;
  if (ps.size() < 2) {
    std::cout << markError
              << " findPolylinePointByCoordinate: polyline needs at least"
              << " two vertices" << std::endl;
    return nullptr;
  }

  if (count < 1) {
    std::cout << markError
              << " findPolylinePointByCoordinate: count must be >= 1"
              << std::endl;
    return nullptr;
  }

  double length = calcPolylineLength(ps);
  if (length <= tol) {
    std::cout << markError
              << " findPolylinePointByCoordinate: polyline length is zero"
              << std::endl;
    return nullptr;
  }

  PDCELVertex *pv = new PDCELVertex(label);
  double ulength{0};
  int counter{0};
  double left_x2{0.0}, left_x3{0.0}, right_x2{0.0}, right_x3{0.0};
  bool found{false};

  for (std::size_t i = 0; i + 1 < ps.size(); ++i) {
    double dl = ps[i]->point().distance(ps[i + 1]->point());
    if (axis == PolylineAxis::X2) {
      left_x2 = ps[i]->y();
      left_x3 = ps[i]->z();
      right_x2 = ps[i + 1]->y();
      right_x3 = ps[i + 1]->z();
    } else {
      left_x3 = ps[i]->y();
      left_x2 = ps[i]->z();
      right_x3 = ps[i + 1]->y();
      right_x2 = ps[i + 1]->z();
    }
    if ((left_x2 <= loc && loc <= right_x2) ||
        (left_x2 >= loc && loc >= right_x2)) {
      counter++;
      if (counter == count) {
        if (fabs(loc - left_x2) < tol) {
          pv->setPosition(ps[i]->x(), ps[i]->y(), ps[i]->z());
          break;
        }
        else if (fabs(loc - right_x2) < tol) {
          ulength += dl;
          pv->setPosition(ps[i + 1]->x(), ps[i + 1]->y(), ps[i + 1]->z());
          found = true;
          break;
        }
        else {
          double dy, dz, loc2;
          dy = right_x2 - left_x2;
          if (std::abs(dy) < tol) {
            std::cout << markError
                      << " findPolylinePointByCoordinate: coordinate "
                      << toAxisString(axis)
                      << " is constant on the matched segment"
                      << std::endl;
            delete pv;
            return nullptr;
          }
          dz = right_x3 - left_x3;
          loc2 = dz / dy * (loc - left_x2) + left_x3;
          ulength += dl * (loc - left_x2) / dy;
          if (axis == PolylineAxis::X2) {
            pv->setPosition(0, loc, loc2);
          } else {
            pv->setPosition(0, loc2, loc);
          }
          found = true;
          break;
        }
      }
    }
    ulength += dl;
  }

  if (!found) {
    std::cout << markError
              << " findPolylinePointByCoordinate: failed to find coordinate "
              << loc << " on polyline by " << toAxisString(axis)
              << " (count = " << count << ")" << std::endl;
    delete pv;
    return nullptr;
  }

  param = ulength / length;
  return pv;
}

PDCELVertex *findPolylinePointByCoordinate(
  const std::vector<PDCELVertex *> &ps, const std::string label,
  const double loc, double tol,
  const int count, const PolylineAxis axis
) {
  double param{0};
  return findPolylinePointByCoordinate(
      ps, label, loc, tol, param, count, axis);
}

double findPolylineParamByCoordinate(
  const std::vector<PDCELVertex *> &ps,
  const double loc, double tol,
  const int count, const PolylineAxis axis
) {
  double param{0};
  std::string label{"newp"};
  PDCELVertex *pv =
      findPolylinePointByCoordinate(ps, label, loc, tol, param, count, axis);
  if (pv == nullptr) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  delete pv;
  return param;
}

PDCELVertex *findPolylinePointAtParam(
  const std::vector<PDCELVertex *> &ps,
  const double &u, bool &is_new, int &seg, const double &tol
) {
  if (ps.size() < 2) {
    is_new = false;
    seg = 0;
    return nullptr;
  }

  if (u < -tol || u > 1.0 + tol) {
    std::cout << markError
              << " findPolylinePointAtParam: parameter u = " << u
              << " is outside [0, 1]" << std::endl;
    is_new = false;
    seg = 0;
    return nullptr;
  }

  double length = calcPolylineLength(ps);
  if (length <= tol) {
    std::cout << markError
              << " findPolylinePointAtParam: polyline length is zero"
              << std::endl;
    is_new = false;
    seg = 0;
    return nullptr;
  }

  if (u <= tol) {
    is_new = false;
    seg = 0;
    return ps.front();
  }
  if (u >= 1.0 - tol) {
    is_new = false;
    seg = static_cast<int>(ps.size()) - 1;
    return ps.back();
  }

  double ulength = u * length;

  std::size_t nlseg = ps.size() - 1;
  double ui = 0, li = 0.0;
  bool found_segment{false};
  for (std::size_t i = 0; i < nlseg; ++i) {
    li = ps[i]->point().distance(ps[i + 1]->point());
    if (li <= tol) {
      continue;
    }
    if (ulength > li) {
      ulength -= li;
    }
    else {
      seg = static_cast<int>(i);
      found_segment = true;
      break;
    }
  }

  if (!found_segment) {
    is_new = false;
    seg = static_cast<int>(ps.size()) - 1;
    return ps.back();
  }

  ui = ulength / li;
  SPoint3 newp = calcPointFromParam(
    ps[seg]->point(), ps[seg + 1]->point(), ui, is_new, tol
  );
  if (!is_new) {
    if (ui <= 0.5) {
      return ps[seg];
    }
    return ps[seg + 1];
  }
  PDCELVertex *newv = new PDCELVertex(newp);
  seg += 1;
  return newv;
}

int trimCurveAtVertex(std::vector<PDCELVertex *> &c, PDCELVertex *v,
                      CurveEnd remove) {
  if (v == nullptr) {
    std::cout << markError << " trim: trim vertex must be valid" << std::endl;
    return -1;
  }

  std::vector<PDCELVertex *>::iterator it = std::find(c.begin(), c.end(), v);
  if (it == c.end()) {
    std::cout << markError << " trim: trim vertex was not found on the curve"
              << std::endl;
    return -1;
  }

  if (remove == CurveEnd::Begin) {
    c.assign(it, c.end());
  } else {
    c.assign(c.begin(), it + 1);
  }

  return 0;
}
