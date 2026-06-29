#include "PModel.hpp"

#include "PDCELFace.hpp"
#include "PDCELUtils.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "GmshApi.hpp"

// #include "gmsh/GEdge.h"
// #include "gmsh/GEdgeLoop.h"
// #include "gmsh/GEntity.h"
// #include "gmsh/GFace.h"
// #include "gmsh/GModel.h"
// #include "gmsh/GVertex.h"
// #include "gmsh/Gmsh.h"
// #include "gmsh/MElement.h"
// #include "gmsh/MTriangle.h"

#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <utility>

namespace {

const double kGmshFailFastMaxRetries = 1.0;
const double kGmshMeshingHardTimeoutSeconds = 12.0;
const int kGmshEdgeRecoveryWarningLimit = 3;

bool isGmshEdgeRecoveryMessage(const std::string &msg) {
  return msg.find("Impossible to recover edge") != std::string::npos
      || msg.find("Unable to recover the edge") != std::string::npos
      || msg.find("recover the edge") != std::string::npos
      || msg.find("error tag -1") != std::string::npos;
}

bool hasGmshEdgeRecoveryFailure(const std::vector<std::string> &log) {
  for (const auto &msg : log) {
    if (isGmshEdgeRecoveryMessage(msg)) return true;
  }
  return false;
}

int countGmshEdgeRecoveryMessages(const std::vector<std::string> &log) {
  int count = 0;
  for (const auto &msg : log) {
    if (isGmshEdgeRecoveryMessage(msg)) ++count;
  }
  return count;
}

std::string gmshEvidenceSnippet(const std::vector<std::string> &log) {
  std::vector<std::string> evidence;
  for (const auto &msg : log) {
    if (isGmshEdgeRecoveryMessage(msg)
        || msg.find("Error") != std::string::npos) {
      evidence.push_back(msg);
    }
  }
  if (evidence.empty()) return "";

  const std::size_t max_lines = 6;
  const std::size_t first =
      evidence.size() > max_lines ? evidence.size() - max_lines : 0;
  std::ostringstream oss;
  for (std::size_t i = first; i < evidence.size(); ++i) {
    if (i != first) oss << " | ";
    oss << evidence[i];
  }
  return oss.str();
}

std::string gmshMeshContext(double global_mesh_size, double elapsed_s) {
  std::vector<std::pair<int, int>> ents;
  try { gmsh::model::getEntities(ents, 2); } catch (...) {}

  std::ostringstream oss;
  oss << "faces=" << ents.size()
      << ", global_mesh_size=" << global_mesh_size
      << ", elapsed_s=" << elapsed_s;
  return oss.str();
}

void startGmshLoggerBestEffort(bool &started) {
  started = false;
  try {
    gmsh::logger::start();
    started = true;
  } catch (...) {}
}

void collectAndStopGmshLoggerBestEffort(
    bool started, std::vector<std::string> &log) {
  if (!started) return;
  try { gmsh::logger::get(log); } catch (...) {}
  try { gmsh::logger::stop(); } catch (...) {}
}

class GmshMeshingWatchdog {
public:
  GmshMeshingWatchdog(bool logger_started, double global_mesh_size)
      : _done(false), _logger_started(logger_started),
        _global_mesh_size(global_mesh_size), _face_count(0) {
    std::vector<std::pair<int, int>> ents;
    try { gmsh::model::getEntities(ents, 2); } catch (...) {}
    _face_count = ents.size();
    _worker = std::thread([this]() { run(); });
  }

  ~GmshMeshingWatchdog() {
    _done.store(true);
    if (_worker.joinable()) _worker.join();
  }

private:
  std::atomic<bool> _done;
  bool _logger_started;
  double _global_mesh_size;
  std::size_t _face_count;
  std::thread _worker;

  void failFromWatchdog(
      const std::string &reason, double elapsed_s,
      const std::vector<std::string> &log) {
    const std::string evidence = gmshEvidenceSnippet(log);
    std::ostringstream context;
    context << "faces=" << _face_count
            << ", global_mesh_size=" << _global_mesh_size
            << ", elapsed_s=" << elapsed_s;
    const std::string msg =
        "fatal exception: gmsh mesh generation failed: " + reason
        + " (" + context.str() + ")"
        + (evidence.empty() ? "" : " | gmsh log: " + evidence);

    PLOG(error) << msg;
    flushPrevabsLoggers();
    std::cerr << "xx  " << msg << std::endl;
    std::exit(1);
  }

  void run() {
    const auto t0 = std::chrono::steady_clock::now();
    while (!_done.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      const auto now = std::chrono::steady_clock::now();
      const double elapsed_s =
          std::chrono::duration<double>(now - t0).count();

      std::vector<std::string> log;
      if (_logger_started) {
        try { gmsh::logger::get(log); } catch (...) {}
        if (countGmshEdgeRecoveryMessages(log)
            >= kGmshEdgeRecoveryWarningLimit) {
          failFromWatchdog(
              "repeated edge recovery warnings during meshing",
              elapsed_s, log);
        }
      }

      if (elapsed_s >= kGmshMeshingHardTimeoutSeconds) {
        failFromWatchdog(
            "meshing exceeded hard time limit of "
            + std::to_string(kGmshMeshingHardTimeoutSeconds)
            + " seconds",
            elapsed_s, log);
      }
    }
  }
};

void generateMesh2DWithDiagnostics(double global_mesh_size) {
  bool logger_started = false;
  std::vector<std::string> log;
  startGmshLoggerBestEffort(logger_started);

  const auto t0 = std::chrono::steady_clock::now();
  try {
    gmsh::option::setNumber("General.AbortOnError", 3);
    gmsh::option::setNumber("Mesh.MaxRetries", kGmshFailFastMaxRetries);
  } catch (...) {}

  try {
    GmshMeshingWatchdog watchdog(logger_started, global_mesh_size);
    gmsh::model::mesh::generate(2);
  } catch (const std::exception &e) {
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed_s =
        std::chrono::duration<double>(t1 - t0).count();
    collectAndStopGmshLoggerBestEffort(logger_started, log);

    std::string reason = e.what();
    std::string last_error;
    try { gmsh::logger::getLastError(last_error); } catch (...) {}
    if (!last_error.empty() && reason.find(last_error) == std::string::npos) {
      reason += "; last_error=" + last_error;
    }

    const std::string evidence = gmshEvidenceSnippet(log);
    throw std::runtime_error(
        "gmsh mesh generation failed: " + reason
        + " (" + gmshMeshContext(global_mesh_size, elapsed_s) + ")"
        + (evidence.empty() ? "" : " | gmsh log: " + evidence));
  }

  const auto t1 = std::chrono::steady_clock::now();
  const double elapsed_s =
      std::chrono::duration<double>(t1 - t0).count();
  collectAndStopGmshLoggerBestEffort(logger_started, log);

  if (hasGmshEdgeRecoveryFailure(log)) {
    const std::string evidence = gmshEvidenceSnippet(log);
    throw std::runtime_error(
        "gmsh mesh generation failed: edge recovery reported failure"
        + std::string(" (") + gmshMeshContext(global_mesh_size, elapsed_s)
        + ")"
        + (evidence.empty() ? "" : " | gmsh log: " + evidence));
  }
}

}  // namespace





void PModel::createGmshVertices() {

    PLOG(info) << "creating gmsh vertices";

  int _gv_tag;

  // Distinct DCEL vertices that are geometrically coincident must share a
  // single Gmsh point, otherwise Gmsh meshes each separately and any element
  // spanning the coincident pair is degenerate (zero area -> negative Jacobian,
  // rejected by VABS). Such coincident pairs arise where two adjacent
  // sub-segments build their per-layer interface vertices independently on a
  // shared end-cap (e.g. the c-spar and web both end at p3/p4), and — for open
  // layered segments — where the final layer's arc-resampled shell point lands
  // on top of an existing shell vertex. These independent interpolations land
  // ~1e-8 apart (measured up to ~1e-6), at or just above VERTEX_MERGE_TOL, so
  // the DCEL's own mid-build coincidence-merge does not unify them. We cannot
  // widen that mid-build merge (the area-build code is not robust to its
  // vertices merging), so the sharing is realized here, at the export boundary,
  // where the DCEL is already complete and a wider EXPORT_MERGE_TOL is safe: it
  // sits far above the coincident gaps yet far below the smallest intentional
  // feature (the resample min-separation, ~1e-5, and any real geometry, ~1e-3).
  // Created points are recorded so coincident followers reuse the same tag.
  const double EXPORT_MERGE_TOL = 2.0e-6;
  struct CreatedPoint { double x, y, z; int tag; };
  std::vector<CreatedPoint> created;

  auto tagForLocation = [&](PDCELVertex *v) -> int {
    const double vx = v->x(), vy = v->y(), vz = v->z();
    for (const auto &c : created) {
      const double dx = c.x - vx, dy = c.y - vy, dz = c.z - vz;
      if (std::sqrt(dx * dx + dy * dy + dz * dz) <= EXPORT_MERGE_TOL) {
        return c.tag;
      }
    }
    int tag = gmsh::model::geo::addPoint(vx, vy, vz, _global_mesh_size);
    created.push_back({vx, vy, vz, tag});
    return tag;
  };

  for (auto v : _dcel->vertices()) {

    if (v->isFinite()) {
      _gv_tag = tagForLocation(v);
      _gmsh_vertex_tags[v] = _gv_tag;
            PLOG_DEBUG_AT(geo) <<
        "  vertex " + v->printString()
        + " -> gmsh tag " + std::to_string(_gv_tag);
    }
    else {
            PLOG_DEBUG_AT(geo) <<
        "  vertex " + v->printString() + " skipped (not finite)";
    }

  }

  return;

}









void PModel::recordInterface(PDCELHalfEdge *he) {

  if (_joint_halfedges.count(he) == 0 && _joint_halfedges.count(he->twin()) == 0) {

    if (he->face() != nullptr && he->twin()->face() != nullptr) {
      // std::cout << he->face()->material() << std::endl;
      // std::cout << he->twin()->face()->material() << std::endl;

      if (he->face()->material() != nullptr && he->twin()->face()->material() != nullptr) {

        int mid_f1 = he->face()->material()->id();
        int mid_f2 = he->twin()->face()->material()->id();
        double theta1_f1 = he->face()->theta1();
        double theta1_f2 = he->twin()->face()->theta1();
        double theta3_f1 = he->face()->theta3();
        double theta3_f2 = he->twin()->face()->theta3();

        // std::cout << "mid_f1 = " << mid_f1 << ", mid_f2 = " << mid_f2 << std::endl;
        // std::cout << "theta1_f1 = " << theta1_f1 << ", theta1_f2 = " << theta1_f2 << std::endl;
        // std::cout << "theta3_f1 = " << theta3_f1 << ", theta3_f2 = " << theta3_f2 << std::endl;

        // Check interface criteria
        // std::cout << "checking interface criteria..." << std::endl;
        bool is_interface = false;
        if (mid_f1 != mid_f2) {
          is_interface = true;
        }
        else {
          // std::cout << "compare theta3" << std::endl;
          // std::cout << std::abs(theta3_f1 - theta3_f2) << std::endl;
          // std::cout << _itf_theta3_diff_th << std::endl;
          if (std::abs(theta3_f1 - theta3_f2) > _itf_theta3_diff_th) {
            is_interface = true;
          }
          else {
            // std::cout << "compare theta1" << std::endl;
            // std::cout << std::abs(theta1_f1 - theta1_f2) << std::endl;
            // std::cout << _itf_theta1_diff_th << std::endl;
            if (std::abs(theta1_f1 - theta1_f2) > _itf_theta1_diff_th) {
              is_interface = true;
            }
          }
        }

        if (is_interface) {

          bool found = false;

          unsigned int i_itf=0;

          for (i_itf=0; i_itf<_itf_material_pairs.size(); i_itf++) {

            int mat_p1f1 = _itf_material_pairs[i_itf].first;
            int mat_p1f2 = _itf_material_pairs[i_itf].second;
            double th1_p1f1 = _itf_theta1_pairs[i_itf].first;
            double th1_p1f2 = _itf_theta1_pairs[i_itf].second;
            double th3_p1f1 = _itf_theta3_pairs[i_itf].first;
            double th3_p1f2 = _itf_theta3_pairs[i_itf].second;

            if ((mid_f1 == mat_p1f1 && mid_f2 == mat_p1f2) || (mid_f2 == mat_p1f1 && mid_f1 == mat_p1f2)) {
              if ((theta1_f1 == th1_p1f1 && theta1_f2 == th1_p1f2) || (theta1_f2 == th1_p1f1 && theta1_f1 == th1_p1f2)) {
                if ((theta3_f1 == th3_p1f1 && theta3_f2 == th3_p1f2) || (theta3_f2 == th3_p1f1 && theta3_f1 == th3_p1f2)) {
                  found = true;
                  break;
                }
              }
            }
          }

          // std::cout << "found = " << found << std::endl;

          if (found) {
            // std::cout << "i_itf = " << i_itf << std::endl;
            _itf_halfedges[i_itf].push_back(he);
          }

          else {
            std::pair<int, int> mat_pair{mid_f1, mid_f2};
            std::pair<double, double> th1_pair{theta1_f1, theta1_f2};
            std::pair<double, double> th3_pair{theta3_f1, theta3_f2};

            _itf_material_pairs.push_back(mat_pair);
            _itf_theta1_pairs.push_back(th1_pair);
            _itf_theta3_pairs.push_back(th3_pair);

            std::vector<PDCELHalfEdge *> itf_hes{he};
            _itf_halfedges.push_back(itf_hes);
          }

          _joint_halfedges.insert(he);
        }

      }
    }
  }

  return;
}









void PModel::createGmshEdges() {

    PLOG(info) << "creating gmsh edges";

  int _ge_tag;

  for (auto he : _dcel->halfedges()) {

    if (he->isFinite()) {

      // Check if the Gmsh edge for the twin has been created already
      auto it_twin = _gmsh_edge_tags.find(he->twin());
      _ge_tag = (it_twin != _gmsh_edge_tags.end()) ? it_twin->second : 0;

      if (_ge_tag == 0) {

        // Resolve the endpoint Gmsh tags from the positively-signed half-edge.
        PDCELHalfEdge *he_pos = (he->sign() > 0) ? he : he->twin();
        int src_tag = _gmsh_vertex_tags[he_pos->source()];
        int tgt_tag = _gmsh_vertex_tags[he_pos->target()];
                  PLOG_DEBUG_AT(geo) <<
          "  he " + he_pos->printString()
          + " | src_tag=" + std::to_string(src_tag)
          + " tgt_tag=" + std::to_string(tgt_tag);
        if (src_tag == 0 || tgt_tag == 0) {
          PLOG(error) << "  vertex tag is 0 for he " + he_pos->printString()
            + " | src=" + he_pos->source()->printString()
            + " (tag=" + std::to_string(src_tag) + ")"
            + " | tgt=" + he_pos->target()->printString()
            + " (tag=" + std::to_string(tgt_tag) + ")";
        }

        // Endpoints merged to the same Gmsh point: this is a zero-length riser
        // (the cap-step between two coincident layer-interface vertices). Emit
        // no Gmsh line and mark both half-edges collapsed so face-loop assembly
        // skips them; the loop stays closed because both endpoints are the same
        // point.
        if (src_tag == tgt_tag) {
          _gmsh_collapsed_halfedges.insert(he);
          _gmsh_collapsed_halfedges.insert(he->twin());
                    PLOG_DEBUG_AT(geo) <<
            "  he " + he->printString() + " collapsed (coincident endpoints)";
          continue;
        }

        _ge_tag = gmsh::model::geo::addLine(src_tag, tgt_tag);

        // Interface
        if (_itf_output) {

          recordInterface(he);

        }

      }
      _gmsh_edge_tags[he] = _ge_tag;
    }
    else {
            PLOG_DEBUG_AT(geo) <<
        "  he " + he->printString() + " skipped (not finite)";
    }
  }
}









void PModel::createGmshFaces() {

    PLOG(info) << "creating gmsh faces";

  int _ge_tag;  // gmsh edge tag
  int _gel_tag;  // gmsh edge loop tag
  int _gf_tag;  // gmsh face tag

  for (auto f : _dcel->faces()) {

        PLOG_DEBUG_AT(geo) << "";
        PLOG_DEBUG_AT(geo) << "  face: " + _face_data[f].name;

    if (f->isBounded() && f->outer() != nullptr) {

      // Vector of edge tags of a loop
      std::vector<int> _ge_tags;

      // Vector of edge loop tags
      std::vector<int> _geloop_tags;


      // Add outer loop
            PLOG_DEBUG_AT(geo) << "  adding outer loop";
      walkLoopWithLimit(f->outer(), [&](PDCELHalfEdge *he) {
        // Skip zero-length risers collapsed by vertex dedup: no Gmsh line
        // exists for them and the loop stays closed without them.
        if (_gmsh_collapsed_halfedges.count(he)) {
          return;
        }
        auto it_he = _gmsh_edge_tags.find(he);
        int _tag = (it_he != _gmsh_edge_tags.end()) ? it_he->second : 0;
        if (_tag == 0) {
          auto it_tw = _gmsh_edge_tags.find(he->twin());
          _tag = (it_tw != _gmsh_edge_tags.end()) ? it_tw->second : 0;
        }
        if (he->sign() < 0) {
          _tag = -1 * _tag;
        }
        _ge_tags.push_back(_tag);
      });
      _gel_tag = gmsh::model::geo::addCurveLoop(_ge_tags);
      _geloop_tags.push_back(_gel_tag);


      // Add inner loops
            PLOG_DEBUG_AT(geo) << "  adding inner loops";
      for (auto hei : f->inners()) {

        _ge_tags.clear();
        walkLoopWithLimit(hei, [&](PDCELHalfEdge *he) {
          if (_gmsh_collapsed_halfedges.count(he)) {
            return;
          }
          auto it_he = _gmsh_edge_tags.find(he);
          int _tag = (it_he != _gmsh_edge_tags.end()) ? it_he->second : 0;
          if (_tag == 0) {
            auto it_tw = _gmsh_edge_tags.find(he->twin());
            _tag = (it_tw != _gmsh_edge_tags.end()) ? -(it_tw->second) : 0;
          }
          _ge_tags.push_back(_tag);
        });
        _gel_tag = gmsh::model::geo::addCurveLoop(_ge_tags);
        _geloop_tags.push_back(_gel_tag);

      }

      _gf_tag = gmsh::model::geo::addPlaneSurface(_geloop_tags);
      _gmsh_face_tags[f] = _gf_tag;


      // Create embedded entities and set local mesh sizes
            PLOG_DEBUG_AT(geo) << "  adding local mesh size";
      if (_face_data[f].mesh_size != -1) {
        int _gv_tag_prev = 0;
        int _gv_tag_curr = 0;

        for (auto v : _face_data[f].embedded_vertices) {

          _gv_tag_curr = gmsh::model::geo::addPoint(
            v->x(), v->y(), v->z(), _face_data[f].mesh_size);
          _gmsh_face_embedded_vertex_tags[f].push_back(_gv_tag_curr);

          // If there are more than one embedded vertices, add embedded edges
          if (_gv_tag_prev != 0) {
            _ge_tag = gmsh::model::geo::addLine(_gv_tag_prev, _gv_tag_curr);
            _gmsh_face_embedded_edge_tags[f].push_back(_ge_tag);
          }

          _gv_tag_prev = _gv_tag_curr;

        }
      }
    }
  }
}









void PModel::createGmshEmbeddedEntities() {

    PLOG_DEBUG_AT(geo) << "  adding embedded entities";
  for (auto f : _dcel->faces()) {

        PLOG_DEBUG_AT(geo) << "";
        PLOG_DEBUG_AT(geo) << "  face: " + _face_data[f].name;

    auto it_ev = _gmsh_face_embedded_vertex_tags.find(f);
    if (it_ev != _gmsh_face_embedded_vertex_tags.end() && !it_ev->second.empty()) {
      gmsh::model::mesh::embed(0, it_ev->second, 2, _gmsh_face_tags[f]);

      auto it_ee = _gmsh_face_embedded_edge_tags.find(f);
      if (it_ee != _gmsh_face_embedded_edge_tags.end() && !it_ee->second.empty()) {
        gmsh::model::mesh::embed(1, it_ee->second, 2, _gmsh_face_tags[f]);
      }
    }
  }

  return;
}









void PModel::createGmshPhyscialGroups() {

    PLOG(info) << "creating physical groups";

  std::vector<int> group_tags;
  std::vector<std::vector<int>> group_face_tags;

  for (auto f : _dcel->faces()) {

        PLOG_DEBUG_AT(geo) << "";
        PLOG_DEBUG_AT(geo) << "  face: " + _face_data[f].name;

    if (f->isBounded() && f->outer() != nullptr) {

            PLOG_DEBUG_AT(geo) <<
        "  id: " + std::to_string(f->layertype()->id())
        + ", material: " + f->layertype()->material()->getName()
        + ", theta_3 = " + std::to_string(f->layertype()->angle())
      ;

      // Check if the group has been created
      int index = -1;
      for (auto i=0; i<group_tags.size(); i++) {
        if (group_tags[i] == f->layertype()->id()) {
          index = i;
          break;
        }
      }

      // If not found, create a new group
      if (index == -1) {
        group_tags.push_back(f->layertype()->id());
        group_face_tags.push_back(std::vector<int>());
        index = static_cast<int>(group_tags.size()) - 1;
      }

      // Add face tag to the group
      group_face_tags[index].push_back(_gmsh_face_tags[f]);

      _gmsh_face_physical_group_tags[f] = f->layertype()->id();

    }
  }

  // Create physical groups
  for (auto i=0; i<group_tags.size(); i++) {

    gmsh::model::addPhysicalGroup(2, group_face_tags[i], group_tags[i]);

  }

  return;
}









void PModel::createGmshGeo() {

  createGmshVertices();
  createGmshEdges();
  // createGmshFaces(pmessage);

}









void PModel::buildGmsh() {


  // _gmodel = new GModel();
  // _gmodel->setFactory("Gmsh");
  gmsh::model::add(_name);


  // ------------------------------
  // Create Gmsh vertices
  createGmshVertices();


  // ------------------------------
  // Create Gmsh edges
  createGmshEdges();


  // ------------------------------
  // Create Gmsh faces
  createGmshFaces();


  gmsh::model::geo::synchronize();

  // ------------------------------
  // Create embedded entities and set local mesh sizes
  createGmshEmbeddedEntities();


  gmsh::model::geo::synchronize();

  // ------------------------------
  // Create Gmsh physical groups
  createGmshPhyscialGroups();


  if (config.debug_level >= DebugLevel::geo) {
    // Create Gmsh model and write Gmsh files for debugging

    plotGeoDebug(false);

  }


  // ------------------------------
  // Meshing

    PLOG(info) << "meshing";

  if (_element_shape == 4) {
    if (_transfinite_auto) {
      gmsh::model::mesh::setTransfiniteAutomatic(
        {}, _transfinite_corner_angle, _transfinite_recombine
      );
    }

    if (_recombine) {
      for (const auto &face_tag : _gmsh_face_tags) {
        gmsh::model::mesh::setRecombine(
          2, face_tag.second, _recombine_angle
        );
      }
    }
  }

  // gmsh::logger::stop();

  // unsigned int mesh_algo = 6;
  // GmshSetOption("Mesh", "Algorithm", mesh_algo);
  // _gmodel->mesh(2);
  generateMesh2DWithDiagnostics(_global_mesh_size);
  // pmessage->print(1, "element type: " + std::to_string(_element_type));
  if (_element_type == 2) {
    // _gmodel->setOrderN(2, 0, 0);
    gmsh::model::mesh::setOrder(2);
  }

  // gmsh::logger::start();




}









// int PModel::indexGmshElements() {
//   int index = 0;
//   // for (auto fit = _gmodel->firstFace(); fit != _gmodel->lastFace(); ++fit) {
//   for (auto pf : _dcel->faces()) {
//     if (pf->gface() != nullptr) {
//       for (auto ele : pf->gface()->triangles) {
//         index++;
//         _gmodel->setMeshElementIndex(ele, index);
//       }
//     }
//   }
//   return index;
// }
