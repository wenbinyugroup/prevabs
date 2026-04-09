#pragma once

// PDCELFaceData — Property Map migration foundation
//
// The classical DCEL face should carry only topology:
//   PDCELHalfEdge *_outer;
//   std::vector<PDCELHalfEdge *> _inners;
//   bool _is_bounded;
//
// Domain-specific fields are migrated here incrementally (one at a time):
//   1. Add field to PDCELFaceData.
//   2. Add std::unordered_map<PDCELFace *, PDCELFaceData> _face_data to PModel.
//   3. Remove the field from PDCELFace, fixing compilation errors.
//   4. Update all call sites to use bcfg.model->faceData(f).field.
//
// Keep this header self-contained: use forward declarations for all pointer
// types to avoid circular include chains (PArea.hpp → utilities.hpp →
// PModel.hpp → PDCELFaceData.hpp → PArea.hpp).

// Forward declarations — all members below are raw pointers or value types.
class LayerType;
class Material;
class PArea;
class PDCELVertex;

#include "geo_types.hpp"  // SVector3

#include <string>
#include <vector>

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
