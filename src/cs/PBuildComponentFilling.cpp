#include "PComponent.hpp"

#include "PDCEL.hpp"
#include "PGeoClasses.hpp"
#include "PModel.hpp"
#include "curve.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "plog.hpp"

#include <cmath>
#include <list>
#include <sstream>

namespace {

static PDCELHalfEdgeLoop *findOutermostLoop(
    PDCELHalfEdgeLoop *loop, PDCEL *dcel)
{
  while (dcel->adjacentLoop(loop) != nullptr) {
    loop = dcel->adjacentLoop(loop);
  }
  return loop;
}




std::string faceLabel(PDCELFace *face, PModel *model) {
  if (face == nullptr) {
    return "nullptr";
  }

  std::ostringstream oss;
  oss << static_cast<void *>(face);
  std::string label = "face@" + oss.str();
  if (model != nullptr) {
    const std::string &name = model->faceData(face).name;
    if (!name.empty()) {
      label += " [" + name + "]";
    }
  }
  return label;
}




void logVertexEdgeRing(
    PDCELVertex *vertex, PModel *model, const std::string &prefix) {
  if (vertex == nullptr) {
    PLOG(error) << prefix << ": vertex=nullptr";
    return;
  }

  PLOG(error) << prefix << ": vertex=" << vertex->printString();
  if (vertex->edge() == nullptr) {
    PLOG(error) << prefix << ": edge ring is empty";
    return;
  }

  PDCELHalfEdge *start = vertex->edge();
  PDCELHalfEdge *he = start;
  int iter = 0;
  do {
    if (he == nullptr) {
      PLOG(error) << prefix << ": encountered nullptr half-edge in ring";
      return;
    }
    if (++iter > kDCELLoopHardCap) {
      PLOG(error) << prefix
                  << ": edge ring walk exceeded "
                  << kDCELLoopHardCap << " steps";
      return;
    }

    std::ostringstream line;
    line << prefix
         << ": outgoing[" << (iter - 1) << "] "
         << he->source()->printString()
         << " -> " << he->target()->printString()
         << " | face=" << faceLabel(he->face(), model)
         << " | loop=" << (he->loop() ? "set" : "nullptr");
    PLOG(error) << line.str();

    if (he->twin() == nullptr) {
      PLOG(error) << prefix << ": outgoing[" << (iter - 1)
                  << "] has nullptr twin";
      return;
    }
    he = he->twin()->next();
  } while (he != nullptr && he != start);

  if (he == nullptr) {
    PLOG(error) << prefix << ": edge ring terminated at nullptr next";
  }
}




void logMissingEnclosingLoop(
    const std::string &component_name, PDCELVertex *location,
    const BuilderConfig &bcfg) {
  PLOG(error) << "buildFilling: findEnclosingLoop returned nullptr"
              << " for component '" << component_name << "'"
              << ", fill location="
              << (location ? location->printString() : "nullptr")
              << ", loop_count="
              << bcfg.dcel->halfedgeloops().size();
  logVertexEdgeRing(
      location, bcfg.model,
      "buildFilling: fill-location edge ring for component '" + component_name
          + "'");
}




void logMissingHalfEdgeBetween(
    const std::string &component_name, PDCELVertex *source,
    PDCELVertex *target, const BuilderConfig &bcfg) {
  PLOG(error) << "buildFilling: findHalfEdgeBetween returned nullptr"
              << " for component '" << component_name << "'"
              << ", source="
              << (source ? source->printString() : "nullptr")
              << ", target="
              << (target ? target->printString() : "nullptr");
  logVertexEdgeRing(
      source, bcfg.model,
      "buildFilling: source edge ring for component '" + component_name + "'");
}

} // namespace

void PComponent::buildFilling(const BuilderConfig &bcfg) {
  FillingState &filling = _filling;
  PDCELHalfEdgeLoop *hel_out = nullptr;

  if (!filling.baseline_groups.empty()) {

    std::list<Baseline *> bl_closed;
    std::list<Baseline *> bl_open;

    // Join baselines
    Baseline *bl_joined;
    for (auto blg : filling.baseline_groups) {
      bl_joined = joinCurves(blg);
      if (bl_joined == nullptr) {
        PLOG(error) << "buildFilling: failed to join a filling baseline group"
                    << " for component '" << _name << "'";
        return;
      }

      if (filling.ref_baseline == blg.front()) {
        filling.ref_baseline = bl_joined;
      }

      if (bl_joined->vertices().front() == bl_joined->vertices().back()) {
        bl_closed.push_back(bl_joined);
      } else {
        bl_open.push_back(bl_joined);
      }
    }

    // std::cout << "\ncurve bl\n";
    // for (auto k = 0; k < bl->vertices().size(); k++) {
    //   std::cout << k << ": " << bl->vertices()[k] << std::endl;
    // }

    // Trim/Extend ends for each open baseline
    for (auto bl : bl_open) {
      double u1_head, u2_head = 0.0, u1_tail, u2_tail = 0.0, u1_tmp, u2_tmp;
      int ls_i_head = -1, ls_i_tmp;
      int ls_i_tail = static_cast<int>(bl->vertices().size());
      u1_head = -INF;
      u1_tail = INF;

      std::vector<PDCELVertex *> tmp_vertices;
      PDCELHalfEdge *he_tool_head = nullptr, *he_tool_tail = nullptr, *he;
      PDCELHalfEdgeLoop *hel_tool_head, *hel_tool_tail;

      for (auto hel : bcfg.dcel->halfedgeloops()) {
        if (!bcfg.dcel->isLoopKept(hel)) {
          // Assumption: open filling baselines are trimmed/extended only from
          // the leading segment at the head and the trailing segment at the
          // tail. This does not search the full polyline for the earliest
          // intersection; if internal vertices matter, this algorithm must be
          // upgraded to use the whole baseline rather than just the end span.
          tmp_vertices = {bl->vertices()[0], bl->vertices()[1]};
          he = findCurveLoopIntersection(tmp_vertices, hel, 0, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE);
          if (he != nullptr) {
            if (
              (ls_i_tmp == 0 && u1_tmp < 0 && u1_tmp > u1_head)  // before the first vertex
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp > ls_i_head)  // inner line segment
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp == ls_i_head && u1_tmp > u1_head) // same line segment but inner u
            ) {
              u1_head = u1_tmp;
              u2_head = u2_tmp;
              he_tool_head = he;
              hel_tool_head = hel;
            }
          }

          // Same limitation for the tail search: only the last baseline span
          // participates in the intersection test.
          tmp_vertices.clear();
          tmp_vertices = {bl->vertices()[bl->vertices().size() - 2], bl->vertices()[bl->vertices().size() - 1]};
          he = findCurveLoopIntersection(tmp_vertices, hel, 1, ls_i_tmp, u1_tmp, u2_tmp, TOLERANCE);
          if (he != nullptr) {
            if (
              ((ls_i_tmp == tmp_vertices.size() - 1) && u1_tmp > 1 && u1_tmp < u1_tail)  // after the first vertex
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp < ls_i_tail)  // inner line segment
              || ((fabs(u1_tmp) <= TOLERANCE || (u1_tmp > 0 && u1_tmp < 1) || fabs(1 - u1_tmp) <= TOLERANCE) && ls_i_tmp == ls_i_tail && u1_tmp < u1_tail) // same line segment but inner u
              ) {
              u1_tail = u1_tmp;
              u2_tail = u2_tmp;
              he_tool_tail = he;
              hel_tool_tail = hel;
            }
          }
        }
      }

      // std::cout << "        u1_head = " << u1_head << std::endl;
      // std::cout << "        he_tool_head = " << he_tool_head << std::endl;
      // std::cout << "        u1_tail = " << u1_tail << std::endl;
      // std::cout << "        he_tool_tail = " << he_tool_tail << std::endl;

      if (he_tool_head == nullptr) {
        PLOG(error) << "buildFilling: failed to find a head intersection"
                    << " for open filling baseline in component '"
                    << _name << "'";
        return;
      }

      if (he_tool_tail == nullptr) {
        PLOG(error) << "buildFilling: failed to find a tail intersection"
                    << " for open filling baseline in component '"
                    << _name << "'";
        return;
      }

      PDCELVertex *vnew;

      if (u2_head == 0) {
        vnew = he_tool_head->source();
      } else if (u2_head == 1) {
        vnew = he_tool_head->target();
      } else {
        PGeoLineSegment ls(he_tool_head->source(),
                           he_tool_head->target());
        vnew = ls.getParametricVertex(u2_head);
        vnew = bcfg.dcel->splitEdge(he_tool_head, vnew);
      }
      bl->vertices()[0] = vnew;

      if (u2_tail == 0) {
        vnew = he_tool_tail->source();
      } else if (u2_tail == 1) {
        vnew = he_tool_tail->target();
      } else {
        PGeoLineSegment ls(he_tool_tail->source(),
                           he_tool_tail->target());
        vnew = ls.getParametricVertex(u2_tail);
        vnew = bcfg.dcel->splitEdge(he_tool_tail, vnew);
      }
      bl->vertices()[bl->vertices().size() - 1] = vnew;

      // std::cout << "\ncurve bl\n";
      // for (auto k = 0; k < bl->vertices().size(); k++) {
      //   std::cout << k << ": " << bl->vertices()[k] << std::endl;
      // }

      // std::cout << "        curve _fill_ref_baseline:" << std::endl;
      // for (auto v : _fill_ref_baseline->vertices()) {
      //   // std::cout << "        " << v << std::endl;
      //   v->printWithAddress();
      // }

      bcfg.dcel->addEdgesFromCurve(bl->vertices());
    }

    for (auto bl : bl_closed) {
      bcfg.dcel->addEdgesFromCurve(bl->vertices());
    }

    bcfg.dcel->removeTempLoops();
    bcfg.dcel->createTempLoops();
    bcfg.dcel->linkHalfEdgeLoops();

    // bcfg.dcel->print_dcel();
  }
  if (filling.location != nullptr) {
    // The filling area is defined by a point The half edge loop
    // has been already created

    // Find the half edge loop enclosing the point
    bcfg.dcel->addVertex(filling.location);
    hel_out = bcfg.dcel->findEnclosingLoop(filling.location);
    // std::cout << "[debug] half edge loop hel_out:" << std::endl;
    // hel_out->print();
    bcfg.dcel->removeVertex(filling.location);

    if (hel_out == nullptr) {
      logMissingEnclosingLoop(_name, filling.location, bcfg);
      return;
    }
  }

  else {
    // The filling area is defined by the side of some baseline
    PDCELHalfEdge *he;
    he = bcfg.dcel->findHalfEdgeBetween(filling.ref_baseline->vertices()[0],
                                        filling.ref_baseline->vertices()[1]);

    if (he == nullptr) {
      logMissingHalfEdgeBetween(
          _name, filling.ref_baseline->vertices()[0],
          filling.ref_baseline->vertices()[1], bcfg);
      return;
    }

    // std::cout << "        half edge he:" << he << std::endl;

    if (filling.side == FillSide::right) {
      he = he->twin();
    }

    // Find the outer boundary
    hel_out = findOutermostLoop(he->loop(), bcfg.dcel);
  }




  // Keep the loop and create a new face
  if (hel_out == bcfg.dcel->halfedgeloops().front()) {
    PLOG(error) << "buildFilling: fill location is outside the shape"
                << " for component '" << _name << "'";
    return;
  }
  else {
    filling.face = bcfg.dcel->addFace(hel_out);
    if (filling.face == nullptr) {
      PLOG(error) << "buildFilling: failed to create fill face"
                  << " for component '" << _name << "'";
      return;
    }
    if (filling.layertype == nullptr) {
      PLOG(error) << "buildFilling: missing fill layer type"
                  << " for component '" << _name << "'";
      return;
    }
    bcfg.model->faceData(filling.face).name = _name + "_fill_face";
    filling.face->setMaterial(filling.material);
    bcfg.dcel->setLoopKept(hel_out, true);
    hel_out->setFace(filling.face);

    filling.face->setLayerType(filling.layertype);
    filling.face->setTheta1(filling.theta1);

    // Update all corresponding inner boundaries, if there are any
    bcfg.dcel->linkHalfEdgeLoops();
    for (auto heli : bcfg.dcel->halfedgeloops()) {
      if (!bcfg.dcel->isLoopKept(heli)) {
        // heli->print();
        PDCELHalfEdgeLoop *helj = findOutermostLoop(heli, bcfg.dcel);
        if (helj == hel_out) {
          bcfg.dcel->setLoopKept(heli, true);
          heli->setFace(filling.face);
          filling.face->addInnerComponent(heli->incidentEdge());
        }
      }
    }
    // _fill_face->print();
  }




  // Set local mesh size and embedded vertices in the property map.
  if (_mesh_size != -1) {
    PDCELFaceData &fd = bcfg.model->faceData(filling.face);
    fd.mesh_size = _mesh_size;
    for (auto v : _embedded_vertices) {
      fd.embedded_vertices.push_back(v);
    }
  }


}
