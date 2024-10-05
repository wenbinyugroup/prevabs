#include "PModel.hpp"
#include "PModelIO.hpp"
#include "PDataClasses.hpp"
#include "PMeshClasses.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>




// ===================================================================

void PModel::writeNodes(FILE *file, Message *pmessage) {

  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("writing nodes");

  // std::vector<GEntity *> gentities;
  // pmodel->gmodel()->getEntities(gentities);

  std::vector<std::size_t> node_tags;
  std::vector<double> node_coords, node_params;

  gmsh::model::mesh::getNodes(node_tags, node_coords, node_params, -1, -1);

  for (std::size_t i = 0; i < node_tags.size(); ++i) {
    fprintf(file, "%8d%16e%16e\n",
            node_tags[i],
            node_coords[3*i+1],
            node_coords[3*i+2]);

    if (interfaceOutput()) {
      std::vector<int> eids;
      // Add an empty vector as a placeholder
      node_elements.push_back(eids);
    }

  }

  fprintf(file, "\n");

  pmessage->decreaseIndent();

  return;

}




void writeElementVABS(
  FILE *file, int elem_tag, std::vector<std::size_t> node_tags,
  int elem_type,
  Message *pmessage
  ) {

  fprintf(file, "%8d", eid);

  std::vector<int> inums(9, 0);

  if (elem_type == 2) {
    // 3-node triangle
    inums = {node_tags[0], node_tags[1], node_tags[2], 0, 0, 0, 0, 0, 0};
  } else if (elem_type == 3) {
    // 4-node quadrilateral
    inums = {node_tags[0], node_tags[1], node_tags[2], node_tags[3], 0, 0, 0, 0, 0};
  } else if (elem_type == 9) {
    // 6-node 2nd order triangle
    inums = {node_tags[0], node_tags[1], node_tags[2], node_tags[3], 0, node_tags[4], node_tags[5], 0, 0};
  } else if (elem_type == 10) {
    // 9-node 2nd order quadrilateral
    inums = {node_tags[0], node_tags[1], node_tags[2], node_tags[3], node_tags[4], node_tags[5], node_tags[6], node_tags[7], node_tags[8]};
  } else if (elem_type == 16) {
    // 8-node 2nd order quadrilateral
    inums = {node_tags[0], node_tags[1], node_tags[2], node_tags[3], node_tags[4], node_tags[5], node_tags[6], node_tags[7], 0};
  }

  writeNumbers(file, "%8d", inums);

  return;
}




void PModel::writeElementsVABS(FILE *file, Message *pmessage) {

  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("writing elements");

  // Write connectivity for each element
  std::vector<int> elem_types;
  std::vector<std::vector<std::size_t>> elem_tags, elem_node_tags;
  gmsh::model::mesh::getElements(
    elem_types, elem_tags, elem_node_tags, -1, -1);

  for (auto _i = 0; _i < elem_types.size(); ++_i) {

    // Get properties of the element type
    std::string etype_name;
    int etype_dim, etype_order, etype_nnodes, etype_nnodes_primary;
    std::vector<double> etype_local_coords;
    gmsh::model::mesh::getElementProperties(
      elem_types[_i], etype_name, etype_dim, etype_order,
      etype_nnodes, etype_nnodes_primary, etype_local_coords);

    for (auto _j = 0; _j < elem_tags[_i].size(); ++_j) {

      // Get the slice of the element nodes
      std::vector<std::size_t> node_tags = std::vector<std::size_t>(
        elem_node_tags[_i].begin() + _j * etype_nnodes,
        elem_node_tags[_i].begin() + (_j + 1) * etype_nnodes);

      writeElementVABS(
        file, elem_tags[_i][_j], node_tags, elem_types[_i], pmessage);

    }
  }

  // Wirte local coordinate for each element
  std::vector<double> dnums;
  for (auto f : _dcel->faces()) {

    if (f->gfaceTag() != 0) {

      // Get the element tags of the face
      std::vector<int> face_elem_types;
      std::vector<std::vector<std::size_t>> face_elem_tags, face_elem_node_tags;
      gmsh::model::mesh::getElements(
        face_elem_types, face_elem_tags, face_elem_node_tags, -1, f->gfaceTag()
      );

      for (auto _etype_tags : face_elem_tags) {
        for (auto _etag : _etype_tags) {

          fprintf(file, "%8d", _etag);
          dnums = {
            // 1.0, 0.0, 0.0, 
            f->localy1()[0], f->localy1()[1], f->localy1()[2], 
            f->localy2()[0], f->localy2()[1], f->localy2()[2], 
            0.0, 0.0, 0.0
          };
          writeNumbers(file, "%16e", dnums);

        }
      }
    }

  }

  pmessage->decreaseIndent();

  return;

}









void writeElementSC(
  FILE *file, int elem_tag, int mid,
  std::vector<std::size_t> node_tags, int elem_type,
  Message *pmessage) {
  // std::vector<int> inums(9, 0);
  // fprintf(file, "%8d%8d", model->gmodel()->getMeshElementIndex(elem), mid);
  // for (int i = 0; i < elem->getNumVertices(); ++i) {
  //   if (i < 3) {
  //     inums[i] = elem->getVertex(i)->getIndex();
  //   } else {
  //     inums[i + 1] = elem->getVertex(i)->getIndex();
  //   }
  // }
  // writeNumbers(file, "%8d", inums);
}




void PModel::writeElementsSC(FILE *file, Message *pmessage) {

  // // Write connectivity for each element
  // for (auto f : model->dcel()->faces()) {
  //   if (f->gface() != nullptr) {
  //     for (auto elem : f->gface()->triangles) {
  //       writeElementSC(file, model, elem, f->layertype()->id());
  //     }
  //   }
  // }
  // fprintf(file, "\n");

  // // Wirte local coordinate for each element
  // std::vector<double> dnums;
  // for (auto f : model->dcel()->faces()) {
  //   if (f->gface() != nullptr) {
  //     for (auto elem : f->gface()->triangles) {
  //       fprintf(file, "%8d", model->gmodel()->getMeshElementIndex(elem));
  //       dnums = {
  //         // 1.0, 0.0, 0.0, 
  //         f->localy1()[0], f->localy1()[1], f->localy1()[2], 
  //         f->localy2()[0], f->localy2()[1], f->localy2()[2], 
  //         0.0, 0.0, 0.0
  //       };
  //       writeNumbers(file, "%16e", dnums);
  //     }
  //   }
  // }
  // fprintf(file, "\n");

  return;
}









// ===================================================================


int PModel::writeGmsh(const std::string &fn_base, Message *pmessage) {
  // i_indent++;
  // pmessage->increaseIndent();

  writeGmshOpt(fn_base, pmessage);
  writeGmshGeo(fn_base, pmessage);
  writeGmshMsh(fn_base, pmessage);

  // pmessage->decreaseIndent();

  return 1;
}









int PModel::writeGmshGeo(const std::string &fn_base, Message *pmessage) {
  pmessage->increaseIndent();


  std::string fn;
  if (config.homo) {
    fn = fn_base + ".geo";
    PLOG(info) << pmessage->message("writing gmsh .geo file: " + fn);
    _gmodel->writeGEO(fn);
  }
  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    fn = fn_base + "_local.geo";
    PLOG(info) << pmessage->message("writing gmsh .geo file: " + fn);
    std::ofstream ofs_geo(fn);
    ofs_geo.close();
  }

  config.file_name_geo = fn;


  pmessage->decreaseIndent();

  return 0;
}




int PModel::writeGmshMsh(const std::string &fn_base, Message *pmessage) {
  pmessage->increaseIndent();


  // open the .opt file for extra options
  std::string fn_opt;
  if (config.homo) {
    fn_opt = fn_base + ".opt";
  }
  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    fn_opt = fn_base + "_local.opt";
  }
  // FILE *file_opt;
  // file_opt = fopen(fn_opt.c_str(), "a");


  // write .msh file
  std::string fn_msh;
  if (config.homo) {

    fn_msh = fn_base + ".msh";
    PLOG(info) << pmessage->message("writing gmsh .msh file: " + fn_msh);
    _gmodel->writeMSH(fn_msh, 2.2);

  }

  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {

    for (auto state : _local_states) {  // For each laod case

      std::string case_name = state->name();
      if (case_name == "") {
        fn_msh = fn_base + "_local_case_" + std::to_string(state->id()) + ".msh";
      }
      else {
        fn_msh = fn_base + "_local_case_" + case_name + ".msh";
      }
      PLOG(info) << pmessage->message("writing gmsh .msh file: " + fn_msh);

      FILE *f_msh, *f_opt;
      f_msh = fopen(fn_msh.c_str(), "w");
      f_opt = fopen(fn_opt.c_str(), "a");

      // Write head
      fprintf(f_msh, "$MeshFormat\n");
      fprintf(f_msh, "2.2  0  8\n");
      fprintf(f_msh, "$EndMeshFormat\n");

      // Write nodes and elements
      _pmesh->writeGmshMsh(f_msh, pmessage);
      // fclose(f_msh);


      // Write data
      // file = fopen((fn_base + "_temp.msh").c_str(), "w");
      state->writeGmshMsh(f_msh, f_opt, pmessage);
      // state->writeGmshMsh(fn_msh, fn_opt, pmessage);


      fclose(f_msh);
      fclose(f_opt);

    }

  }

  config.file_name_msh = fn_msh;

  // fclose(file_opt);


  pmessage->decreaseIndent();

  return 0;
}




int PModel::writeGmshOpt(const std::string &fn_base, Message *pmessage) {
  pmessage->increaseIndent();

  std::string fn;

  if (config.homo) {
    fn = fn_base + ".opt";
  }
  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    fn = fn_base + "_local.opt";
  }

  PLOG(info) << pmessage->message("writing gmsh .opt file: " + fn);

  FILE *file;
  file = fopen(fn.c_str(), "w");

  // General settings
  fprintf(file, "General.Axes = %d;\n", 3);
  fprintf(file, "General.BackgroundGradient = %d;\n", 0);
  fprintf(file, "General.Orthographic = %d;\n", 1);
  fprintf(file, "General.RotationX = %d;\n", 270);
  fprintf(file, "General.RotationY = %d;\n", 360);
  fprintf(file, "General.RotationZ = %d;\n", 270);
  fprintf(file, "General.ScaleX = %d;\n", 2);
  fprintf(file, "General.ScaleY = %d;\n", 2);
  fprintf(file, "General.ScaleZ = %d;\n", 2);
  fprintf(file, "General.Trackball = %d;\n", 0);
  fprintf(file, "Geometry.LineWidth = %d;\n", 1);

  // Mesh settings
  fprintf(file, "Mesh.ColorCarousel = %d;\n", 2);
  // fprintf(file, "Mesh.RemeshParametrization = %d;\n", 0);

  if (config.homo) {
    fprintf(file, "Mesh.SurfaceFaces = %d;\n", 1);
  }
  else if (config.dehomo || config.fail_strength || config.fail_index || config.fail_envelope) {
    fprintf(file, "Mesh.Points = %d;\n", 0);
    fprintf(file, "Mesh.SurfaceEdges = %d;\n", 0);
    fprintf(file, "Mesh.SurfaceFaces = %d;\n", 0);
    fprintf(file, "Mesh.VolumeEdges = %d;\n", 0);
  }


  fclose(file);
  config.file_name_opt = fn;

  pmessage->decreaseIndent();


  return 0;
}









void PMesh::writeGmshMsh(FILE *file, Message *pmessage) {
  fprintf(file, "$Nodes\n");
  // number of nodes
  fprintf(file, "%d\n", _nodes.size());
  // coordinates
  for (auto n : _nodes) {
    fprintf(file, "%8d", n->getId());
    fprintf(file, "%16e", n->getCoords()->x());
    fprintf(file, "%16e", n->getCoords()->y());
    fprintf(file, "%16e", n->getCoords()->z());
    fprintf(file, "\n");
  }
  fprintf(file, "$EndNodes\n");


  fprintf(file, "$Elements\n");
  // number of elements
  fprintf(file, "%d\n", _elements.size());
  for (auto e : _elements) {
    fprintf(file, "%8d", e->getId());
    if (e->getNodes().size() == 3) {
      fprintf(file, "%8d", 2);  // element type (3-node triangle)
    }
    else if (e->getNodes().size() == 6) {
      fprintf(file, "%8d", 9);  // element type (6-node triangle)
    }
    else if (e->getNodes().size() == 4) {
      fprintf(file, "%8d", 3);  // element type (4-node quadrangle)
    }
    else if (e->getNodes().size() == 8) {
      fprintf(file, "%8d", 16);  // element type (8-node quadrangle)
    }
    else if (e->getNodes().size() == 9) {
      fprintf(file, "%8d", 10);  // element type (9-node quadrangle)
    }
    fprintf(file, "%8d", 2);  // 2 integer tags below
    fprintf(file, "%8d%8d", 1, 1);  // physical entity, elementary entity
    for (auto n : e->getNodes()) {
      fprintf(file, "%8d", n);
    }
    fprintf(file, "\n");
  }
  fprintf(file, "$EndElements\n");

  return;
}









void LocalState::writeGmshMsh(FILE *f_msh, FILE *f_opt, Message *pmessage) {
  pmessage->increaseIndent();

  int view_id = -1;

  if (config.dehomo) {
    if (_u) {_u->writeGmshMsh(f_msh, f_opt, view_id, pmessage);}
    if (_s) {_s->writeGmshMsh(f_msh, f_opt, view_id, pmessage);}
    if (_e) {_e->writeGmshMsh(f_msh, f_opt, view_id, pmessage);}
    if (_sm) {_sm->writeGmshMsh(f_msh, f_opt, view_id, pmessage);}
    if (_em) {_em->writeGmshMsh(f_msh, f_opt, view_id, pmessage);}

    if (_sn.size() > 0) {
      for (auto sn : _sn) {
        sn->writeGmshMsh(f_msh, f_opt, view_id, pmessage);
        fprintf(f_opt, "View[%d].Group = \"%s\";\n", view_id, "Stress (global, element node)");
      }
    }
    if (_en.size() > 0) {
      for (auto en : _en) {
        en->writeGmshMsh(f_msh, f_opt, view_id, pmessage);
        fprintf(f_opt, "View[%d].Group = \"%s\";\n", view_id, "Strain (global, element node)");
      }
    }
  }

  if (config.fail_strength || config.fail_index || config.fail_envelope) {
    // if (_sr) {_sr->writeGmshMsh(file, file_opt, pmessage);}
    // if (_fi) {_fi->writeGmshMsh(file, file_opt, pmessage);}
    if (_sr) {
      _sr->writeGmshMsh(f_msh, f_opt, view_id, pmessage);
      fprintf(f_opt, "View[%d].RangeType = %d;\n", view_id, 2);
      fprintf(f_opt, "View[%d].IntervalsType = %d;\n", view_id, 3);
      fprintf(f_opt, "View[%d].NbIso = %d;\n", view_id, 20);
      fprintf(f_opt, "View[%d].CustomMin = %d;\n", view_id, 0);
      fprintf(f_opt, "View[%d].CustomMax = %d;\n", view_id, 2);
      // fprintf(f_opt, "View[%d].CustomMin = %e;\n", view_id, _sr_min);
      // fprintf(f_opt, "View[%d].CustomMax = %e;\n", view_id, 10*_sr_min);
      fprintf(f_opt, "View[%d].ColormapNumber = %d;\n", view_id, 2);
      fprintf(f_opt, "View[%d].ColormapSwap = %d;\n", view_id, 1);
      fprintf(f_opt, "View[%d].ScaleType = %d;\n", view_id, 1);
    }
    // if (_sr_min < 1) {
    //   fprintf(f_opt, "View[%d].ColormapNumber = %d;\n", view_id, 2);
    //   fprintf(f_opt, "View[%d].CustomMax = %e;\n", view_id, 2-_sr_min);
    // }
    // else {
    //   fprintf(f_opt, "View[%d].ColormapNumber = %d;\n", view_id, 17);
    //   fprintf(f_opt, "View[%d].CustomMax = %e;\n", view_id, 10*_sr_min);
    // }

    if (_fi) {
      _fi->writeGmshMsh(f_msh, f_opt, view_id, pmessage);
      fprintf(f_opt, "View[%d].ScaleType = %d;\n", view_id, 1);
    }
  }


  pmessage->decreaseIndent();

  return;
}




// void LocalState::writeGmshMsh(std::string &fn_msh, std::string &fn_opt, Message *pmessage) {
//   pmessage->increaseIndent();


//   if (config.dehomo) {
//     FILE *f_msh, *f_opt;
//     f_msh = fopen(fn_msh.c_str(), "a");
//     f_opt = fopen(fn_opt.c_str(), "a");

//     if (_u) {_u->writeGmshMsh(f_msh, f_opt, pmessage);}
//     if (_s) {_s->writeGmshMsh(f_msh, f_opt, pmessage);}
//     if (_e) {_e->writeGmshMsh(f_msh, f_opt, pmessage);}
//     if (_sm) {_sm->writeGmshMsh(f_msh, f_opt, pmessage);}
//     if (_em) {_em->writeGmshMsh(f_msh, f_opt, pmessage);}

//     fclose(f_msh);
//     fclose(f_opt);
//   }

//   if (config.fail_strength || config.fail_index || config.fail_envelope) {
//     // if (_sr) {_sr->writeGmshMsh(file, file_opt, pmessage);}
//     // if (_fi) {_em->writeGmshMsh(file, file_opt, pmessage);}
//     if (_sr) {_sr->writeGmshMsh(fn_msh, fn_opt, pmessage);}
//     // if (_fi) {_em->writeGmshMsh(fn_msh, fn_opt, pmessage);}
//   }


//   pmessage->decreaseIndent();

//   return;
// }









// void writeGmshElementData(FILE *file,
//                           const std::vector<PElementNodeData> &data,
//                           const std::vector<std::string> &labels, Message *pmessage) {
void PElementNodeData::writeGmshMsh(FILE *f_msh, FILE *f_opt, int &view_id, Message *pmessage) {

  pmessage->increaseIndent();

  // std::cout << "_label = " << _label << std::endl;


  // Write scaler/vector/tensor as a single quantity
  view_id++;
  if (_type == 0) {
    PLOG(info) << pmessage->message("writing node data: " + _label);
    fprintf(f_msh, "$NodeData\n");
  }
  else if (_type == 1) {
    PLOG(info) << pmessage->message("writing element data: " + _label);
    fprintf(f_msh, "$ElementData\n");
  }
  else if (_type == 2) {
    PLOG(info) << pmessage->message("writing element node data: " + _label);
    fprintf(f_msh, "$ElementNodeData\n");
  }
  config.gmsh_views += 1;


  // string tags
  fprintf(f_msh, "%d\n", 1);  // 1 string tag
  if (_order == 0) {
    fprintf(f_msh, "\"%s\"\n", _label.c_str());
  }
  else {
    fprintf(f_msh, "\"%s - %s\"\n", _label.c_str(), "Full field");
  }

  // real tags
  fprintf(f_msh, "%d\n", 1);  // 1 real tag
  fprintf(f_msh, "%f\n", 0.0);  // time value

  // integer tags
  fprintf(f_msh, "%d\n", 3);  // 3 integer tag
  fprintf(f_msh, "%d\n", 0);  // time step
  if (_order == 0) {fprintf(f_msh, "%d\n", 1);}  // scalar
  else if (_order == 1) {fprintf(f_msh, "%d\n", 3);}  // vector
  else if (_order == 2) {fprintf(f_msh, "%d\n", 9);}  // tensor
  fprintf(f_msh, "%d\n", _data.size());

  // data
  for (auto row : _data) {
    // write id
    fprintf(f_msh, "%8d", row->getMainId());

    // write values
    std::vector<double> datum = row->getData();
    std::vector<std::string> str_datum = row->getStringData();
    if (_type == 0 || _type == 1) {  // node data or element data
      if (_order == 0) {
        if (_label == "Strength ratio" || _label == "Failure index") {
          fprintf(f_msh, "    %s\n", str_datum[0].c_str());
        }
        else {
          fprintf(f_msh, "%16e\n", datum[0]);
        }
      }
      else if (_order == 1) {
        // fprintf(f_msh, "%8d", row->getMainId());
        writeVectorToFile(f_msh, datum);
      }
      else if (_order == 2) {
        // fprintf(f_msh, "%8d", row->getMainId());
        fprintf(f_msh, "%16e%16e%16e", datum[0], datum[1], datum[2]);
        fprintf(f_msh, "%16e%16e%16e", datum[1], datum[3], datum[4]);
        fprintf(f_msh, "%16e%16e%16e", datum[2], datum[4], datum[5]);
        fprintf(f_msh, "\n");
      }
    }

    else if (_type == 2) {  // element node data
      if (_order == 0) {  // scalar
        fprintf(f_msh, "%8d", datum.size());
        writeVectorToFile(f_msh, datum);
      }
    }

  }


  if (_type == 0) {fprintf(f_msh, "$EndNodeData\n");}
  else if (_type == 1) {fprintf(f_msh, "$EndElementData\n");}
  else if (_type == 2) {fprintf(f_msh, "$EndElementNodeData\n");}

  // options
  if (_order > 0) {
    fprintf(f_opt, "View[%d].Group = \"%s\";\n", config.gmsh_views-1, _label.c_str());
  }

  if (config.fail_index) {
    if (_label == "Strength ratio") {
      fprintf(f_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 1);
      // fprintf(f_opt, "View[%d].ColormapSwap = %d;\n", config.gmsh_views-1, 1);
      // fprintf(f_opt, "View[%d].RangeType = %d;\n", config.gmsh_views-1, 2);
    }
    else {
      fprintf(f_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
    }
    // fprintf(f_opt, "View[%d].ColormapNumber = %d;\n", config.gmsh_views-1, 2);
    // fprintf(f_opt, "View[%d].CustomMax = %e;\n", config.gmsh_views-1, 1e10);
    // fprintf(f_opt, "View[%d].CustomMin = %e;\n", config.gmsh_views-1, 1e-10);
    // fprintf(f_opt, "View[%d].IntervalsType = %d;\n", config.gmsh_views-1, 2);
    fprintf(f_opt, "View[%d].SaturateValues = %d;\n", config.gmsh_views-1, 1);
    // fprintf(f_opt, "View[%d].ScaleType = %d;\n", config.gmsh_views-1, 2);
  }
  else {
    if (_label == "Stress (global)" || _label == "SN11") {
      fprintf(f_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 1);
    }
    else {
      fprintf(f_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
    }
  }





  // Write each component individually for vector/tensor
  for (int comp{0}; comp < _component_labels.size(); ++comp) {
    PLOG(info) << pmessage->message("writing component: " + _component_labels[comp]);

    // For each strain/stress component
    view_id++;
    if (_type == 0) {fprintf(f_msh, "$NodeData\n");}
    else if (_type == 1) {fprintf(f_msh, "$ElementData\n");}
    else if (_type == 2) {fprintf(f_msh, "$ElementNodeData\n");}
    config.gmsh_views += 1;


    // string tags
    fprintf(f_msh, "%d\n", 1);  // 1 string tag
    fprintf(f_msh, "\"%s\"\n", _component_labels[comp].c_str());

    // real tags
    fprintf(f_msh, "%d\n", 1);  // 1 real tag
    fprintf(f_msh, "%f\n", 0.0);

    // integer tags
    fprintf(f_msh, "%d\n", 3);  // 3 integer tag
    fprintf(f_msh, "%d\n", 0);  // time step
    fprintf(f_msh, "%d\n", 1);
    fprintf(f_msh, "%d\n", _data.size());

    // data
    for (auto row : _data) {
      // element id
      fprintf(f_msh, "%8d", row->getMainId());

      // value
      if (_type == 0 || _type == 1) {
        fprintf(f_msh, "%16e\n", row->getData()[comp]);
      }
      // ofs_msh.width(4);
      // ofs_msh << elements[i][1];
      // for (int j{comp}; j < data[i].size(); j += 6) {
      // ofs_msh.setf(std::fstream::scientific);
      // ofs_msh.width(16);
      // ofs_msh << elem.getData()[comp] << std::endl;
      // }
      // ofs_msh;
    }


    if (_type == 0) {fprintf(f_msh, "$EndNodeData\n");}
    else if (_type == 1) {fprintf(f_msh, "$EndElementData\n");}
    else if (_type == 2) {fprintf(f_msh, "$EndElementNodeData\n");}


    // options
    fprintf(f_opt, "View[%d].Group = \"%s\";\n", config.gmsh_views-1, _label.c_str());
    fprintf(f_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
  }

  pmessage->decreaseIndent();

  return;

}









// void PElementNodeData::writeGmshMsh(std::string &fn_msh, std::string &fn_opt, Message *pmessage) {

//   pmessage->increaseIndent();

//   std::ofstream fs_msh, fs_opt;
//   fs_msh.open(fn_msh, std::fstream::out | std::fstream::app);
//   fs_opt.open(fn_opt, std::fstream::out | std::fstream::app);

//   // Write scaler/vector/tensor as a single quantity
//   if (_type == 0) {
//     PLOG(info) << pmessage->message("writing node data: " + _label);
//     // fprintf(file, "$NodeData\n");
//     fs_msh << "$NodeData" << std::endl;
//   }
//   else if (_type == 1) {
//     PLOG(info) << pmessage->message("writing element data: " + _label);
//     // fprintf(file, "$ElementData\n");
//     fs_msh << "$ElementData" << std::endl;
//   }
//   else if (_type == 2) {
//     PLOG(info) << pmessage->message("writing element node data: " + _label);
//     // fprintf(file, "$ElementNodeData\n");
//     fs_msh << "$ElementNodeData" << std::endl;
//   }
//   config.gmsh_views += 1;


//   // string tags
//   // fprintf(file, "%d\n", 1);  // 1 string tag
//   fs_msh << 1 << std::endl;
//   if (_order == 0) {
//     // fprintf(file, "\"%s\"\n", _label.c_str());
//     fs_msh << "\"" << _label << "\"" << std::endl;
//   }
//   else {
//     // fprintf(file, "\"%s - %s\"\n", _label.c_str(), "Full field");
//     fs_msh << "\"" << _label << " - Full field" << "\"" << std::endl;
//   }

//   // real tags
//   // fprintf(file, "%d\n", 1);  // 1 real tag
//   fs_msh << 1 << std::endl;
//   // fprintf(file, "%f\n", 0.0);  // time value
//   fs_msh << 0.0 << std::endl;

//   // integer tags
//   // fprintf(file, "%d\n", 3);  // 3 integer tag
//   fs_msh << 3 << std::endl;
//   // fprintf(file, "%d\n", 0);  // time step
//   fs_msh << 0 << std::endl;
//   if (_order == 0) {  // scalar
//     // fprintf(file, "%d\n", 1);
//     fs_msh << 1 << std::endl;
//   }
//   else if (_order == 1) {  // vector
//     // fprintf(file, "%d\n", 3);
//     fs_msh << 3 << std::endl;
//   }
//   else if (_order == 2) {  // tensor
//     // fprintf(file, "%d\n", 9);
//     fs_msh << 9 << std::endl;
//   }
//   // fprintf(file, "%d\n", _data.size());
//   fs_msh << _data.size() << std::endl;

//   // data
//   int nrows_buffer = 1000;
//   int row_num = 1;
//   for (auto row : _data) {
//     // write id
//     // fprintf(file, "%8d", row->getMainId());
//     // fs_msh.width(8);
//     fs_msh << row->getMainId();

//     // write values
//     std::vector<double> datum = row->getData();
//     std::vector<std::string> str_datum = row->getStringData();
//     if (_type == 0 || _type == 1) {  // node data or element data
//       if (_order == 0) {
//         // fs_msh.setf(std::fstream::scientific);
//         // fs_msh.width(16);
//         fs_msh << "    " << str_datum[0] << std::endl;
//         // if (_label == "Strength ratio") {
//         //   // PLOG(info) << pmessage->message("writing entry: " + std::to_string(row->getMainId()) + "  " + str_datum[0]);
//         //   // int nch;
//         //   // nch = fprintf(file, "    1\n");
//         //   // printf("%8d    %s\n", row->getMainId(), str_datum[0].c_str());
//         //   // nch = fprintf(file, "%8d    %s\n", row->getMainId(), str_datum[0].c_str());
//         //   // std::cout << "elm " << row->getMainId() << "  nch = " << nch << std::endl;
//         // }
//         // else {
//         //   fprintf(file, "%16e\n", datum[0]);
//         // }
//       }
//       else if (_order == 1) {
//         // fprintf(file, "%8d", row->getMainId());
//         // writeVectorToFile(file, datum);
//       }
//       else if (_order == 2) {
//         // fprintf(file, "%8d", row->getMainId());
//         // fprintf(file, "%16e%16e%16e", datum[0], datum[1], datum[2]);
//         // fprintf(file, "%16e%16e%16e", datum[1], datum[3], datum[4]);
//         // fprintf(file, "%16e%16e%16e", datum[2], datum[4], datum[5]);
//         // fprintf(file, "\n");
//       }
//     }

//     // if (row_num == nrows_buffer) {
//     //   int rv_flush = fflush(file);
//     //   std::cout << "flushed: " << rv_flush << std::endl;
//     //   row_num = 1;
//     // } else {
//     //   row_num++;
//     // }
//   }


//   if (_type == 0) {fs_msh << "$EndNodeData" << std::endl;}
//   else if (_type == 1) {fs_msh << "$EndElementData" << std::endl;}
//   else if (_type == 2) {fs_msh << "$EndElementNodeData" << std::endl;}

//   // options
//   if (_order > 0) {
//     // fprintf(file_opt, "View[%d].Group = \"%s\";\n", config.gmsh_views-1, _label.c_str());
//   }

//   if (config.fail_index) {
//     if (_label == "Strength ratio") {
//       // fprintf(file_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 1);
//     }
//     else {
//       // fprintf(file_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
//     }
//   }
//   else {
//     if (_label == "Stress (global)") {
//       // fprintf(file_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 1);
//     }
//     else {
//       // fprintf(file_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
//     }
//   }





//   // Write each component individually for vector/tensor
//   // for (int comp{0}; comp < _component_labels.size(); ++comp) {
//   //   PLOG(info) << pmessage->message("writing component: " + _component_labels[comp]);

//   //   // For each strain/stress component
//   //   if (_type == 0) {fprintf(file, "$NodeData\n");}
//   //   else if (_type == 1) {fprintf(file, "$ElementData\n");}
//   //   else if (_type == 2) {fprintf(file, "$ElementNodeData\n");}
//   //   config.gmsh_views += 1;


//   //   // string tags
//   //   fprintf(file, "%d\n", 1);  // 1 string tag
//   //   fprintf(file, "\"%s\"\n", _component_labels[comp].c_str());

//   //   // real tags
//   //   fprintf(file, "%d\n", 1);  // 1 real tag
//   //   fprintf(file, "%f\n", 0.0);

//   //   // integer tags
//   //   fprintf(file, "%d\n", 3);  // 3 integer tag
//   //   fprintf(file, "%d\n", 0);  // time step
//   //   fprintf(file, "%d\n", 1);
//   //   fprintf(file, "%d\n", _data.size());

//   //   // data
//   //   for (auto row : _data) {
//   //     // element id
//   //     fprintf(file, "%8d", row->getMainId());

//   //     // value
//   //     if (_type == 0 || _type == 1) {
//   //       fprintf(file, "%16e\n", row->getData()[comp]);
//   //     }
//   //     // ofs_msh.width(4);
//   //     // ofs_msh << elements[i][1];
//   //     // for (int j{comp}; j < data[i].size(); j += 6) {
//   //     // ofs_msh.setf(std::fstream::scientific);
//   //     // ofs_msh.width(16);
//   //     // ofs_msh << elem.getData()[comp] << std::endl;
//   //     // }
//   //     // ofs_msh;
//   //   }


//   //   if (_type == 0) {fprintf(file, "$EndNodeData\n");}
//   //   else if (_type == 1) {fprintf(file, "$EndElementData\n");}
//   //   else if (_type == 2) {fprintf(file, "$EndElementNodeData\n");}


//   //   // options
//   //   fprintf(file_opt, "View[%d].Group = \"%s\";\n", config.gmsh_views-1, _label.c_str());
//   //   fprintf(file_opt, "View[%d].Visible = %d;\n", config.gmsh_views-1, 0);
//   // }

//   fs_msh.close();

//   pmessage->decreaseIndent();

// }





