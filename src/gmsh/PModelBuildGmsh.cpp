#include "PModel.hpp"

#include "PDCELFace.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include <gmsh.h>

// #include "gmsh/GEdge.h"
// #include "gmsh/GEdgeLoop.h"
// #include "gmsh/GEntity.h"
// #include "gmsh/GFace.h"
// #include "gmsh/GModel.h"
// #include "gmsh/GVertex.h"
// #include "gmsh/Gmsh.h"
// #include "gmsh/MElement.h"
// #include "gmsh/MTriangle.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <utility>


// void PModel::initGmshModel(Message *pmessage) {
//   _gmodel = new GModel();
//   _gmodel->setFactory("Gmsh");

//   return;
// }




// std::size_t PModel::getNumOfNodes() {
//   std::size_t nnodes;
//   gmsh::model::mesh::getMaxNodeTag(nnodes);
//   return nnodes;
// }




// std::size_t PModel::getNumOfElements() {
//   std::size_t nelems;
//   gmsh::model::mesh::getMaxElementTag(nelems);
//   return nelems;
// }









void PModel::createGmshVertices(Message *pmessage) {
  PLOG(info) << pmessage->message("creating gmsh vertices");

  // GVertex *gv;
  int _gv_tag;

  for (auto v : _dcel->vertices()) {

    if (v->gbuild()) {
      // gv = _gmodel->addVertex(v->x(), v->y(), v->z(), _global_mesh_size);

      // if (v->onJoint()) {
      //   gv->tags.push_back("joint");
      // }

      // v->setGVertex(gv);

      _gv_tag = gmsh::model::geo::addPoint(
        v->x(), v->y(), v->z(), _global_mesh_size);
      v->setGVertexTag(_gv_tag);
    }

  }

  return;

}









void PModel::recordInterface(PDCELHalfEdge *he, Message *pmessage) {

  if (!he->onJoint() && !he->twin()->onJoint()) {

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

          he->setOnJoint(true);
        }

      }
    }
  }

  return;
}









void PModel::createGmshEdges(Message *pmessage) {

  PLOG(info) << pmessage->message("creating gmsh edges");

  // GEdge *ge;
  int _ge_tag;
  // GEdgeSigned *ges;

  for (auto he : _dcel->halfedges()) {

    if (he->source()->gbuild() && he->target()->gbuild()) {

      // Check if the GEdge twin has been created
      // ge = he->twin()->gedge();
      _ge_tag = he->twin()->gedgeTag();

      // if (ge == nullptr) {
      if (_ge_tag == 0) {

        // New GEdge for the pair of half edges
        if (he->sign() > 0) {
          // ge = _gmodel->addLine(he->source()->gvertex(),
          //                       he->target()->gvertex());
          _ge_tag = gmsh::model::geo::addLine(
            he->source()->gvertexTag(),
            he->target()->gvertexTag());
        }
        else {
          // ge = _gmodel->addLine(he->twin()->source()->gvertex(),
          //                       he->twin()->target()->gvertex());
          _ge_tag = gmsh::model::geo::addLine(
            he->twin()->source()->gvertexTag(),
            he->twin()->target()->gvertexTag());
        }

        // Interface
        if (_itf_output) {

          recordInterface(he, pmessage);

        }

      }
      // he->setGEdge(ge);
      he->setGEdgeTag(_ge_tag);
    }
  }
}









void PModel::createGmshFaces(Message *pmessage) {
  PLOG(info) << pmessage->message("creating gmsh faces");

  // GVertex *gv;
  // GEdge *ge;
  // GFace *gf;
  // int _gv_tag;  // gmsh vertex tag
  int _ge_tag;  // gmsh edge tag
  int _gel_tag;  // gmsh edge loop tag
  int _gf_tag;  // gmsh face tag

  for (auto f : _dcel->faces()) {

    PLOG(debug) << pmessage->message("");
    PLOG(debug) << pmessage->message("  face: " + f->name());

    if (f->gbuild() && f->outer() != nullptr) {

      // std::vector<GEdge *> geloop;
      // std::vector<std::vector<GEdge *>> geloops;
      // std::vector<GEdgeSigned> gesloop;
      // std::vector<std::vector<GEdgeSigned>> gesloops;

      // Vector of edge tags of a loop
      std::vector<int> _ge_tags;

      // Vector of edge loop tags
      std::vector<int> _geloop_tags;


      // Add outer loop
      PLOG(debug) << pmessage->message("  adding outer loop");
      PDCELHalfEdge *he = f->outer();
      do {

        // gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
        // std::cout << he << std::endl;
        int _tag = he->gedgeTag();
        // std::cout << "tag: " << _tag << std::endl;
        if (_tag == 0) {
          _tag = he->twin()->gedgeTag();
        }
        if (he->sign() < 0) {
          _tag = -1 * _tag;
        }
        _ge_tags.push_back(_tag);
        he = he->next();

      } while (he != f->outer());
      // gesloops.push_back(gesloop);
      // printVector(_ge_tags);
      _gel_tag = gmsh::model::geo::addCurveLoop(_ge_tags);
      _geloop_tags.push_back(_gel_tag);


      // Add inner loops
      PLOG(debug) << pmessage->message("  adding inner loops");
      for (auto hei : f->inners()) {

        // gesloop.clear();
        _ge_tags.clear();
        he = hei;
        do {
          // gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
          int _tag = he->gedgeTag();
          if (_tag == 0) {
            _tag = -1 * he->twin()->gedgeTag();
          }
          _ge_tags.push_back(_tag);
          he = he->next();
        }
        while (he != hei);
        // gesloops.push_back(gesloop);
        _gel_tag = gmsh::model::geo::addCurveLoop(_ge_tags);
        _geloop_tags.push_back(_gel_tag);

      }

      // gf = _gmodel->addPlanarFace(gesloops);
      // f->setGFace(gf);
      _gf_tag = gmsh::model::geo::addPlaneSurface(_geloop_tags);
      f->setGFaceTag(_gf_tag);


      // Set physical entity id
      // gf->addPhysicalEntity(f->layertype()->id());


      // Create embedded entities and set local mesh sizes
      PLOG(debug) << pmessage->message("  adding local mesh size");
      if (f->getMeshSize() != -1) {
        // std::cout << f->getMeshSize() << std::endl;
        // PDCELVertex *v = f->getEmbeddedVertex();
        // std::vector<GVertex *> gvs;
        int _gv_tag_prev = 0;
        int _gv_tag_curr = 0;

        for (auto v : f->getEmbeddedVertices()) {

          // gv = _gmodel->addVertex(v->x(), v->y(), v->z(), f->getMeshSize());
          _gv_tag_curr = gmsh::model::geo::addPoint(
            v->x(), v->y(), v->z(), f->getMeshSize());
          // v->setGVertex(gv);
          // v->setGVertexTag(_gv_tag_curr);
          // gvs.push_back(gv);
          // gf->addEmbeddedVertex(gv);
          f->addEmbeddedGVertexTag(_gv_tag_curr);

          // If there are more than one embedded vertices, add embedded edges
          if (_gv_tag_prev != 0) {
            _ge_tag = gmsh::model::geo::addLine(_gv_tag_prev, _gv_tag_curr);
            f->addEmbeddedGEdgeTag(_ge_tag);
          }

          _gv_tag_prev = _gv_tag_curr;

        }

        // if (gvs.size() > 1) {

        //   for (auto i = 0; i < gvs.size() - 1; i++) {
        //     ge = _gmodel->addLine(gvs[i], gvs[i+1]);
        //     gf->addEmbeddedEdge(ge);
        //   }

        // }
      }
    }
  }
}









void PModel::createGmshEmbeddedEntities(Message *pmessage) {

  PLOG(debug) << pmessage->message("  adding embedded entities");
  for (auto f : _dcel->faces()) {

    PLOG(debug) << pmessage->message("");
    PLOG(debug) << pmessage->message("  face: " + f->name());
    // debug_print = false;

    if (f->getEmbeddedGVertexTags().size() > 0) {
      gmsh::model::mesh::embed(0, f->getEmbeddedGVertexTags(), 2, f->gfaceTag());

      if (f->getEmbeddedGEdgeTags().size() > 0) {
        gmsh::model::mesh::embed(1, f->getEmbeddedGEdgeTags(), 2, f->gfaceTag());
      }

    }
  }

  return;
}









void PModel::createGmshPhyscialGroups(Message *pmessage) {

  PLOG(info) << pmessage->message("creating physical groups");

  std::vector<int> group_tags;
  std::vector<std::vector<int>> group_face_tags;

  for (auto f : _dcel->faces()) {

    PLOG(debug) << pmessage->message("");
    PLOG(debug) << pmessage->message("  face: " + f->name());

    if (f->gbuild() && f->outer() != nullptr) {

      PLOG(debug) << pmessage->message(
        "  id: " + std::to_string(f->layertype()->id())
        + ", material: " + f->layertype()->material()->getName()
        + ", theta_3 = " + std::to_string(f->layertype()->angle())
      );

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
        index = group_tags.size() - 1;
      }

      // Add face tag to the group
      group_face_tags[index].push_back(f->gfaceTag());

      // gf->addPhysicalEntity(f->layertype()->id());
      f->setGmshPhysicalGroupTag(f->layertype()->id());

    }
  }

  // Create physical groups
  for (auto i=0; i<group_tags.size(); i++) {

    gmsh::model::addPhysicalGroup(2, group_face_tags[i], group_tags[i]);

  }

  return;
}









void PModel::createGmshGeo(Message *pmessage) {

  createGmshVertices(pmessage);
  createGmshEdges(pmessage);
  // createGmshFaces(pmessage);

}









void PModel::buildGmsh(Message *pmessage) {

  pmessage->increaseIndent();

  // _gmodel = new GModel();
  // _gmodel->setFactory("Gmsh");
  gmsh::model::add(_name);


  // ------------------------------
  // Create Gmsh vertices
  createGmshVertices(pmessage);


  // ------------------------------
  // Create Gmsh edges
  createGmshEdges(pmessage);


  // ------------------------------
  // Create Gmsh faces
  createGmshFaces(pmessage);


  gmsh::model::geo::synchronize();

  // ------------------------------
  // Create embedded entities and set local mesh sizes
  createGmshEmbeddedEntities(pmessage);


  gmsh::model::geo::synchronize();

  // ------------------------------
  // Create Gmsh physical groups
  createGmshPhyscialGroups(pmessage);


  if (config.debug) {
    // Create Gmsh model and write Gmsh files for debugging

    plotGeoDebug(pmessage, false);

  }


  // ------------------------------
  // Meshing

  PLOG(info) << pmessage->message("meshing");

  // gmsh::logger::stop();

  // unsigned int mesh_algo = 6;
  // GmshSetOption("Mesh", "Algorithm", mesh_algo);
  // _gmodel->mesh(2);
  gmsh::model::mesh::generate(2);
  // pmessage->print(1, "element type: " + std::to_string(_element_type));
  if (_element_type == 2) {
    // _gmodel->setOrderN(2, 0, 0);
    gmsh::model::mesh::setOrder(2);
  }

  // gmsh::logger::start();




  pmessage->decreaseIndent();

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

