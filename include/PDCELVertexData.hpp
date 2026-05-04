#pragma once

// PDCELVertexData — Property Map for domain fields of a DCEL vertex.
//
// Domain-specific fields removed from PDCELVertex live here.
// Owned by PModel in an std::unordered_map<PDCELVertex*, PDCELVertexData>.
//
// Use forward declarations for all pointer types to avoid circular includes.

class Baseline;
class PDCELVertex;

/// Domain-specific data associated with a DCEL vertex.
/// Intended to live in std::unordered_map<PDCELVertex*, PDCELVertexData>
/// owned by PModel.
struct PDCELVertexData {
  /// The baseline this vertex lies on (if any).
  Baseline       *on_line = nullptr;
  /// If this point coincides with an existing vertex on a baseline, the
  /// authoritative vertex it should resolve to.
  PDCELVertex    *link_to = nullptr;
};
