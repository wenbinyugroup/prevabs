#include "PModel.hpp"

#include "PModelIO.hpp"
#include "CrossSection.hpp"
#include "Material.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "execu.hpp"
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

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

PModel::PModel(std::string name) {
  _name = name;
  _global_mesh_size = 1.0;
  _element_type = 1;
}





void PModel::initialize() {

  if (config.debug) {
    config.fdeb = fopen((config.file_directory + config.file_base_name + ".debug").c_str(), "w");
  }

  gmsh::initialize();
  // GmshInitialize(0, 0);
  // printInfo(1, "Gmsh initialized");

  // gmsh::logger::stop();

}





void PModel::finalize() {

  gmsh::finalize();
  // GmshFinalize();

  if (config.debug) {
    fclose(config.fdeb);
  }

}





PDCELVertex *PModel::getPointByName(std::string name) {
  for (auto p : _vertices) {
    if (p->name() == name) {
      return p;
    }
  }
  return nullptr;
}





Baseline *PModel::getBaselineByName(std::string name) {
  for (auto l : _baselines) {
    if (l->getName() == name) {
      return l;
    }
  }
  return nullptr;
}





Baseline *PModel::getBaselineByNameCopy(std::string name) {
  Baseline *bl;
  for (auto l : _baselines) {
    if (l->getName() == name) {
      bl = new Baseline(l);
      return bl;
    }
  }
  return nullptr;
}





Material *PModel::getMaterialByName(std::string name) {
  for (auto m : _materials) {
    if (m->getName() == name) {
      return m;
    }
  }
  return nullptr;
}





Lamina *PModel::getLaminaByName(std::string name) {
  for (auto l : _laminas) {
    if (l->getName() == name) {
      return l;
    }
  }
  return nullptr;
}





LayerType *PModel::getLayerTypeByMaterialAngle(Material *m, double angle) {
  for (auto lt : _layertypes) {
    if (lt->material() == m && lt->angle() == angle) {
      return lt;
    }
  }
  return nullptr;
}





LayerType *PModel::getLayerTypeByMaterialNameAngle(std::string name, double angle) {
  for (auto lt : _layertypes) {
    if (lt->material()->getName() == name && lt->angle() == angle) {
      return lt;
    }
  }
  return nullptr;
}





Layup *PModel::getLayupByName(std::string name) {
  for (auto l : _layups) {
    if (l->getName() == name) {
      return l;
    }
  }
  return nullptr;
}

// std::vector<int> &PModel::getIntParamsForGmsh() {
//   std::vector<int> i_params(13, 0);

//   i_params[0] = config.analysis_tool;
//   i_params[1] = _element_type;
//   i_params[2] = _cross_section->getUsedMaterials().size();
//   i_params[3] = _cross_section->getUsedLayerTypes().size();

//   if (config.analysis_tool == 1) {
//     // VABS
//     if (_analysis_thermal != 0) {
//       i_params[4] = 3;
//     }
//     i_params[5] = _analysis_model;
//     i_params[6] = _analysis_trapeze;
//     i_params[7] = _analysis_vlasov;
//     i_params[8] = _analysis_curvature;
//     i_params[9] = _analysis_oblique;
//   } else {
//     // SwiftComp
//     i_params[5] = _analysis_model;
//   }

//   return i_params;
// }

// std::vector<double> &PModel::getDoubleParamsForGmsh() {
//   std::vector<double> d_params(5, 0.0);

//   d_params[0] = _analysis_curvatures[0];
//   d_params[1] = _analysis_curvatures[1];
//   d_params[2] = _analysis_curvatures[2];
//   d_params[3] = _analysis_obliques[0];
//   d_params[4] = _analysis_obliques[1];

//   return d_params;
// }





void PModel::setCurvatures(double k1, double k2, double k3) {
  _analysis_curvatures.push_back(k1);
  _analysis_curvatures.push_back(k2);
  _analysis_curvatures.push_back(k3);
}





void PModel::setObliques(double cos11, double cos21) {
  _analysis_obliques.push_back(cos11);
  _analysis_obliques.push_back(cos21);
}





void PModel::addLayerType(Material *m, double a) {
  LayerType *lt = new LayerType(0, m, a);
  _layertypes.push_back(lt);
}





void PModel::addLayerType(std::string name, double a) {
  Material *m = getMaterialByName(name);
  LayerType *lt = new LayerType(0, m, a);
  _layertypes.push_back(lt);
}





void PModel::summary(Message *pmessage) {
  // std::cout << std::fixed;
  if (scientific_format) {
    std::cout << std::scientific;
  }

  // std::cout << doubleLine80 << std::endl << std::endl;
  pmessage->print(9, doubleLine80);

  // std::cout << markInfo << " SUMMARY" << std::endl << std::endl;
  pmessage->print(9, "SUMMARY");

  // std::cout << doubleLine80 << std::endl;
  pmessage->print(9, doubleLine80);

  // std::cout << "BASEPOINTS" << std::setw(8) << basepoints.size() <<
  // std::endl; std::cout << singleLine80 << std::endl; std::cout <<
  // std::setw(16) << "Name" << std::setw(16) << "X" << std::setw(16)
  //           << "Y" << std::endl;
  // for (auto bp : basepoints)
  //   std::cout << bp << std::endl;
  pmessage->printBlank();
  pmessage->print(9, "summary of base points");
  for (auto bp : _vertices) {
    std::stringstream ss;
    ss << bp;
    pmessage->print(9, ss);
  }

  // std::cout << doubleLine80 << std::endl;
  pmessage->print(9, doubleLine80);

  // std::cout << "BASELINES" << std::setw(8) << _baselines.size() << std::endl;
  // std::cout << singleLine80 << std::endl;
  // std::cout << std::setw(16) << "Name" << std::setw(16) << "Type"
  //           << std::setw(16) << "# Points" << std::endl;
  // for (auto bl : _baselines)
  //   std::cout << std::setw(16) << bl->getName() << std::setw(16)
  //             << bl->getType() << std::setw(16) << bl->getNumberOfBasepoints()
  //             << std::endl;
  pmessage->printBlank();
  pmessage->print(9, "summary of base lines");
  for (auto bsl : _baselines) {
    bsl->print(pmessage);
    pmessage->printBlank();
  }

  // std::cout << doubleLine80 << std::endl;
  pmessage->print(9, doubleLine80);

  std::cout << "MATERIALS" << std::setw(8) << _materials.size() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Name" << std::setw(32)
            << "Type" << std::setw(16) << "Density" << std::endl;
  for (auto m : _materials)
    std::cout << std::setw(16) << m->id() << std::setw(16) << m->getName()
              << std::setw(32) << m->getType() << std::setw(16)
              << m->getDensity() << std::endl;

  // std::cout << doubleLine80 << std::endl;
  pmessage->print(9, doubleLine80);

  std::cout << "LAYER TYPES" << std::setw(8) << _layertypes.size() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Material ID"
            << std::setw(16) << "Angle" << std::setw(16) << "Material Name"
            << std::endl;
  for (auto plt : _layertypes)
    std::cout << plt << std::endl;

  std::cout << doubleLine80 << std::endl;

  std::cout << "LAYUPS" << std::setw(8) << _layups.size() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(32) << "Name" << std::setw(16) << "# Plies"
            << std::setw(16) << "Thickness" << std::endl;
  for (auto l : _layups)
    std::cout << std::setw(32) << l->getName() << std::setw(16)
              << l->getPlies().size() << std::setw(16) << l->getThickness()
              << std::endl;

  pmessage->printBlank();
  pmessage->print(9, "summary of layups");
  for (auto lyp : _layups) {
    lyp->print(pmessage, 9);
    pmessage->printBlank();
  }

  // std::cout << doubleLine80 << std::endl;
  pmessage->print(9, doubleLine80);

  pmessage->printBlank();
  pmessage->print(9, "summary of components");
  for (auto cmp : _cross_section->components()) {
    cmp->print(pmessage, 9);
    pmessage->printBlank();
  }
}





void PModel::build(Message *pmessage) {
  _dcel = new PDCEL();
  _dcel->initialize();

  // for (auto cs : crosssections) {
  //   cs->build();
  // }
  _cross_section->build(pmessage);

  // _dcel->print_dcel();

  // _dcel->vertextree()->printInOrder();

  pmessage->printBlank();
  _dcel->fixGeometry(pmessage);
}









// void PModel::initGmshModel(Message *pmessage) {
//   _gmodel = new GModel();
//   _gmodel->setFactory("Gmsh");

//   return;
// }









// void PModel::createGmshVertices(Message *pmessage) {
//   PLOG(info) << pmessage->message("creating gmsh vertices");
//   GVertex *gv;
//   for (auto v : _dcel->vertices()) {
//     if (v->gbuild()) {
//       gv = _gmodel->addVertex(v->x(), v->y(), v->z(), _global_mesh_size);
//       v->setGVertex(gv);
//     }
//   }

//   return;
// }









// void PModel::createGmshEdges(Message *pmessage) {
//   PLOG(info) << pmessage->message("creating gmsh edges");
//   GEdge *ge;
//   // GEdgeSigned *ges;
//   for (auto he : _dcel->halfedges()) {
//     if (he->source()->gbuild() && he->target()->gbuild()) {
//       ge = he->twin()->gedge();

//       if (ge == nullptr) {
//         // New GEdge for the pair of half edges
//         // std::cout << "new gedge: " << he << std::endl;
//         if (he->sign() > 0) {
//           ge = _gmodel->addLine(he->source()->gvertex(),
//                                 he->target()->gvertex());
//         } else {
//           ge = _gmodel->addLine(he->twin()->source()->gvertex(),
//                                 he->twin()->target()->gvertex());
//         }
//       }
//       // ges = new GEdgeSigned(he->sign(), ge);
//       he->setGEdge(ge);
//     }
//   }
// }









// void PModel::createGmshFaces(Message *pmessage) {
//   PLOG(info) << pmessage->message("creating gmsh faces");

//   GVertex *gv;
//   GEdge *ge;
//   GFace *gf;

//   for (auto f : _dcel->faces()) {

//     if (f->gbuild() && f->outer() != nullptr) {

//       std::vector<GEdge *> geloop;
//       std::vector<std::vector<GEdge *>> geloops;
//       std::vector<GEdgeSigned> gesloop;
//       std::vector<std::vector<GEdgeSigned>> gesloops;


//       // Add outer loop
//       PDCELHalfEdge *he = f->outer();
//       do {

//         gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
//         he = he->next();

//       } while (he != f->outer());
//       gesloops.push_back(gesloop);


//       // Add inner loops
//       for (auto hei : f->inners()) {

//         gesloop.clear();
//         he = hei;
//         do {
//           gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
//           he = he->next();
//         }
//         while (he != hei);
//         gesloops.push_back(gesloop);

//       }

//       gf = _gmodel->addPlanarFace(gesloops);
//       f->setGFace(gf);


//       // Set physical entity id
//       gf->addPhysicalEntity(f->layertype()->id());


//       // Set local mesh size
//       if (f->getMeshSize() != -1) {
//         // std::cout << f->getMeshSize() << std::endl;
//         // PDCELVertex *v = f->getEmbeddedVertex();
//         std::vector<GVertex *> gvs;
//         for (auto v : f->getEmbeddedVertices()) {
//           gv = _gmodel->addVertex(v->x(), v->y(), v->z(), f->getMeshSize());
//           v->setGVertex(gv);
//           gvs.push_back(gv);
//           gf->addEmbeddedVertex(gv);
//         }
//         if (gvs.size() > 1) {
//           for (auto i = 0; i < gvs.size() - 1; i++) {
//             ge = _gmodel->addLine(gvs[i], gvs[i+1]);
//             gf->addEmbeddedEdge(ge);
//           }
//         }
//       }
//     }
//   }
// }









// void PModel::createGmshGeo(Message *pmessage) {

//   createGmshVertices(pmessage);
//   createGmshEdges(pmessage);
//   // createGmshFaces(pmessage);

// }









void PModel::plotGeoDebug(Message *pmessage, bool create_gmsh_geo) {

  if (create_gmsh_geo) {
    // initGmshModel(pmessage);
    createGmshGeo(pmessage);
  }

  debug_plot_count++;
  std::string fn_base = config.file_directory + config.file_base_name + "_debug";

  writeGmshGeo(fn_base+"_"+std::to_string(debug_plot_count)+".geo_unrolled", pmessage);

  if (debug_plot_count == 1) {
    writeGmshOpt(fn_base, pmessage);
  }

  for (auto v : _dcel->vertices()) {
    // if (v->gvertex()) v->resetGVertex();
    if (v->gvertexTag() != 0) v->resetGVertexTag();
  }

  for (auto he : _dcel->halfedges()) {
    // if (he->gedge()) he->resetGEdge();
    if (he->gedgeTag() != 0) he->resetGEdgeTag();
  }

}









// void PModel::buildGmsh(Message *pmessage) {
//   // i_indent++;
//   pmessage->increaseIndent();
//   // GmshInitialize(0, 0);
//   // printInfo(1, "Gmsh initialized");

//   _gmodel = new GModel();
//   _gmodel->setFactory("Gmsh");

//   // ------------------------------
//   // Create Gmsh vertices

//   // std::cout << "- creating gmsh vertices" << std::endl;
//   // printInfo(i_indent, "creating gmsh vertices");
//   // pmessage->print(1, "creating gmsh vertices");
//   PLOG(info) << pmessage->message("creating gmsh vertices");
//   GVertex *gv;
//   for (auto v : _dcel->vertices()) {
//     if (v->gbuild()) {
//       gv = _gmodel->addVertex(v->x(), v->y(), v->z(), _global_mesh_size);
//       v->setGVertex(gv);
//     }
//   }

//   // ------------------------------
//   // Create Gmsh edges

//   // std::cout << "- creating gmsh edges" << std::endl;
//   // printInfo(i_indent, "creating gmsh edges");
//   // pmessage->print(1, "creating gmsh edges");
//   PLOG(info) << pmessage->message("creating gmsh edges");
//   GEdge *ge;
//   // GEdgeSigned *ges;
//   for (auto he : _dcel->halfedges()) {
//     if (he->source()->gbuild() && he->target()->gbuild()) {
//       ge = he->twin()->gedge();

//       if (ge == nullptr) {
//         // New GEdge for the pair of half edges
//         // std::cout << "new gedge: " << he << std::endl;
//         if (he->sign() > 0) {
//           ge = _gmodel->addLine(he->source()->gvertex(),
//                                 he->target()->gvertex());
//         } else {
//           ge = _gmodel->addLine(he->twin()->source()->gvertex(),
//                                 he->twin()->target()->gvertex());
//         }
//       }
//       // ges = new GEdgeSigned(he->sign(), ge);
//       he->setGEdge(ge);
//     }
//   }

//   // ------------------------------
//   // Create Gmsh faces

//   // std::cout << "- creating gmsh faces" << std::endl;
//   // printInfo(i_indent, "creating gmsh face");
//   // pmessage->print(1, "creating gmsh face");
//   PLOG(info) << pmessage->message("creating gmsh face");
//   GFace *gf;
//   // int face_count = 0;
//   bool debug_print = false;
//   for (auto f : _dcel->faces()) {

//     PLOG(debug) << pmessage->message("");
//     PLOG(debug) << pmessage->message("  face: " + f->name());
//     debug_print = false;
//     if (f->gbuild() && f->outer() != nullptr) {
//       // if (
//       //   f->name() == "sgm_18_area_1_layer_1" || f->name() == "sgm_18_area_2_layer_1"
//       // ) debug_print = true;

//       // face_count++;
//       // std::cout << "gmsh face: " << face_count << std::endl;
//       std::vector<GEdge *> geloop;
//       std::vector<std::vector<GEdge *>> geloops;
//       std::vector<GEdgeSigned> gesloop;
//       std::vector<std::vector<GEdgeSigned>> gesloops;

//       // if (debug_print) std::cout << "\n" << f->name() << std::endl;

//       // Add outer loop
//       PLOG(debug) << pmessage->message("  adding outer loop");
//       PDCELHalfEdge *he = f->outer();
//       do {
//         // if (debug_print) {
//         //   std::cout << he << std::endl;
//         // }
//         gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
//         he = he->next();
//       } while (he != f->outer());
//       gesloops.push_back(gesloop);

//       // Add inner loops
//       PLOG(debug) << pmessage->message("  adding inner loops");
//       for (auto hei : f->inners()) {
//         gesloop.clear();
//         he = hei;
//         do {
//           gesloop.push_back(GEdgeSigned(he->sign(), he->gedge()));
//           he = he->next();
//         }
//         while (he != hei);
//         gesloops.push_back(gesloop);
//       }

//       gf = _gmodel->addPlanarFace(gesloops);
//       f->setGFace(gf);

//       // Set physical entity id
//       PLOG(debug) << pmessage->message("  adding physical entity");
//       if (debug_print) {
//       std::cout << f->layertype() << std::endl;
//       }
//       PLOG(debug) << pmessage->message(
//         "  id: " + std::to_string(f->layertype()->id())
//         + ", material: " + f->layertype()->material()->getName()
//         + ", theta_3 = " + std::to_string(f->layertype()->angle())
//       );
//       gf->addPhysicalEntity(f->layertype()->id());

//       // Set local mesh size
//       PLOG(debug) << pmessage->message("  adding local mesh size");
//       if (f->getMeshSize() != -1) {
//         // std::cout << f->getMeshSize() << std::endl;
//         // PDCELVertex *v = f->getEmbeddedVertex();
//         std::vector<GVertex *> gvs;
//         for (auto v : f->getEmbeddedVertices()) {
//           gv = _gmodel->addVertex(v->x(), v->y(), v->z(), f->getMeshSize());
//           v->setGVertex(gv);
//           gvs.push_back(gv);
//           gf->addEmbeddedVertex(gv);
//         }
//         if (gvs.size() > 1) {
//           for (auto i = 0; i < gvs.size() - 1; i++) {
//             ge = _gmodel->addLine(gvs[i], gvs[i+1]);
//             gf->addEmbeddedEdge(ge);
//           }
//         }
//       }
//     }
//   }

//   if (config.debug) {
//     // Create Gmsh model and write Gmsh files for debugging

//     plotGeoDebug(pmessage, false);

//     // writeGmshGeo(fn_base+".geo", pmessage);
//     // writeGmshOpt(fn_base+".opt", pmessage);
//   }

//   // ------------------------------
//   // Meshing

//   // std::cout << "- meshing...";
//   // printInfo(i_indent, "meshing");
//   // pmessage->print(1, "meshing");
//   PLOG(info) << pmessage->message("meshing");

//   unsigned int mesh_algo = 6;
//   GmshSetOption("Mesh", "Algorithm", mesh_algo);
//   _gmodel->mesh(2);
//   // pmessage->print(1, "element type: " + std::to_string(_element_type));
//   if (_element_type == 2) {
//     _gmodel->setOrderN(2, 0, 0);
//   }
//   // std::cout << "done" << std::endl;
//   // printInfo(i_indent, "meshing -- done");

//   i_indent--;
//   pmessage->decreaseIndent();
// }









void PModel::homogenize(Message *pmessage) {

  try {
    // ================
    // READ INPUT FILES

    pmessage->printBlank();
    PLOG(info) << pmessage->message("reading input files");

    readInputMain(config.main_input, config.file_directory, this, pmessage);
    // pmodel->summary(pmessage);

    PLOG(info) << pmessage->message("reading input files -- done");
    pmessage->printBlank();


    // ==============
    // BUILD GEOMETRY

    pmessage->printBlank();
    PLOG(info) << pmessage->message("building the shape");

    build(pmessage);

    PLOG(info) << pmessage->message("building the shape -- done");
    pmessage->printBlank();


    // ================
    // MODELING IN GMSH

    pmessage->printBlank();
    PLOG(info) << pmessage->message("modeling in Gmsh");

    buildGmsh(pmessage);

    PLOG(info) << pmessage->message("modeling in Gmsh -- done");
    pmessage->printBlank();


    // ===================
    // WRITE SG INPUT FILE

    // if (config.analysis_tool != 3) {
    if (!config.integrated_solver) {
      pmessage->printBlank();
      PLOG(info) << pmessage->message("writing outputs");

      if (config.plot) {
        writeGmsh(config.file_directory + config.file_base_name, pmessage);
      }
      writeSG(config.file_name_vsc, config.analysis_tool, pmessage);

      if (_itf_output) {
        // Write supplement files
        writeSupp(pmessage);
      }

      PLOG(info) << pmessage->message("writing outputs -- done");
      pmessage->printBlank();
    }
  }
  catch (std::exception &exception) {
    pmessage->print(2, exception.what());
    return;
  }


  return;
}









void PModel::dehomogenize(Message *pmessage) {
  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("dehomogenizing...");

  // Read cs xml file
  readInputMain(config.main_input, config.file_directory, this, pmessage);


  // Write glb file
  std::string s_file_name_glb;
  s_file_name_glb = config.file_name_vsc + ".glb";
  writeGLB(s_file_name_glb, pmessage);


  // Do dehomogenization/failure analysis
  // if (config.execute) {
  //   run(pmessage);
  // }

  pmessage->decreaseIndent();

  return;
}









void PModel::run(Message *pmessage) {
  pmessage->increaseIndent();

  pmessage->printBlank();
  PLOG(info) << pmessage->message("running " + config.tool_name + " for " + config.msg_analysis);
  pmessage->printBlank();
  pmessage->print(1, " [" + config.tool_name + " Messages] ");
  pmessage->printBlank();


  std::vector<std::string> cmd_args;
  if (config.analysis_tool == 1) {
    // VABS
    if (config.integrated_solver) {
      runIntegratedVABS(config.file_name_vsc, this, pmessage);
    }
    else {
      if (config.dehomo) {
        config.vabs_option = "2";
        if (config.dehomo_nl) {
          config.vabs_option = "1";
        }
      }
      cmd_args.push_back(config.vabs_option);
      if (_load_cases.size() > 1) {
        cmd_args.push_back(std::to_string(_load_cases.size()));
      }

      runVABS(config.file_name_vsc, cmd_args, pmessage);

    }
  }

  else if (config.analysis_tool == 2) {
    // SwiftComp
    if (_analysis_model_dim == 1) {
      cmd_args.push_back("1D");
    }
    else if (_analysis_model_dim == 2) {
      cmd_args.push_back("2D");
    }
    else if (_analysis_model_dim == 3) {
      cmd_args.push_back("3D");
    }
    cmd_args.push_back(config.sc_option);
    runSC(config.file_name_vsc, cmd_args, pmessage);

  }


  pmessage->printBlank();
  pmessage->print(1, " [" + config.tool_name + " Messages End] ");
  pmessage->printBlank();
  PLOG(info) << pmessage->message("running " + config.tool_name + " for " + config.msg_analysis + " -- done");
  pmessage->printBlank();

  pmessage->decreaseIndent();

  return;
}









void PModel::plot(Message *pmessage) {
  if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    plotDehomo(pmessage);
    // pmessage->printBlank();
    // PLOG(info) << pmessage->message("post-processing recover results");


    // if (config.analysis_tool == 1) {
    //   postVABS(pmessage);
    // }
    // else if (config.analysis_tool == 2) {
    //   postSCDehomo();
    // }


    // PLOG(info) << pmessage->message("post-processing recover results -- done");
    // pmessage->printBlank();
  }


  pmessage->printBlank();
  PLOG(info) << pmessage->message("running Gmsh for visualization");

  runGmsh(config.file_name_geo, config.file_name_msh, config.file_name_opt, pmessage);

  return;
}









// ===================================================================
//
// Private helper functions
//
// ===================================================================

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
