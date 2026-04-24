#include "PModel.hpp"
#include "PModelIO.hpp"

#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"

// #include "gmsh/GVertex.h"
// #include "gmsh/GEdge.h"
// #include "gmsh/MVertex.h"
#include <gmsh.h>

#include <fstream>
#include <string>
#include <vector>
#include <utility>


int PModel::writeSupp() {
    PLOG(info) << "writing supplement files...";

    writeInterfacePairs(this);
    writeInterfaceNodes(this);
    writeNodeElements(this);

    return 1;
}




void writeInterfaceNodes(PModel *pmodel) {
    std::string fn = config.file_directory + config.file_base_name + "_interface_nodes.dat";
    PLOG(info) << "writing interface nodes id to file: " << fn;

    FILE *p_file = fopen(fn.c_str(), "w");

    int i_itf = 0;
    for (auto itf_hes : pmodel->getInterfaceHalfEdges()) {
        i_itf++;
        fprintf(p_file, "%8d", i_itf);

        // std::vector<int> mv_id_on_vertex;
        std::vector<std::size_t> done_node_tags;

        for (auto he : itf_hes) {

            // Nodes on the edge
            std::vector<std::size_t> edge_node_tags;
            std::vector<double> edge_node_coords, edge_node_param_coords;
            int _edge_tag = pmodel->getGmshEdgeTag(he);
            gmsh::model::mesh::getNodes(
                edge_node_tags, edge_node_coords, edge_node_param_coords,
                1, _edge_tag, false, false
            );

            // for (std::size_t _ntag : edge_node_tags) {
            for (auto _i = 0; _i < edge_node_tags.size(); ++_i) {

                std::size_t _ntag = edge_node_tags[_i];

                // Check if the node is already written
                bool found = false;
                // for (std::size_t dntag : done_node_tags) {
                for (auto _j = 0; _j < done_node_tags.size(); ++_j) {
                    std::size_t dntag = done_node_tags[_j];
                    if (_ntag == dntag) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    done_node_tags.push_back(_ntag);
                    fprintf(p_file, "%8zd", _ntag);
                }

            }

            // // Node at the source vertex
            // int nid0 = he->source()->gvertex()->mesh_vertices[0]->getIndex();
            // bool found = false;
            // for (auto nid : mv_id_on_vertex) {
            //     if (nid == nid0) {
            //         found = true;
            //         break;
            //     }
            // }
            // if (!found) {
            //     fprintf(p_file, "%8d", nid0);
            //     mv_id_on_vertex.push_back(nid0);
            // }

            // // Node at the target vertex
            // int nid1 = he->twin()->source()->gvertex()->mesh_vertices[0]->getIndex();
            // found = false;
            // for (auto nid : mv_id_on_vertex) {
            //     if (nid == nid1) {
            //         found = true;
            //         break;
            //     }
            // }
            // if (!found) {
            //     fprintf(p_file, "%8d", nid1);
            //     mv_id_on_vertex.push_back(nid1);
            // }

        }

        fprintf(p_file, "\n");

    }

    fclose(p_file);

    return;
}




void writeInterfacePairs(PModel *pmodel) {
    std::string fn = config.file_directory + config.file_base_name + "_interface_pairs.dat";
    PLOG(info) << "writing interface pairs to file: " << fn;

    FILE *p_file = fopen(fn.c_str(), "w");

    std::vector<std::pair<int, int>> mat_pairs = pmodel->getInterfaceMaterialPairs();
    std::vector<std::pair<double, double>> th1_pairs = pmodel->getInterfaceTheta1Pairs();
    std::vector<std::pair<double, double>> th3_pairs = pmodel->getInterfaceTheta3Pairs();

    for (auto i_itf = 0; i_itf < mat_pairs.size(); i_itf++) {
        fprintf(p_file, "%8d", i_itf+1);

        fprintf(p_file, "%8d%8d", mat_pairs[i_itf].first, mat_pairs[i_itf].second);
        fprintf(p_file, "%16.4e%16.4e", th3_pairs[i_itf].first, th3_pairs[i_itf].second);
        fprintf(p_file, "%16.4e%16.4e", th1_pairs[i_itf].first, th1_pairs[i_itf].second);

        fprintf(p_file, "\n");
    }

    fclose(p_file);

    return;
}



void writeNodeElements(PModel *pmodel) {
    std::string fn = config.file_directory + config.file_base_name + "_node_elements.dat";
    PLOG(info) << "writing node elements to file: " << fn;

    FILE *p_file = fopen(fn.c_str(), "w");

    // std::vector<std::vector<int>> nd_els = pmodel->getNodeElements();

    int nid = 0;
    for (auto els : pmodel->node_elements) {
        nid++;
        fprintf(p_file, "%8d", nid);
        for (auto eid : els) {
            fprintf(p_file, "%8d", eid);
        }
        fprintf(p_file, "\n");
    }

    fclose(p_file);

    return;
}
