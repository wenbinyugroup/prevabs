#pragma once

// baseOffsetMapSvg.hpp
// Tiny debug helper: render a base curve, its offset curve, and the
// staircase BaseOffsetMap correspondence as a self-contained SVG file.
//
// The point is to make the discrete (base_i -> offset_j) mapping visually
// obvious — repeated base indices show up as a fan of mapping lines meeting
// at one base vertex, repeated offset indices show as a fan meeting at one
// offset vertex, and dropped ranges show up as gaps along the base curve.

#include "geo_types.hpp"

#include <string>
#include <vector>

class PDCELVertex;

// Write an SVG visualizing (base curve, offset curve, staircase pairs).
// Coordinate convention: PreVABS cross-section plane is y-z. The SVG uses
// svg_x = y and svg_y = -z so that +z points up on screen.
//
// path   absolute or working-dir-relative output file path
// title  short label drawn at the top of the SVG (e.g. segment name)
// base   base curve vertices (closed: front() == back() is honoured)
// offset offset curve vertices
// pairs  staircase BaseOffsetMap; one polyline per pair connects
//        base[p.base] to offset[p.offset]
// offset_resampled
//        Optional per-offset-vertex origin tag (may be null or empty,
//        in which case every offset vertex is rendered as "raw"). Must
//        be parallel to `offset` when non-null. `false` = raw Clipper2
//        corner/cap, `true` = synthesized at a base-vertex foot of
//        perpendicular by the open-path resample step. The renderer
//        draws raw vertices as filled circles and resampled vertices
//        as hollow squares so the staircase fan structure can be
//        attributed to either layer.
// pre_resample_raw_points
//        Optional debug overlay: Clipper2 raw run vertices snapshot
//        at the moment immediately before the open-path resample step
//        replaced them. Drawn as a topmost scatter layer (small red
//        dots, no connecting line) so the viewer can see where
//        Clipper2 originally placed vertices in runs that the
//        resample wholesale-replaced. Empty / null → layer skipped.
void dumpBaseOffsetMapSvg(const std::string& path,
                          const std::string& title,
                          const std::vector<PDCELVertex*>& base,
                          const std::vector<PDCELVertex*>& offset,
                          const BaseOffsetMap& pairs,
                          const std::vector<bool>* offset_resampled = nullptr,
                          const std::vector<SPoint2>* pre_resample_raw_points = nullptr);
