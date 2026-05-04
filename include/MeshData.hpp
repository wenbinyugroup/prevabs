#pragma once

class PMesh;

#include <cstddef>
#include <vector>

/**
 * Owns post-Gmsh mesh data: the mesh object, node-element adjacency, and counts.
 *
 * Extracted from PModel as part of the Step 6 God-object split.
 */
struct MeshData {
  PMesh* mesh = nullptr;
  std::vector<std::vector<int>> node_elements;
  std::size_t num_nodes    = 0;
  std::size_t num_elements = 0;
};
