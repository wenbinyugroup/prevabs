#include "airfoil_loader.hpp"

// MSVC needs this macro set before <cmath> for M_PI to be visible; we
// also set it via CMake compile definitions but defining it here as a
// belt-and-suspenders measure keeps this .cpp portable.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifndef CLIPPER2_AIRFOIL_DIR
#define CLIPPER2_AIRFOIL_DIR "."
#endif

namespace clipper2_airfoil {

namespace {

bool isCommentOrBlank(const std::string& line) {
  for (char c : line) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    return c == '#';
  }
  return true;
}

// Strict "two doubles only" parser. Returns true only when the line
// contains exactly two whitespace-separated doubles and nothing else
// after them (apart from whitespace). Without this guard, headers like
// "20-32C AIRFOIL" get partially parsed as (20, -32) by istringstream.
bool parseXyLine(const std::string& line, double& x, double& y) {
  std::istringstream iss(line);
  if (!(iss >> x >> y)) return false;
  std::string trailing;
  if (iss >> trailing) {
    // Anything beyond the two numbers (non-whitespace) makes the line
    // non-numeric — most likely a header that begins with digits.
    return false;
  }
  return true;
}

}  // namespace

AirfoilProfile loadFromFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) {
    throw std::runtime_error("airfoil_loader: failed to open " + path);
  }

  AirfoilProfile out;
  std::string line;
  // First non-blank line is the header.
  while (std::getline(f, line)) {
    if (!isCommentOrBlank(line)) {
      double tx, ty;
      if (parseXyLine(line, tx, ty)) {
        // No header — treat the whole file as numeric data.
        out.points.push_back({tx, ty});
        break;
      }
      // Trim trailing whitespace for nicer test output.
      while (!line.empty() && (line.back() == ' ' || line.back() == '\t'
             || line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
      }
      out.name = line;
      break;
    }
  }

  while (std::getline(f, line)) {
    if (isCommentOrBlank(line)) continue;
    double x, y;
    if (!parseXyLine(line, x, y)) {
      throw std::runtime_error(
          "airfoil_loader: malformed line in " + path + ": " + line);
    }
    out.points.push_back({x, y});
  }

  if (out.points.size() < 4) {
    throw std::runtime_error(
        "airfoil_loader: too few vertices in " + path);
  }

  const auto& f0 = out.points.front();
  const auto& fb = out.points.back();
  out.te_gap = std::sqrt((f0.x - fb.x) * (f0.x - fb.x)
                       + (f0.y - fb.y) * (f0.y - fb.y));
  out.te_closed = out.te_gap == 0.0;

  // When the .dat duplicates the TE point (sharp TE), drop the trailing
  // duplicate so Clipper2 sees the contour exactly once. Clipper2 closes
  // the polygon implicitly via EndType::Polygon.
  if (out.te_closed && out.points.size() > 2) {
    out.points.pop_back();
  }

  return out;
}

AirfoilProfile loadByName(const std::string& short_name) {
  return loadFromFile(std::string(CLIPPER2_AIRFOIL_DIR) + "/"
                      + short_name + ".dat");
}

AirfoilProfile makeNaca4(const std::string& four_digit,
                         int n_chord, double chord) {
  if (four_digit.size() != 4) {
    throw std::runtime_error(
        "makeNaca4: code must be 4 digits, got '" + four_digit + "'");
  }
  const double m = (four_digit[0] - '0') / 100.0;     // max camber
  const double p = (four_digit[1] - '0') / 10.0;      // pos of max camber
  const int    tt_int = std::stoi(four_digit.substr(2, 2));
  const double t = tt_int / 100.0;                    // thickness

  if (n_chord < 3) n_chord = 3;

  // Cosine-spaced x in [0, 1].
  std::vector<double> xs(n_chord);
  for (int i = 0; i < n_chord; ++i) {
    const double beta = M_PI * static_cast<double>(i)
                            / static_cast<double>(n_chord - 1);
    xs[i] = 0.5 * (1.0 - std::cos(beta));
  }

  auto yt = [&](double x) {
    return 5.0 * t * (
        0.2969 * std::sqrt(x)
      - 0.1260 * x
      - 0.3516 * x * x
      + 0.2843 * x * x * x
      - 0.1015 * x * x * x * x);   // sharp-TE form (no 0.1036 tweak)
  };
  auto camber = [&](double x, double& yc, double& dyc_dx) {
    if (p > 0.0 && m > 0.0) {
      if (x < p) {
        yc     = m / (p * p) * (2 * p * x - x * x);
        dyc_dx = 2 * m / (p * p) * (p - x);
      } else {
        yc     = m / ((1 - p) * (1 - p)) * (1 - 2 * p + 2 * p * x - x * x);
        dyc_dx = 2 * m / ((1 - p) * (1 - p)) * (p - x);
      }
    } else {
      yc = 0.0;
      dyc_dx = 0.0;
    }
  };

  AirfoilProfile out;
  out.name = "NACA " + four_digit + " (synthetic)";

  // Upper surface: LE -> TE
  std::vector<std::pair<double, double>> upper;
  upper.reserve(n_chord);
  for (int i = 0; i < n_chord; ++i) {
    const double x = xs[i];
    const double half_t = yt(x);
    double yc, dy;
    camber(x, yc, dy);
    const double theta = std::atan(dy);
    const double xu = x - half_t * std::sin(theta);
    const double yu = yc + half_t * std::cos(theta);
    upper.emplace_back(xu, yu);
  }

  // Lower surface: LE -> TE
  std::vector<std::pair<double, double>> lower;
  lower.reserve(n_chord);
  for (int i = 0; i < n_chord; ++i) {
    const double x = xs[i];
    const double half_t = yt(x);
    double yc, dy;
    camber(x, yc, dy);
    const double theta = std::atan(dy);
    const double xl = x + half_t * std::sin(theta);
    const double yl = yc - half_t * std::cos(theta);
    lower.emplace_back(xl, yl);
  }

  // Order: TE upper -> LE -> TE lower, mirroring the UIUC convention.
  out.points.clear();
  for (int i = n_chord - 1; i >= 0; --i) {
    out.points.push_back({upper[i].first * chord, upper[i].second * chord});
  }
  for (int i = 1; i < n_chord; ++i) {  // skip LE duplicate
    out.points.push_back({lower[i].first * chord, lower[i].second * chord});
  }

  // Sharp TE in this construction — first vs last should coincide up to
  // floating point rounding from the camber/thickness composition.
  // Snap them so callers can rely on te_closed.
  if (out.points.size() > 2) {
    const auto& f0 = out.points.front();
    auto&       fb = out.points.back();
    const double gap = std::sqrt((f0.x - fb.x) * (f0.x - fb.x)
                               + (f0.y - fb.y) * (f0.y - fb.y));
    if (gap < 1e-9) {
      fb.x = f0.x;
      fb.y = f0.y;
      // And then drop the duplicate so Clipper2 sees a clean contour.
      out.points.pop_back();
    }
  }
  out.te_gap = 0.0;
  out.te_closed = true;

  return out;
}

}  // namespace clipper2_airfoil
