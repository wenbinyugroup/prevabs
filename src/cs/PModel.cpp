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
    runtime.fdeb = fopen(config.file_name_deb.c_str(), "w");
    if (!runtime.fdeb) {
      std::cerr << "ERROR: Cannot open debug file: " << config.file_name_deb << std::endl;
      config.debug = false;
    }
  }

  gmsh::initialize();

  // Control Gmsh's own console output (meshing progress, info lines, etc.).
  // Level: 0=silent, 1=errors, 2=warnings, 3=info (default), 5=debug.
  // Controlled via --gmsh-verbosity; defaults to 2 (warnings).
  gmsh::option::setNumber("General.Verbosity", config.app.gmsh_verbosity);

}





void PModel::finalize() {

  gmsh::finalize();
  // GmshFinalize();

  if (config.debug && runtime.fdeb) {
    fclose(runtime.fdeb);
    runtime.fdeb = nullptr;
  }

}





void PModel::setCurvatures(double k1, double k2, double k3) {
  _analysis_curvatures.push_back(k1);
  _analysis_curvatures.push_back(k2);
  _analysis_curvatures.push_back(k3);
}





void PModel::setObliques(double cos11, double cos21) {
  _analysis_obliques.push_back(cos11);
  _analysis_obliques.push_back(cos21);
}





// addLayerType — implemented inline in PModel.hpp, delegating to MaterialRepository.










void PModel::summary() {
  MESSAGE_SCOPE(g_msg);

  // std::cout << std::fixed;
  if (scientific_format) {
    std::cout << std::scientific;
  }

  // std::cout << doubleLine80 << std::endl << std::endl;
    PLOG(debug) << doubleLine80;

  // std::cout << markInfo << " SUMMARY" << std::endl << std::endl;
    PLOG(debug) << "SUMMARY";

  // std::cout << doubleLine80 << std::endl;
    PLOG(debug) << doubleLine80;

  // std::cout << "BASEPOINTS" << std::setw(8) << basepoints.size() <<
  // std::endl; std::cout << singleLine80 << std::endl; std::cout <<
  // std::setw(16) << "Name" << std::setw(16) << "X" << std::setw(16)
  //           << "Y" << std::endl;
  // for (auto bp : basepoints)
  //   std::cout << bp << std::endl;
  g_msg->printBlank();
    PLOG(debug) << "summary of base points";
  for (auto bp : _geo_repo.vertices()) {
    std::stringstream ss;
    ss << bp;
        PLOG(debug) << ss.str();
  }

  // std::cout << doubleLine80 << std::endl;
    PLOG(debug) << doubleLine80;

  // std::cout << "BASELINES" << std::setw(8) << _baselines.size() << std::endl;
  // std::cout << singleLine80 << std::endl;
  // std::cout << std::setw(16) << "Name" << std::setw(16) << "Type"
  //           << std::setw(16) << "# Points" << std::endl;
  // for (auto bl : _baselines)
  //   std::cout << std::setw(16) << bl->getName() << std::setw(16)
  //             << bl->getType() << std::setw(16) << bl->getNumberOfBasepoints()
  //             << std::endl;
  g_msg->printBlank();
    PLOG(debug) << "summary of base lines";
  for (auto bsl : _geo_repo.baselines()) {
    bsl->print();
    g_msg->printBlank();
  }

  // std::cout << doubleLine80 << std::endl;
    PLOG(debug) << doubleLine80;

  std::cout << "MATERIALS" << std::setw(8) << _mat_repo.numMaterials() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Name" << std::setw(32)
            << "Type" << std::setw(16) << "Density" << std::endl;
  for (auto m : _mat_repo.materials())
    std::cout << std::setw(16) << m->id() << std::setw(16) << m->getName()
              << std::setw(32) << m->getType() << std::setw(16)
              << m->getDensity() << std::endl;

  // std::cout << doubleLine80 << std::endl;
    PLOG(debug) << doubleLine80;

  std::cout << "LAYER TYPES" << std::setw(8) << _mat_repo.numLayerTypes() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "ID" << std::setw(16) << "Material ID"
            << std::setw(16) << "Angle" << std::setw(16) << "Material Name"
            << std::endl;
  for (auto plt : _mat_repo.layertypes())
    std::cout << plt << std::endl;

  std::cout << doubleLine80 << std::endl;

  std::cout << "LAYUPS" << std::setw(8) << _mat_repo.layups().size() << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(32) << "Name" << std::setw(16) << "# Plies"
            << std::setw(16) << "Thickness" << std::endl;
  for (auto l : _mat_repo.layups())
    std::cout << std::setw(32) << l->getName() << std::setw(16)
              << l->getPlies().size() << std::setw(16) << l->getThickness()
              << std::endl;

  g_msg->printBlank();
    PLOG(debug) << "summary of layups";
  for (auto lyp : _mat_repo.layups()) {
    lyp->print();
    g_msg->printBlank();
  }

  // std::cout << doubleLine80 << std::endl;
    PLOG(debug) << doubleLine80;

  g_msg->printBlank();
    PLOG(debug) << "summary of components";
  for (auto cmp : _cross_section->components()) {
    cmp->print();
    g_msg->printBlank();
  }
}





void PModel::build() {
  _dcel = new PDCEL();
  _dcel->initialize();

  BuilderConfig bcfg{
    config.debug, config.tool, config.app.tol, config.app.geo_tol,
    _dcel, this,
    [this](Message* msg) { this->plotGeoDebug(); }
  };
  bcfg.model = this;
  _face_data[_dcel->faces().front()].name = "background";

  // for (auto cs : crosssections) {
  //   cs->build();
  // }
  _cross_section->build(bcfg);

  // _dcel->print_dcel();

  // _dcel->vertextree()->printInOrder();

  g_msg->printBlank();
  _dcel->fixGeometry(bcfg);
}




void PModel::plotGeoDebug(bool create_gmsh_geo) {

  debug_plot_count++;
  std::string fn_base =
      config.file_directory + config.file_base_name + "_debug";
  std::string fn_geo =
      fn_base + "_" + std::to_string(debug_plot_count) + ".geo_unrolled";

  if (create_gmsh_geo) {
    createGmshGeo();
    // Commit pending geo entities so gmsh::write can see them.
    gmsh::model::geo::synchronize();
    writeGmshGeo(fn_geo);
    if (debug_plot_count == 1) {
      writeGmshOpt(fn_base);
    }
    // Remove the current model (clears all accumulated geo entities and resets
    // the tag counter) then create a fresh one for the next debug snapshot.
    gmsh::model::remove();
    gmsh::model::add("");
  } else {
    writeGmshGeo(fn_geo);
    if (debug_plot_count == 1) {
      writeGmshOpt(fn_base);
    }
  }

  _gmsh_vertex_tags.clear();
  _gmsh_edge_tags.clear();
  _gmsh_face_tags.clear();
  _gmsh_face_embedded_vertex_tags.clear();
  _gmsh_face_embedded_edge_tags.clear();
  _gmsh_face_physical_group_tags.clear();

}




void PModel::homogenize() {

  MESSAGE_SCOPE(g_msg);

  try {
    // ================
    // READ INPUT FILES

    g_msg->printBlank();
        g_msg->print("reading input files");

    readInputMain(config.main_input, config.file_directory, this);
    // pmodel->summary(g_msg);

        g_msg->print("reading input files -- done");
    g_msg->printBlank();


    // ==============
    // BUILD GEOMETRY

    g_msg->printBlank();
        g_msg->print("building the shape");

    build();

        g_msg->print("building the shape -- done");
    g_msg->printBlank();


    // ================
    // MODELING IN GMSH

    g_msg->printBlank();
        g_msg->print("modeling in Gmsh");

    buildGmsh();

        g_msg->print("modeling in Gmsh -- done");
    g_msg->printBlank();


    // ===================
    // WRITE SG INPUT FILE

    // if (config.analysis_tool != 3) {
    if (!config.integrated_solver) {
      g_msg->printBlank();
            g_msg->print("writing outputs");

      if (config.plot) {
        writeGmsh(config.file_directory + config.file_base_name);
      }
      WriterConfig wcfg{config.tool, config.isDehomo(), config.tool_ver, config.file_name_vsc};
      writeSG(config.file_name_vsc, wcfg);

      if (_itf_output) {
        // Write supplement files
        writeSupp();
      }

            g_msg->print("writing outputs -- done");
      g_msg->printBlank();
    }
  }
  catch (std::exception &exception) {
    g_msg->error(exception.what());
    return;
  }


  return;
}









void PModel::dehomogenize() {
  MESSAGE_SCOPE(g_msg);
    g_msg->print("dehomogenizing...");

  // Read cs xml file
  readInputMain(config.main_input, config.file_directory, this);


  // Write glb file
  std::string s_file_name_glb;
  s_file_name_glb = config.file_name_vsc + ".glb";
  writeGLB(s_file_name_glb);


  // Do dehomogenization/failure analysis
  // if (config.execute) {
  //   run(pmessage);
  // }

  return;
}









void PModel::run() {
  MESSAGE_SCOPE(g_msg);

  g_msg->printBlank();
    g_msg->print("running " + config.tool_name + " for " + config.msg_analysis);
  g_msg->printBlank();
  g_msg->print(" [" + config.tool_name + " Messages] ");
  g_msg->printBlank();


  std::vector<std::string> cmd_args;

  if (config.isVABS()) {
    cmd_args.push_back(config.file_name_vsc);
    {
      if (config.isDehomo()) {
        config.vabs_option = (config.mode == AnalysisMode::DehomogenizationNL) ? "1" : "2";
      }
      if (!config.vabs_option.empty()) {
        cmd_args.push_back(config.vabs_option);
      }
      if (_pp_data.load_cases.size() > 1) {
        cmd_args.push_back(std::to_string(_pp_data.load_cases.size()));
      }

      runVABS(config.vabs_name, cmd_args);
    }
  }

  else if (config.isSC()) {
    cmd_args.push_back(config.file_name_vsc);
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
    runSC(config.sc_name, cmd_args);

  }


  g_msg->printBlank();
  g_msg->print(" [" + config.tool_name + " Messages End] ");
  g_msg->printBlank();
    g_msg->print("running " + config.tool_name + " for " + config.msg_analysis + " -- done");
  g_msg->printBlank();

  return;
}









void PModel::plot() {
  MESSAGE_SCOPE(g_msg);
  if (config.isRecovery()) {
    plotDehomo();
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


  g_msg->printBlank();
    g_msg->print("running Gmsh for visualization");

  // Load the opt file (view options) and open the FLTK GUI via the Gmsh API.
  // This avoids requiring a separate `gmsh` executable on PATH.
  if (!config.file_name_opt.empty()) {
    gmsh::merge(config.file_name_opt);
  }
  gmsh::fltk::run();

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
