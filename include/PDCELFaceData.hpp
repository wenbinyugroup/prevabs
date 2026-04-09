#pragma once

// PDCELFaceData — Property Map migration foundation
//
// The classical DCEL face should carry only topology:
//   PDCELHalfEdge *_outer;
//   std::vector<PDCELHalfEdge *> _inners;
//
// All domain-specific fields currently in PDCELFace belong here.
// Migration path (incremental — one field at a time):
//   1. Add field to PDCELFaceData.
//   2. Add std::unordered_map<PDCELFace *, PDCELFaceData> face_data to PModel.
//   3. Remove the field from PDCELFace, fixing compilation errors.
//   4. Update all call sites to use pmodel->face_data[f].field.
//
// Fields already migrated: (none yet — this is the foundation)

#include "Material.hpp"
#include "PArea.hpp"
#include "geo_types.hpp"
#include "globalConstants.hpp"

#include <string>
#include <vector>

class PDCELVertex;
class LayerType;

/// All domain-specific data associated with a DCEL face.
/// Intended to live in an std::unordered_map<PDCELFace *, PDCELFaceData>
/// owned by PModel (or the CS builder layer).
struct PDCELFaceData {
  std::string     name;
  PArea          *area      = nullptr;
  Material       *material  = nullptr;
  LayerType      *layertype = nullptr;
  double          theta1    = 0.0;
  double          theta3    = 0.0;
  SVector3        y1{1, 0, 0};
  SVector3        y2{0, 1, 0};
  double          mesh_size = -1.0;
  std::vector<PDCELVertex *> embedded_vertices;
};
