#include "PModel.hpp"

#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "gmsh/GEdge.h"
#include "gmsh/GEdgeLoop.h"
#include "gmsh/GEntity.h"
#include "gmsh/GFace.h"
#include "gmsh/GModel.h"
#include "gmsh/GVertex.h"
#include "gmsh/Gmsh.h"
#include "gmsh/MElement.h"
#include "gmsh/MTriangle.h"

#include <iostream>
#include <string>
#include <vector>


void PModel::initGmshModel(Message *pmessage) {
  _gmodel = new GModel();
  _gmodel->setFactory("Gmsh");

  return;
}









void PModel::createGmshVertices(Message *pmessage) {
  PLOG(info) << pmessage->message("creating gmsh vertices");
  GVertex *gv;
  for (auto v : _dcel->vertices()) {
    if (v->gbuild()) {
      gv = _gmodel->addVertex(v->x(), v->y(), v->z(), _global_mesh_size);
      v->setGVertex(gv);
    }
  }

  return;
}









void PModel::createGmshEdges(Message *pmessage) {
  PLOG(info) << pmessage->message("creating gmsh edges");
  GEdge *ge;
  // GEdgeSigned *ges;
  for (auto he : _dcel->halfedges()) {
    if (he->source()->gbuild() && he->target()->gbuild()) {
      ge = he->twin()->gedge();

      if (ge == nullptr) {
        // New GEdge for the pair of half edges
        // std::cout << "new gedge: " << he << std::endl;
        if (he->sign() > 0) {
          ge = _gmodel->addLine(he->source()->gvertex(),
                                he->target()->gvertex());
        } else {
          ge = _gmodel->addLine(he->twin()->source()->gvertex(),
                                he->twin()->target()->gvertex());
        }
      }
      // ges = new GEdgeSigned(he->sign(), ge);
      he->setGEdge(ge);
    }
  }
}









void PModel::createGmshFaces(Message *pmessage) {
  PLOG(info) << pmessage->message("creating gmsh faces");

  GVertex *gv;
  GEdge *ge;
  GFace *gf;

  for (auto f : _dcel->faces()) {

    if (f->gbuild() && f->outer() != nullptr) {

      std::vector<GEdge *> geloop;
      std::vector<std::vector<GEdge *>> geloops;
      std::vector<GEdgeSigned> gesloop;
      std::vector<std::vector<GEdgeSigned>> gesloops;


      // Add outer loop
      PDCELHalfEdge *he = f->outer();
      do {

        gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
        he = he->next();

      } while (he != f->outer());
      gesloops.push_back(gesloop);


      // Add inner loops
      for (auto hei : f->inners()) {

        gesloop.clear();
        he = hei;
        do {
          gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
          he = he->next();
        }
        while (he != hei);
        gesloops.push_back(gesloop);

      }

      gf = _gmodel->addPlanarFace(gesloops);
      f->setGFace(gf);


      // Set physical entity id
      gf->addPhysicalEntity(f->layertype()->id());


      // Set local mesh size
      if (f->getMeshSize() != -1) {
        // std::cout << f->getMeshSize() << std::endl;
        // PDCELVertex *v = f->getEmbeddedVertex();
        std::vector<GVertex *> gvs;
        for (auto v : f->getEmbeddedVertices()) {
          gv = _gmodel->addVertex(v->x(), v->y(), v->z(), f->getMeshSize());
          v->setGVertex(gv);
          gvs.push_back(gv);
          gf->addEmbeddedVertex(gv);
        }
        if (gvs.size() > 1) {
          for (auto i = 0; i < gvs.size() - 1; i++) {
            ge = _gmodel->addLine(gvs[i], gvs[i+1]);
            gf->addEmbeddedEdge(ge);
          }
        }
      }
    }
  }
}









void PModel::createGmshGeo(Message *pmessage) {

  createGmshVertices(pmessage);
  createGmshEdges(pmessage);
  // createGmshFaces(pmessage);

}









void PModel::buildGmsh(Message *pmessage) {
  // i_indent++;
  pmessage->increaseIndent();
  // GmshInitialize(0, 0);
  // printInfo(1, "Gmsh initialized");

  _gmodel = new GModel();
  _gmodel->setFactory("Gmsh");

  // ------------------------------
  // Create Gmsh vertices

  // std::cout << "- creating gmsh vertices" << std::endl;
  // printInfo(i_indent, "creating gmsh vertices");
  // pmessage->print(1, "creating gmsh vertices");
  PLOG(info) << pmessage->message("creating gmsh vertices");
  GVertex *gv;
  for (auto v : _dcel->vertices()) {
    if (v->gbuild()) {
      gv = _gmodel->addVertex(v->x(), v->y(), v->z(), _global_mesh_size);
      v->setGVertex(gv);
    }
  }

  // ------------------------------
  // Create Gmsh edges

  // std::cout << "- creating gmsh edges" << std::endl;
  // printInfo(i_indent, "creating gmsh edges");
  // pmessage->print(1, "creating gmsh edges");
  PLOG(info) << pmessage->message("creating gmsh edges");
  GEdge *ge;
  // GEdgeSigned *ges;
  for (auto he : _dcel->halfedges()) {
    // std::cout << he;
    // std::cout << " " << he->source()->gbuild();
    // std::cout << " " << he->target()->gbuild() << std::endl;
    if (he->source()->gbuild() && he->target()->gbuild()) {
      ge = he->twin()->gedge();

      if (ge == nullptr) {
        // New GEdge for the pair of half edges
        // std::cout << "new gedge: " << he << std::endl;
        if (he->sign() > 0) {
          ge = _gmodel->addLine(he->source()->gvertex(),
                                he->target()->gvertex());
        } else {
          ge = _gmodel->addLine(he->twin()->source()->gvertex(),
                                he->twin()->target()->gvertex());
        }
      }
      // ges = new GEdgeSigned(he->sign(), ge);
      he->setGEdge(ge);
    }
  }

  // ------------------------------
  // Create Gmsh faces

  // std::cout << "- creating gmsh faces" << std::endl;
  // printInfo(i_indent, "creating gmsh face");
  // pmessage->print(1, "creating gmsh face");
  PLOG(info) << pmessage->message("creating gmsh face");
  GFace *gf;
  // int face_count = 0;
  bool debug_print = false;
  for (auto f : _dcel->faces()) {

    PLOG(debug) << pmessage->message("");
    PLOG(debug) << pmessage->message("  face: " + f->name());
    debug_print = false;
    if (f->gbuild() && f->outer() != nullptr) {
      // if (
      //   f->name() == "sgm_18_area_1_layer_1" || f->name() == "sgm_18_area_2_layer_1"
      // ) debug_print = true;

      // face_count++;
      // std::cout << "gmsh face: " << face_count << std::endl;
      std::vector<GEdge *> geloop;
      std::vector<std::vector<GEdge *>> geloops;
      std::vector<GEdgeSigned> gesloop;
      std::vector<std::vector<GEdgeSigned>> gesloops;

      // if (debug_print) std::cout << "\n" << f->name() << std::endl;

      // Add outer loop
      PLOG(debug) << pmessage->message("  adding outer loop");
      PDCELHalfEdge *he = f->outer();
      do {
        // if (debug_print) {
        //   std::cout << he << std::endl;
        // }
        gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
        he = he->next();
      } while (he != f->outer());
      gesloops.push_back(gesloop);

      // Add inner loops
      PLOG(debug) << pmessage->message("  adding inner loops");
      for (auto hei : f->inners()) {
        gesloop.clear();
        he = hei;
        do {
          gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
          he = he->next();
        }
        while (he != hei);
        gesloops.push_back(gesloop);
      }

      gf = _gmodel->addPlanarFace(gesloops);
      f->setGFace(gf);

      // Set physical entity id
      PLOG(debug) << pmessage->message("  adding physical entity");
      if (debug_print) {
      std::cout << f->layertype() << std::endl;
      }
      PLOG(debug) << pmessage->message(
        "  id: " + std::to_string(f->layertype()->id())
        + ", material: " + f->layertype()->material()->getName()
        + ", theta_3 = " + std::to_string(f->layertype()->angle())
      );
      gf->addPhysicalEntity(f->layertype()->id());

      // Set local mesh size
      PLOG(debug) << pmessage->message("  adding local mesh size");
      if (f->getMeshSize() != -1) {
        // std::cout << f->getMeshSize() << std::endl;
        // PDCELVertex *v = f->getEmbeddedVertex();
        std::vector<GVertex *> gvs;
        for (auto v : f->getEmbeddedVertices()) {
          gv = _gmodel->addVertex(v->x(), v->y(), v->z(), f->getMeshSize());
          v->setGVertex(gv);
          gvs.push_back(gv);
          gf->addEmbeddedVertex(gv);
        }
        if (gvs.size() > 1) {
          for (auto i = 0; i < gvs.size() - 1; i++) {
            ge = _gmodel->addLine(gvs[i], gvs[i+1]);
            gf->addEmbeddedEdge(ge);
          }
        }
      }
    }
  }

  if (config.debug) {
    // Create Gmsh model and write Gmsh files for debugging

    plotGeoDebug(pmessage, false);

    // writeGmshGeo(fn_base+".geo", pmessage);
    // writeGmshOpt(fn_base+".opt", pmessage);
  }

  // _gmodel->healGeometry(1e-6);

  // ------------------------------
  // Meshing

  // std::cout << "- meshing...";
  // printInfo(i_indent, "meshing");
  // pmessage->print(1, "meshing");
  PLOG(info) << pmessage->message("meshing");

  unsigned int mesh_algo = 6;
  GmshSetOption("Mesh", "Algorithm", mesh_algo);
  _gmodel->mesh(2);
  // pmessage->print(1, "element type: " + std::to_string(_element_type));
  if (_element_type == 2) {
    _gmodel->setOrderN(2, 0, 0);
  }
  // std::cout << "done" << std::endl;
  // printInfo(i_indent, "meshing -- done");

  // _gmodel->removeDuplicateMeshVertices(1e-6);
  // _gmodel->removeInvisibleElements();

  // i_indent--;
  pmessage->decreaseIndent();
}









int PModel::indexGmshElements() {
  int index = 0;
  // for (auto fit = _gmodel->firstFace(); fit != _gmodel->lastFace(); ++fit) {
  for (auto pf : _dcel->faces()) {
    if (pf->gface() != nullptr) {
      for (auto ele : pf->gface()->triangles) {
        index++;
        _gmodel->setMeshElementIndex(ele, index);
      }
    }
  }
  return index;
}
