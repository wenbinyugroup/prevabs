#include "PModelIO.hpp"

#include "PBaseLine.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include "rapidxml/rapidxml.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

typedef std::pair<double, double> AirfoilCoord;
typedef std::vector<AirfoilCoord> AirfoilSection;

// Geometric mirror options applied after the airfoil polyline is created.
struct AirfoilFlipOptions {
  bool enabled = false;
  int axis = 2;
  double loc = 0.5;
};

struct AirfoilEdgeOverride {
  bool has_node = false;
  bool has_name = false;
  bool has_coord = false;
  std::string name;
  AirfoilCoord coord = AirfoilCoord(0.0, 0.0);
};




// XML-driven options collected for one airfoil baseline definition.
struct AirfoilReadOptions {
  std::string data_form = "file";
  std::string format = "2";
  int head_rows = 0;
  int direction = 1;
  bool reverse = false;
  AirfoilFlipOptions flip;
  AirfoilEdgeOverride le_override;
  AirfoilEdgeOverride te_override;
  bool normalize = false;
};

struct NormalizeTransform {
  double scale = 1.0;
  double offset_x = 0.0;
};

struct EdgePlacement {
  AirfoilSection section;
  AirfoilCoord coord = AirfoilCoord(0.0, 0.0);
  std::size_t index = 0;
};




std::string airfoilContext(const Baseline *line) {
  return "airfoil baseline '" + line->getName() + "'";
}

std::string airfoilDebugCsvPath(const Baseline *line) {
  return config.file_directory + "debug_" + line->getName() + "_final.csv";
}

rapidxml::xml_node<> *findOptionalUniqueChild(
  const rapidxml::xml_node<> *parent, const char *name,
  const std::string &context
) {
  rapidxml::xml_node<> *node = parent->first_node(name);
  if (!node) {
    return nullptr;
  }
  if (node->next_sibling(name)) {
    throw std::runtime_error(
      "Duplicate XML element <" + std::string(name) + "> in " + context
    );
  }
  return node;
}








bool parseOptionalBoolNodeValue(
  const rapidxml::xml_node<> *parent, const char *name,
  const std::string &context
) {
  rapidxml::xml_node<> *node = findOptionalUniqueChild(parent, name, context);
  if (!node) {
    return false;
  }

  const std::string value = trim(node->value());
  if (value.empty()) {
    return true;
  }
  return parseXmlBoolValue(value, context + "/<" + name + ">");
}

AirfoilEdgeOverride parseAirfoilEdgeNode(
  const rapidxml::xml_node<> *p_xn_line, const char *tag,
  const std::string &context
) {
  AirfoilEdgeOverride edge;
  rapidxml::xml_node<> *node = findOptionalUniqueChild(p_xn_line, tag, context);
  if (!node) {
    return edge;
  }

  edge.has_node = true;

  rapidxml::xml_attribute<> *name_attr = node->first_attribute("name");
  if (name_attr) {
    const std::string name = trim(name_attr->value());
    if (!name.empty()) {
      edge.has_name = true;
      edge.name = name;
    }
  }

  const std::string value = trim(node->value());
  if (value.empty()) {
    return edge;
  }

  std::stringstream ss(value);
  double x = 0.0;
  double y = 0.0;
  std::string trailing_token;
  if (!(ss >> x >> y) || (ss >> trailing_token)) {
    throw std::runtime_error(
      "Invalid airfoil edge coordinates '" + value + "' in " + context
      + "/<" + tag + "> (expected exactly two numbers)"
    );
  }

  edge.has_coord = true;
  edge.coord = AirfoilCoord(x, y);
  return edge;
}

bool parseAirfoilNormalize(
  const rapidxml::xml_node<> *p_xn_line, const std::string &context
) {
  return parseOptionalBoolNodeValue(p_xn_line, "normalize", context);
}




int parseAirfoilDirection(
  const rapidxml::xml_node<> *p_xn_pts, const std::string &context
) {
  rapidxml::xml_attribute<> *p_xa_direction =
    p_xn_pts->first_attribute("direction");
  if (!p_xa_direction) {
    return 1;
  }

  const std::string value = lowerString(trim(p_xa_direction->value()));
  if (value.empty()) {
    return 1;
  }
  if (value == "1" || value == "ccw" || value == "forward") {
    return 1;
  }
  if (value == "-1" || value == "cw" || value == "reverse") {
    return -1;
  }

  throw std::runtime_error(
    "Invalid airfoil direction '" + std::string(p_xa_direction->value())
    + "' in " + context + "/<points>"
  );
}




AirfoilFlipOptions parseAirfoilFlipOptions(
  const rapidxml::xml_node<> *p_xn_line, const std::string &context
) {
  AirfoilFlipOptions options;
  rapidxml::xml_node<> *p_xn_flip = p_xn_line->first_node("flip");
  if (!p_xn_flip) {
    return options;
  }

  options.enabled = parseOptionalBoolNodeValue(p_xn_line, "flip", context);
  if (!options.enabled) {
    return options;
  }

  rapidxml::xml_attribute<> *p_xa_axis = p_xn_flip->first_attribute("axis");
  if (p_xa_axis) {
    const std::string axis = lowerString(trim(p_xa_axis->value()));
    if (axis == "2" || axis == "x2" || axis == "y") {
      options.axis = 2;
    } else if (axis == "3" || axis == "x3" || axis == "z") {
      options.axis = 3;
    } else {
      throw std::runtime_error(
        "Invalid flip axis '" + std::string(p_xa_axis->value()) + "' in "
        + context + "/<flip> (expected 2/x2/y or 3/x3/z)"
      );
    }
  }

  rapidxml::xml_attribute<> *p_xa_loc = p_xn_flip->first_attribute("loc");
  if (p_xa_loc) {
    options.loc = parseRequiredDouble(
      p_xa_loc->value(), context + "/<flip>@loc"
    );
  }

  return options;
}




bool isAirfoilSeligFormat(const std::string &format) {
  const std::string normalized = lowerString(trim(format));
  return normalized == "1" || normalized == "selig"
         || (!normalized.empty() && normalized[0] == 's');
}




bool isAirfoilLednicerFormat(const std::string &format) {
  const std::string normalized = lowerString(trim(format));
  return normalized == "2" || normalized == "lednicer"
         || (!normalized.empty() && normalized[0] == 'l');
}




bool isEdgeCoord(
  const AirfoilCoord &coord, const AirfoilCoord &edge, double tol
) {
  return std::fabs(coord.first - edge.first) <= tol
         && std::fabs(coord.second - edge.second) <= tol;
}




bool sameAirfoilCoord(
  const AirfoilCoord &lhs, const AirfoilCoord &rhs, double tol
) {
  return std::fabs(lhs.first - rhs.first) <= tol
         && std::fabs(lhs.second - rhs.second) <= tol;
}




void appendAirfoilCoord(
  AirfoilSection &coords, const AirfoilCoord &coord, double tol
) {
  if (!coords.empty() && sameAirfoilCoord(coords.back(), coord, tol)) {
    return;
  }
  coords.push_back(coord);
}

NormalizeTransform computeNormalizeTransform(
  const std::vector<AirfoilSection> &sections, const std::string &context
) {
  bool found_coord = false;
  double min_x = 0.0;
  double max_x = 0.0;

  for (std::size_t i = 0; i < sections.size(); ++i) {
    for (std::size_t j = 0; j < sections[i].size(); ++j) {
      const double x = sections[i][j].first;
      if (!found_coord) {
        min_x = x;
        max_x = x;
        found_coord = true;
      } else {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
      }
    }
  }

  if (!found_coord) {
    throw std::runtime_error(
      "No airfoil coordinates available for normalization in " + context
    );
  }

  const double scale = max_x - min_x;
  if (scale < GEO_TOL) {
    throw std::runtime_error(
      "Airfoil x-range is degenerate for normalization in " + context
    );
  }

  NormalizeTransform t;
  t.scale = scale;
  t.offset_x = min_x;
  return t;
}

void applyNormalizeToSections(
  std::vector<AirfoilSection> &sections, const NormalizeTransform &t
) {
  for (std::size_t i = 0; i < sections.size(); ++i) {
    for (std::size_t j = 0; j < sections[i].size(); ++j) {
      sections[i][j].first = (sections[i][j].first - t.offset_x) / t.scale;
      sections[i][j].second = sections[i][j].second / t.scale;
    }
  }
}

AirfoilCoord applyNormalizeToCoord(
  const AirfoilCoord &c, const NormalizeTransform &t
) {
  return AirfoilCoord(
    (c.first - t.offset_x) / t.scale,
    c.second / t.scale
  );
}

double distanceSquared(
  const AirfoilCoord &lhs, const AirfoilCoord &rhs
) {
  const double dx = lhs.first - rhs.first;
  const double dy = lhs.second - rhs.second;
  return dx * dx + dy * dy;
}

double pointSegmentDistanceSquared(
  const AirfoilCoord &p, const AirfoilCoord &a, const AirfoilCoord &b
) {
  const double abx = b.first - a.first;
  const double aby = b.second - a.second;
  const double ab2 = abx * abx + aby * aby;
  if (ab2 <= GEO_TOL * GEO_TOL) {
    return distanceSquared(p, a);
  }

  const double apx = p.first - a.first;
  const double apy = p.second - a.second;
  double t = (apx * abx + apy * aby) / ab2;
  if (t < 0.0) {
    t = 0.0;
  } else if (t > 1.0) {
    t = 1.0;
  }

  const AirfoilCoord proj(
    a.first + t * abx,
    a.second + t * aby
  );
  return distanceSquared(p, proj);
}

EdgePlacement placeEdgeOnSection(
  const AirfoilSection &section, const AirfoilCoord &edge, double tol,
  const std::string &context
) {
  if (section.empty()) {
    throw std::runtime_error(
      "Cannot place airfoil edge on an empty section in " + context
    );
  }

  for (std::size_t i = 0; i < section.size(); ++i) {
    if (isEdgeCoord(section[i], edge, tol)) {
      EdgePlacement placement;
      placement.section = section;
      placement.coord = section[i];
      placement.index = i;
      return placement;
    }
  }

  if (section.size() < 2) {
    throw std::runtime_error(
      "Cannot insert airfoil edge into a section with fewer than two points in "
      + context
    );
  }

  std::size_t insert_after = 0;
  double best_dist2 = pointSegmentDistanceSquared(edge, section[0], section[1]);
  for (std::size_t i = 1; i + 1 < section.size(); ++i) {
    const double dist2 =
      pointSegmentDistanceSquared(edge, section[i], section[i + 1]);
    if (dist2 < best_dist2) {
      best_dist2 = dist2;
      insert_after = i;
    }
  }

  EdgePlacement placement;
  placement.section = section;
  placement.section.insert(placement.section.begin() + insert_after + 1, edge);
  placement.coord = edge;
  placement.index = insert_after + 1;
  return placement;
}

EdgePlacement detectSeligLeadingEdge(
  const AirfoilSection &section, double tol, const std::string &context
) {
  if (section.size() < 2) {
    throw std::runtime_error(
      "Selig airfoil data in " + context
      + " must contain at least two coordinate rows"
    );
  }

  double min_x = section.front().first;
  for (std::size_t i = 1; i < section.size(); ++i) {
    if (section[i].first < min_x) {
      min_x = section[i].first;
    }
  }

  std::size_t min_begin = section.size();
  std::size_t min_end = section.size();
  for (std::size_t i = 0; i < section.size(); ++i) {
    if (std::fabs(section[i].first - min_x) <= tol) {
      if (min_begin == section.size()) {
        min_begin = i;
      }
      min_end = i;
    }
  }

  if (min_begin == section.size()) {
    throw std::runtime_error(
      "Selig airfoil data in " + context
      + " does not contain a valid leading-edge neighborhood"
    );
  }

  std::size_t lower_start = min_begin;
  bool found_non_positive = false;
  for (std::size_t i = min_begin; i <= min_end; ++i) {
    if (section[i].second <= 0.0) {
      lower_start = i;
      found_non_positive = true;
      break;
    }
  }
  if (!found_non_positive) {
    lower_start = min_end;
  }

  if (lower_start == 0 || lower_start >= section.size()) {
    throw std::runtime_error(
      "Selig airfoil data in " + context
      + " does not contain a usable lower surface after the leading edge"
    );
  }

  EdgePlacement placement;
  placement.section = section;
  placement.coord = section[lower_start];
  placement.index = lower_start;
  return placement;
}

AirfoilCoord resolveSeligTrailingEdgeCoord(
  const AirfoilSection &section, const AirfoilEdgeOverride &te, double tol
) {
  if (section.empty()) {
    return AirfoilCoord(1.0, 0.0);
  }
  if (te.has_coord) {
    if (isEdgeCoord(section.front(), te.coord, tol)) {
      return section.front();
    }
    if (isEdgeCoord(section.back(), te.coord, tol)) {
      return section.back();
    }
  }
  return section.front();
}




std::vector<AirfoilSection> readAirfoilSectionsFromFile(
  const std::string &filename, int head_rows, const std::string &context
) {
  std::ifstream ifs(filename.c_str());
  if (!ifs.is_open()) {
    throw std::runtime_error(
      "unable to open airfoil data file '" + filename + "' for " + context
    );
  }

  PLOG(info) << "reading airfoil data file: " << filename;

  std::vector<AirfoilSection> sections;
  AirfoilSection current_section;
  std::string data_line;
  int line_number = 0;
  int data_rows = 0;

  while (getline(ifs, data_line)) {
    ++line_number;
    if (line_number <= head_rows) {
      continue;
    }

    const std::string trimmed_line = trim(data_line);
    if (trimmed_line.empty()) {
      // Blank lines separate the upper/lower blocks in Lednicer files.
      if (!current_section.empty()) {
        sections.push_back(current_section);
        current_section.clear();
      }
      continue;
    }

    std::stringstream ss(trimmed_line);
    double x = 0.0;
    double y = 0.0;
    std::string trailing_token;
    if (!(ss >> x >> y) || (ss >> trailing_token)) {
      throw std::runtime_error(
        "Invalid airfoil coordinate row '" + trimmed_line + "' in file '"
        + filename + "' for " + context
      );
    }

    current_section.push_back(AirfoilCoord(x, y));
    ++data_rows;
  }

  if (!current_section.empty()) {
    sections.push_back(current_section);
  }

  if (data_rows == 0) {
    throw std::runtime_error(
      "No airfoil coordinates found in file '" + filename + "' for " + context
    );
  }

  return sections;
}




AirfoilSection buildClosedSeligAirfoil(
  const AirfoilSection &section, AirfoilEdgeOverride &le,
  AirfoilEdgeOverride &te, double tol, const std::string &context
) {
  const EdgePlacement le_placement = le.has_coord
    ? placeEdgeOnSection(section, le.coord, tol, context)
    : detectSeligLeadingEdge(section, tol, context);
  const AirfoilSection &work = le_placement.section;

  // Normalize the output to one closed baseline that always starts at TE.
  const AirfoilCoord te_coord = resolveSeligTrailingEdgeCoord(work, te, tol);
  AirfoilSection closed;
  appendAirfoilCoord(closed, te_coord, tol);

  for (std::size_t i = 0; i < le_placement.index; ++i) {
    const AirfoilCoord &coord = work[i];
    if (isEdgeCoord(coord, te_coord, tol)) {
      continue;
    }
    if (isEdgeCoord(coord, le_placement.coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  appendAirfoilCoord(closed, le_placement.coord, tol);

  for (std::size_t i = le_placement.index + 1; i < work.size(); ++i) {
    const AirfoilCoord &coord = work[i];
    if (isEdgeCoord(coord, te_coord, tol)
        || isEdgeCoord(coord, le_placement.coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  appendAirfoilCoord(closed, te_coord, tol);
  le.coord = le_placement.coord;
  te.coord = te_coord;
  return closed;
}




AirfoilSection buildClosedLednicerAirfoil(
  const std::vector<AirfoilSection> &sections, AirfoilEdgeOverride &le,
  AirfoilEdgeOverride &te, double tol, const std::string &context
) {
  if (sections.size() != 2) {
    throw std::runtime_error(
      "Lednicer airfoil data in " + context
      + " must contain exactly two coordinate blocks "
        "(upper surface, blank line, lower surface)"
    );
  }
  if (sections[0].empty() || sections[1].empty()) {
    throw std::runtime_error(
      "Lednicer airfoil data in " + context
      + " must contain non-empty upper and lower surface blocks"
    );
  }

  const AirfoilSection &upper = sections[0];
  const AirfoilSection &lower = sections[1];
  const AirfoilCoord le_seed = le.has_coord ? le.coord : upper.front();
  const AirfoilCoord te_seed = te.has_coord ? te.coord : upper.back();
  const EdgePlacement upper_le =
    placeEdgeOnSection(upper, le_seed, tol, context + " upper surface");
  const EdgePlacement upper_te =
    placeEdgeOnSection(upper_le.section, te_seed, tol, context + " upper surface");
  const EdgePlacement lower_le =
    placeEdgeOnSection(lower, le_seed, tol, context + " lower surface");
  const EdgePlacement lower_te =
    placeEdgeOnSection(lower_le.section, te_seed, tol, context + " lower surface");

  if (upper_te.index < upper_le.index) {
    throw std::runtime_error(
      "Lednicer upper surface edge ordering is invalid in " + context
    );
  }
  if (lower_te.index < lower_le.index) {
    throw std::runtime_error(
      "Lednicer lower surface edge ordering is invalid in " + context
    );
  }

  // Lednicer stores both surfaces as LE -> TE, so reverse the upper block
  // to produce the same normalized TE -> upper -> LE -> lower -> TE order.
  const AirfoilCoord le_coord = upper_le.coord;
  const AirfoilCoord te_coord = upper_te.coord;
  AirfoilSection closed;
  appendAirfoilCoord(closed, te_coord, tol);

  for (std::size_t i = upper_te.index + 1; i > upper_le.index; --i) {
    const AirfoilCoord &coord = upper_te.section[i - 1];
    if (isEdgeCoord(coord, te_coord, tol)) {
      continue;
    }
    if (isEdgeCoord(coord, le_coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  appendAirfoilCoord(closed, le_coord, tol);

  for (std::size_t i = lower_le.index + 1; i <= lower_te.index; ++i) {
    const AirfoilCoord &coord = lower_te.section[i];
    if (isEdgeCoord(coord, le_coord, tol) || isEdgeCoord(coord, te_coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  appendAirfoilCoord(closed, te_coord, tol);
  le.coord = le_coord;
  te.coord = te_coord;
  return closed;
}




std::string makeAirfoilVertexName(
  const Baseline *line, const std::string &suffix
) {
  // Generated names stay local to the baseline and avoid repository collisions.
  return line->getName() + "_" + suffix;
}

int signWithTol(double value, double tol) {
  if (value > tol) {
    return 1;
  }
  if (value < -tol) {
    return -1;
  }
  return 0;
}

void warnIfEdgeNeighborhoodInconsistent(
  const AirfoilSection &closed, const AirfoilCoord &edge_coord,
  const std::string &edge_label, const std::string &context, double tol
) {
  if (closed.size() < 3) {
    return;
  }

  bool found = false;
  std::size_t edge_index = 0;
  int best_neighbors = -1;
  for (std::size_t i = 0; i < closed.size(); ++i) {
    if (!isEdgeCoord(closed[i], edge_coord, tol)) {
      continue;
    }
    const int neighbor_count =
      static_cast<int>((std::min)(i, static_cast<std::size_t>(2)))
      + static_cast<int>((std::min)(closed.size() - 1 - i,
                                    static_cast<std::size_t>(2)));
    if (!found || neighbor_count > best_neighbors) {
      found = true;
      edge_index = i;
      best_neighbors = neighbor_count;
    }
  }
  if (!found) {
    return;
  }

  const std::size_t begin = edge_index > 2 ? edge_index - 2 : 0;
  const std::size_t end = (std::min)(edge_index + 2, closed.size() - 1);
  if (end - begin < 2) {
    return;
  }

  int reference_sign = 0;
  for (std::size_t i = begin; i + 2 <= end; ++i) {
    const AirfoilCoord &p0 = closed[i];
    const AirfoilCoord &p1 = closed[i + 1];
    const AirfoilCoord &p2 = closed[i + 2];
    const double v1x = p1.first - p0.first;
    const double v1y = p1.second - p0.second;
    const double v2x = p2.first - p1.first;
    const double v2y = p2.second - p1.second;
    const int sign = signWithTol(v1x * v2y - v1y * v2x, tol);
    if (sign == 0) {
      continue;
    }
    if (reference_sign == 0) {
      reference_sign = sign;
      continue;
    }
    if (sign != reference_sign) {
      PLOG(warning)
        << "user-specified " << edge_label << " for " << context
        << " appears geometrically inconsistent: turn signs at neighborhood "
           "are not uniform";
      return;
    }
  }
}

std::string resolveAirfoilEdgeName(
  const Baseline *line, const AirfoilEdgeOverride &edge,
  const std::string &default_suffix
) {
  if (edge.has_name) {
    return edge.name;
  }
  return makeAirfoilVertexName(line, default_suffix);
}

void ensureAirfoilVertexNameAvailable(
  PModel *pmodel, const std::string &name, const Baseline *line
) {
  if (pmodel->getPointByName(name) != nullptr) {
    throw std::runtime_error(
      "Airfoil edge point name '" + name + "' already exists in "
      + airfoilContext(line)
    );
  }
}




void populateAirfoilBaseline(
  Baseline *line, const AirfoilSection &coords, PModel *pmodel,
  const AirfoilEdgeOverride &le, const AirfoilEdgeOverride &te, double tol
) {
  const std::string le_name = resolveAirfoilEdgeName(line, le, "le");
  const std::string te_name = resolveAirfoilEdgeName(line, te, "te");
  if (le_name == te_name) {
    throw std::runtime_error(
      "Airfoil edge point names conflict: '" + le_name + "' in "
      + airfoilContext(line)
    );
  }
  ensureAirfoilVertexNameAvailable(pmodel, le_name, line);
  ensureAirfoilVertexNameAvailable(pmodel, te_name, line);

  PDCELVertex *pv_le = new PDCELVertex(
    le_name, 0.0, le.coord.first, le.coord.second
  );
  PDCELVertex *pv_te = new PDCELVertex(
    te_name, 0.0, te.coord.first, te.coord.second
  );

  pmodel->addVertex(pv_le);
  pmodel->addVertex(pv_te);

  int generated_point_count = 0;
  for (std::size_t i = 0; i < coords.size(); ++i) {
    // Reuse resolved LE/TE vertices and create named interior samples for
    // all remaining coordinates on the normalized polyline.
    PDCELVertex *vertex = nullptr;
    if (isEdgeCoord(coords[i], le.coord, tol)) {
      vertex = pv_le;
    } else if (isEdgeCoord(coords[i], te.coord, tol)) {
      vertex = pv_te;
    } else {
      ++generated_point_count;
      vertex = new PDCELVertex(
        makeAirfoilVertexName(
          line, "p" + std::to_string(generated_point_count)
        ),
        0.0,
        coords[i].first,
        coords[i].second
      );
      pmodel->addVertex(vertex);
    }
    line->addPVertex(vertex);
  }

  if (le.has_node && le.has_coord) {
    warnIfEdgeNeighborhoodInconsistent(
      coords, le.coord, "LE", airfoilContext(line), tol
    );
  }
  if (te.has_node && te.has_coord) {
    warnIfEdgeNeighborhoodInconsistent(
      coords, te.coord, "TE", airfoilContext(line), tol
    );
  }
}




void applyAirfoilFlip(Baseline *line, const AirfoilFlipOptions &flip) {
  std::size_t n_vertices = line->vertices().size();
  if (n_vertices == 0) {
    return;
  }
  if (line->isClosed()) {
    // The closing point is the same object as the first point.
    --n_vertices;
  }

  for (std::size_t i = 0; i < n_vertices; ++i) {
    PDCELVertex *vertex = line->vertices()[i];
    const double x = vertex->x();
    double y = vertex->y();
    double z = vertex->z();

    if (flip.axis == 2) {
      y = 2.0 * flip.loc - y;
    } else if (flip.axis == 3) {
      z = 2.0 * flip.loc - z;
    }

    vertex->setPosition(x, y, z);
  }
}

void writeAirfoilBaselineDebugCsv(Baseline *line) {
  const std::string filename = airfoilDebugCsvPath(line);
  std::ofstream ofs(filename.c_str());
  if (!ofs.is_open()) {
    PLOG(warning) << "unable to write airfoil debug csv: " << filename;
    return;
  }

  ofs << "label,x,y\n";
  for (std::size_t i = 0; i < line->vertices().size(); ++i) {
    PDCELVertex *vertex = line->vertices()[i];
    ofs << vertex->name() << ','
        << vertex->y() << ','
        << vertex->z() << '\n';
  }
}

}  // namespace




int readLineTypeAirfoil(
  Baseline *line, const rapidxml::xml_node<> *p_xn_line,
  const rapidxml::xml_node<> * /*p_xn_geo*/, PModel *pmodel, const double tol
) {
  const std::string context = airfoilContext(line);
  rapidxml::xml_node<> *p_xn_pts = requireNode(p_xn_line, "points", context);

  AirfoilReadOptions options;

  rapidxml::xml_attribute<> *p_xa_data = p_xn_pts->first_attribute("data");
  if (p_xa_data) {
    options.data_form = lowerString(trim(p_xa_data->value()));
  }

  rapidxml::xml_attribute<> *p_xa_format = p_xn_pts->first_attribute("format");
  if (p_xa_format) {
    options.format = trim(p_xa_format->value());
  }

  rapidxml::xml_attribute<> *p_xa_header = p_xn_pts->first_attribute("header");
  if (p_xa_header && trim(p_xa_header->value()).size() > 0) {
    options.head_rows = parseRequiredInt(
      p_xa_header->value(), context + "/<points>@header"
    );
    if (options.head_rows < 0) {
      throw std::runtime_error(
        "Airfoil header row count must be non-negative in " + context
      );
    }
  }

  options.direction = parseAirfoilDirection(p_xn_pts, context);
  options.reverse = parseOptionalBoolNodeValue(p_xn_line, "reverse", context);
  options.flip = parseAirfoilFlipOptions(p_xn_line, context);
  options.le_override =
    parseAirfoilEdgeNode(p_xn_line, "leading_edge", context);
  options.te_override =
    parseAirfoilEdgeNode(p_xn_line, "trailing_edge", context);
  options.normalize = parseAirfoilNormalize(p_xn_line, context);

  if (options.data_form != "file") {
    throw std::runtime_error(
      "Unsupported airfoil data source '" + options.data_form + "' in "
      + context + " (only <points data=\"file\"> is supported)"
    );
  }

  std::string filename = trim(p_xn_pts->value());
  if (filename.empty()) {
    throw std::runtime_error(
      "Missing airfoil data file name in " + context + "/<points>"
    );
  }
  filename = config.file_directory + filename;

  // First read raw coordinate blocks, then normalize them into one closed
  // baseline regardless of the source file convention.
  std::vector<AirfoilSection> sections = readAirfoilSectionsFromFile(
    filename, options.head_rows, context
  );

  if (options.normalize) {
    const NormalizeTransform t = computeNormalizeTransform(sections, context);
    applyNormalizeToSections(sections, t);
    if (options.le_override.has_coord) {
      options.le_override.coord =
        applyNormalizeToCoord(options.le_override.coord, t);
    }
    if (options.te_override.has_coord) {
      options.te_override.coord =
        applyNormalizeToCoord(options.te_override.coord, t);
    }
  }

  AirfoilSection closed_airfoil;
  if (isAirfoilSeligFormat(options.format)) {
    if (sections.size() != 1) {
      throw std::runtime_error(
        "Selig airfoil data in " + context
        + " must contain exactly one continuous coordinate block"
      );
    }
    closed_airfoil = buildClosedSeligAirfoil(
      sections[0], options.le_override, options.te_override, tol, context
    );
  } else if (isAirfoilLednicerFormat(options.format)) {
    closed_airfoil = buildClosedLednicerAirfoil(
      sections, options.le_override, options.te_override, tol, context
    );
  } else {
    throw std::runtime_error(
      "Unsupported airfoil format '" + options.format + "' in " + context
    );
  }

  populateAirfoilBaseline(
    line, closed_airfoil, pmodel, options.le_override, options.te_override, tol
  );

  // Apply direction and post-processing transforms only after geometry
  // creation so Selig and Lednicer share the same downstream pipeline.
  // if (options.direction < 0) {
  //   line->reverse();
  // }
  if (options.flip.enabled) {
    applyAirfoilFlip(line, options.flip);
  }
  if (options.reverse) {
    line->reverse();
  }

  writeAirfoilBaselineDebugCsv(line);

  return 0;
}
