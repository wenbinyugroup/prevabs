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




// XML-driven options collected for one airfoil baseline definition.
struct AirfoilReadOptions {
  std::string data_form = "file";
  std::string format = "2";
  int head_rows = 0;
  int direction = 1;
  bool reverse = false;
  AirfoilFlipOptions flip;
};




std::string airfoilContext(const Baseline *line) {
  return "airfoil baseline '" + line->getName() + "'";
}




int parseRequiredIntValue(
  const std::string &raw_value, const std::string &context
) {
  const std::string value = trim(raw_value);
  if (value.empty()) {
    throw std::runtime_error("Missing integer value in " + context);
  }

  std::size_t pos = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(value, &pos);
  } catch (const std::exception &) {
    throw std::runtime_error(
      "Invalid integer value '" + value + "' in " + context
    );
  }

  if (pos != value.size()) {
    throw std::runtime_error(
      "Invalid integer value '" + value + "' in " + context
    );
  }

  return parsed;
}




double parseRequiredDoubleValue(
  const std::string &raw_value, const std::string &context
) {
  const std::string value = trim(raw_value);
  if (value.empty()) {
    throw std::runtime_error("Missing numeric value in " + context);
  }

  std::size_t pos = 0;
  double parsed = 0.0;
  try {
    parsed = std::stod(value, &pos);
  } catch (const std::exception &) {
    throw std::runtime_error(
      "Invalid numeric value '" + value + "' in " + context
    );
  }

  if (pos != value.size()) {
    throw std::runtime_error(
      "Invalid numeric value '" + value + "' in " + context
    );
  }

  return parsed;
}




bool parseOptionalBoolNodeValue(
  const rapidxml::xml_node<> *parent, const char *name,
  const std::string &context
) {
  rapidxml::xml_node<> *node = parent->first_node(name);
  if (!node) {
    return false;
  }

  const std::string value = trim(node->value());
  if (value.empty()) {
    return true;
  }
  return parseXmlBoolValue(value, context + "/<" + name + ">");
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
    options.loc = parseRequiredDoubleValue(
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




bool isLeadingEdgeCoord(const AirfoilCoord &coord, double tol) {
  return std::fabs(coord.first) <= tol && std::fabs(coord.second) <= tol;
}




bool isTrailingEdgeCoord(const AirfoilCoord &coord, double tol) {
  return std::fabs(coord.first - 1.0) <= tol
         && std::fabs(coord.second) <= tol;
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
  const AirfoilSection &section, double tol, const std::string &context
) {
  if (section.size() < 2) {
    throw std::runtime_error(
      "Selig airfoil data in " + context
      + " must contain at least two coordinate rows"
    );
  }

  // Selig files are expected to walk TE -> upper -> LE -> lower -> TE.
  // Some standard datasets do not include an exact (0, 0) leading-edge row,
  // so detect the LE neighborhood from the minimum chordwise coordinate.
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

  // When several samples share the minimum x-value, prefer the first
  // non-positive y-value as the hand-off to the lower surface.
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
    lower_start = min_end + 1;
  }

  if (lower_start == 0 || lower_start >= section.size()) {
    throw std::runtime_error(
      "Selig airfoil data in " + context
      + " does not contain a usable lower surface after the leading edge"
    );
  }

  // Normalize the output to one closed baseline that always starts at TE.
  AirfoilSection closed;
  appendAirfoilCoord(closed, AirfoilCoord(1.0, 0.0), tol);

  for (std::size_t i = 0; i < lower_start; ++i) {
    const AirfoilCoord &coord = section[i];
    if (isTrailingEdgeCoord(coord, tol)) {
      continue;
    }
    if (i >= min_begin && i <= min_end) {
      continue;
    }
    if (isLeadingEdgeCoord(coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  // Insert one canonical LE vertex even if the source data only approaches it.
  appendAirfoilCoord(closed, AirfoilCoord(0.0, 0.0), tol);

  for (std::size_t i = lower_start; i < section.size(); ++i) {
    const AirfoilCoord &coord = section[i];
    if (isTrailingEdgeCoord(coord, tol) || isLeadingEdgeCoord(coord, tol)) {
      continue;
    }
    appendAirfoilCoord(closed, coord, tol);
  }

  appendAirfoilCoord(closed, AirfoilCoord(1.0, 0.0), tol);
  return closed;
}




AirfoilSection buildClosedLednicerAirfoil(
  const std::vector<AirfoilSection> &sections, double tol,
  const std::string &context
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

  // Lednicer stores both surfaces as LE -> TE, so reverse the upper block
  // to produce the same normalized TE -> upper -> LE -> lower -> TE order.
  AirfoilSection closed;
  appendAirfoilCoord(closed, AirfoilCoord(1.0, 0.0), tol);

  bool found_le = false;
  for (AirfoilSection::const_reverse_iterator it = upper.rbegin();
       it != upper.rend(); ++it) {
    if (isTrailingEdgeCoord(*it, tol)) {
      continue;
    }
    if (isLeadingEdgeCoord(*it, tol)) {
      appendAirfoilCoord(closed, AirfoilCoord(0.0, 0.0), tol);
      found_le = true;
      continue;
    }
    appendAirfoilCoord(closed, *it, tol);
  }

  if (!found_le) {
    throw std::runtime_error(
      "Lednicer airfoil data in " + context
      + " does not contain a leading edge in the upper surface block"
    );
  }

  for (std::size_t i = 0; i < lower.size(); ++i) {
    if (isLeadingEdgeCoord(lower[i], tol) || isTrailingEdgeCoord(lower[i], tol)) {
      continue;
    }
    appendAirfoilCoord(closed, lower[i], tol);
  }

  appendAirfoilCoord(closed, AirfoilCoord(1.0, 0.0), tol);
  return closed;
}




std::string makeAirfoilVertexName(
  const Baseline *line, const std::string &suffix
) {
  // Generated names stay local to the baseline and avoid repository collisions.
  return line->getName() + "_" + suffix;
}




void populateAirfoilBaseline(
  Baseline *line, const AirfoilSection &coords, PModel *pmodel, double tol
) {
  PDCELVertex *pv_le = new PDCELVertex(
    makeAirfoilVertexName(line, "le"), 0.0, 0.0, 0.0
  );
  PDCELVertex *pv_te = new PDCELVertex(
    makeAirfoilVertexName(line, "te"), 0.0, 1.0, 0.0
  );

  pmodel->addVertex(pv_le);
  pmodel->addVertex(pv_te);

  int generated_point_count = 0;
  for (std::size_t i = 0; i < coords.size(); ++i) {
    // Reuse canonical LE/TE vertices and create named interior samples for
    // all remaining coordinates on the normalized polyline.
    PDCELVertex *vertex = nullptr;
    if (isLeadingEdgeCoord(coords[i], tol)) {
      vertex = pv_le;
    } else if (isTrailingEdgeCoord(coords[i], tol)) {
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
    options.head_rows = parseRequiredIntValue(
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
  const std::vector<AirfoilSection> sections = readAirfoilSectionsFromFile(
    filename, options.head_rows, context
  );

  AirfoilSection closed_airfoil;
  if (isAirfoilSeligFormat(options.format)) {
    if (sections.size() != 1) {
      throw std::runtime_error(
        "Selig airfoil data in " + context
        + " must contain exactly one continuous coordinate block"
      );
    }
    closed_airfoil = buildClosedSeligAirfoil(sections[0], tol, context);
  } else if (isAirfoilLednicerFormat(options.format)) {
    closed_airfoil = buildClosedLednicerAirfoil(sections, tol, context);
  } else {
    throw std::runtime_error(
      "Unsupported airfoil format '" + options.format + "' in " + context
    );
  }

  populateAirfoilBaseline(line, closed_airfoil, pmodel, tol);

  // Apply direction and post-processing transforms only after geometry
  // creation so Selig and Lednicer share the same downstream pipeline.
  if (options.direction < 0) {
    line->reverse();
  }
  if (options.flip.enabled) {
    applyAirfoilFlip(line, options.flip);
  }
  if (options.reverse) {
    line->reverse();
  }

  return 0;
}
