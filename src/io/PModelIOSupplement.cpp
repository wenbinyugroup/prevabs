#include "PModel.hpp"
#include "PModelIO.hpp"

#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"

#include "gmsh/GVertex.h"
#include "gmsh/GEdge.h"
#include "gmsh/MVertex.h"

#include <fstream>
#include <string>
#include <vector>
#include <utility>


int PModel::writeSupp(Message *pmessage) {
    pmessage->increaseIndent();

    PLOG(info) << pmessage->message("writing supplement files...");

    writeInterfacePairs(this, pmessage);
    writeInterfaceNodes(this, pmessage);
    writeNodeElements(this, pmessage);

    pmessage->decreaseIndent();

    return 1;
}




void writeInterfaceNodes(PModel *pmodel, Message *pmessage) {
    pmessage->increaseIndent();

    std::string fn = config.file_directory + config.file_base_name + "_interface_nodes.dat";
    PLOG(info) << pmessage->message("writing interface nodes id to file: " + fn);

    FILE *p_file = fopen(fn.c_str(), "w");

    int i_itf = 0;
    for (auto itf_hes : pmodel->getInterfaceHalfEdges()) {
        i_itf++;
        fprintf(p_file, "%8d", i_itf);

        std::vector<int> mv_id_on_vertex;
        for (auto he : itf_hes) {

            // Nodes on the edge
            for (auto mv : he->gedge()->mesh_vertices) {
                fprintf(p_file, "%8d", mv->getIndex());
            }

            // Node at the source vertex
            int nid0 = he->source()->gvertex()->mesh_vertices[0]->getIndex();
            bool found = false;
            for (auto nid : mv_id_on_vertex) {
                if (nid == nid0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                fprintf(p_file, "%8d", nid0);
                mv_id_on_vertex.push_back(nid0);
            }

            // Node at the target vertex
            int nid1 = he->twin()->source()->gvertex()->mesh_vertices[0]->getIndex();
            found = false;
            for (auto nid : mv_id_on_vertex) {
                if (nid == nid1) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                fprintf(p_file, "%8d", nid1);
                mv_id_on_vertex.push_back(nid1);
            }

        }

        fprintf(p_file, "\n");
        // }
    }

    fclose(p_file);

    pmessage->decreaseIndent();

    return;
}




void writeInterfacePairs(PModel *pmodel, Message *pmessage) {
    pmessage->increaseIndent();

    std::string fn = config.file_directory + config.file_base_name + "_interface_pairs.dat";
    PLOG(info) << pmessage->message("writing interface pairs to file: " + fn);

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

    pmessage->decreaseIndent();

    return;
}



void writeNodeElements(PModel *pmodel, Message *pmessage) {
    pmessage->increaseIndent();

    std::string fn = config.file_directory + config.file_base_name + "_node_elements.dat";
    PLOG(info) << pmessage->message("writing node elements to file: " + fn);

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

    pmessage->decreaseIndent();

    return;
}
