// baseOffsetMapJson.cpp — see include/debug/baseOffsetMapJson.hpp.

#include "debug/baseOffsetMapJson.hpp"

#include "adaptive_thickness.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace dcel;  // phase 0: DCEL types moved to namespace dcel

namespace {

inline double snap(double v) {
  return std::fabs(v) < 1e-15 ? 0.0 : v;
}

std::string jsonString(const std::string& s) {
  // Minimal JSON string escape: backslash + quote only — segment names
  // in PreVABS are restricted to identifier-like characters so we don't
  // bother with full Unicode escaping.
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

}  // namespace

void dumpBaseOffsetMapJson(
    const std::string& path,
    const std::string& title,
    const std::vector<PDCELVertex*>& base,
    const std::vector<PDCELVertex*>& offset,
    const BaseOffsetMap& pairs,
    bool   closed,
    int    side,
    double dist,
    const std::vector<int>* dropped_base_ranges_lo,
    const std::vector<int>* dropped_base_ranges_hi,
    const prevabs::geo::LinearAdaptiveThicknessPlan* adaptive_plan) {
  std::ofstream os(path);
  if (!os) {
    PLOG(error) << "dumpBaseOffsetMapJson: cannot open '" << path
                << "' for write";
    return;
  }
  os << std::setprecision(17);

  os << "{\n";
  os << "  \"title\": "  << jsonString(title) << ",\n";
  os << "  \"closed\": " << (closed ? "true" : "false") << ",\n";
  os << "  \"side\": "   << side << ",\n";
  os << "  \"dist\": "   << snap(dist) << ",\n";

  auto write_xy_array = [&](const std::vector<PDCELVertex*>& vs) {
    os << "[";
    for (std::size_t i = 0; i < vs.size(); ++i) {
      if (i) os << ", ";
      auto* v = vs[i];
      if (v) {
        os << "[" << snap(v->y()) << ", " << snap(v->z()) << "]";
      } else {
        os << "null";
      }
    }
    os << "]";
  };

  os << "  \"base\": ";
  write_xy_array(base);
  os << ",\n";

  os << "  \"offset\": ";
  write_xy_array(offset);
  os << ",\n";

  os << "  \"pairs\": [";
  for (std::size_t i = 0; i < pairs.size(); ++i) {
    if (i) os << ", ";
    os << "[" << pairs[i].base << ", " << pairs[i].offset << "]";
  }
  os << "]";

  if (dropped_base_ranges_lo && dropped_base_ranges_hi
      && !dropped_base_ranges_lo->empty()) {
    os << ",\n  \"dropped_base_ranges_lo\": [";
    for (std::size_t i = 0; i < dropped_base_ranges_lo->size(); ++i) {
      if (i) os << ", ";
      os << (*dropped_base_ranges_lo)[i];
    }
    os << "],\n  \"dropped_base_ranges_hi\": [";
    for (std::size_t i = 0; i < dropped_base_ranges_hi->size(); ++i) {
      if (i) os << ", ";
      os << (*dropped_base_ranges_hi)[i];
    }
    os << "]";
  }

  if (adaptive_plan && adaptive_plan->ok) {
    os << ",\n  \"adaptive_thickness\": {\n";
    os << "    \"enabled\": true,\n";
    os << "    \"mode\": \"linear\",\n";
    os << "    \"repair_range\": ["
       << adaptive_plan->range.repair_lo << ", "
       << adaptive_plan->range.repair_hi << "],\n";
    os << "    \"taper_range\": ["
       << adaptive_plan->range.taper_lo << ", "
       << adaptive_plan->range.taper_hi << "],\n";
    os << "    \"design_half_thickness\": "
       << snap(adaptive_plan->range.design_half_thickness) << ",\n";
    os << "    \"repaired_half_thickness\": "
       << snap(adaptive_plan->range.repaired_half_thickness) << ",\n";
    os << "    \"safety\": "
       << snap(adaptive_plan->range.safety) << ",\n";
    os << "    \"stations\": [";
    for (std::size_t i = 0; i < adaptive_plan->stations.size(); ++i) {
      if (i) os << ", ";
      const auto& station = adaptive_plan->stations[i];
      os << "{\"base\": " << station.base_index
         << ", \"s\": " << snap(station.s)
         << ", \"half_thickness\": " << snap(station.half_thickness)
         << "}";
    }
    os << "]\n";
    os << "  }";
  }

  os << "\n}\n";
}
