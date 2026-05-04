#pragma once

class Baseline;
class PDCELVertex;

#include <string>
#include <unordered_map>
#include <vector>

/**
 * Owns named PDCELVertex points and Baseline curves with O(1) name lookups.
 *
 * Extracted from PModel as part of the Step 6 God-object split.
 */
class GeometryRepository {
public:
  // -----------------------------------------------------------------
  // Adders

  void addVertex(PDCELVertex* v);
  void addBaseline(Baseline* l);

  // -----------------------------------------------------------------
  // Getters

  PDCELVertex* getPointByName(const std::string& name) const;
  Baseline*    getBaselineByName(const std::string& name) const;
  /// Returns a heap-allocated copy of the named baseline, or nullptr.
  Baseline*    getBaselineByNameCopy(const std::string& name) const;

  // -----------------------------------------------------------------
  // Ordered access (preserves insertion order)

  std::vector<PDCELVertex*>& vertices()  { return _vertices; }
  std::vector<Baseline*>&    baselines() { return _baselines; }

private:
  std::vector<PDCELVertex*> _vertices;
  std::vector<Baseline*>    _baselines;

  std::unordered_map<std::string, PDCELVertex*> _vertex_map;
  std::unordered_map<std::string, Baseline*>    _baseline_map;
};
