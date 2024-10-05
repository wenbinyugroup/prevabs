#include "execu.hpp"
#include "plog.hpp"
#include "PModel.hpp"
#include "utilities.hpp"

// #include <gmsh/StringUtils.h>
#include <gmsh_mod/StringUtils.h>
// #include "gmsh/MTriangle.h"

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

const int NE_1D = 4;





// extern "C" {
//   void constitutivemodeling_(char *inp_name, int *format_I, int mat_type_layer[], double layup_angle[], int *LAY_CONST, int *nlayer,\
//   int *Timoshenko_I, int *curved_I, int *oblique_I, int *trapeze_I, int *Vlasov_I, int *damping_I, double kb[], double beta[],\
//   int *nnode, int *nelem, int *nmate,  double *coord, int *element, double *layup, int mat_type[], double *material, int orth[], double density[], double damping[],\
//   double damping_layer[], double mass[][6], double *area, double *xm2, double *xm3, double mass_mc[][6], double *I22, double *I33, double *mass_angle, double *Xg2, double *Xg3, double Aee[][NE_1D], double Aee_F[][NE_1D], double *Xe2,\
//   double *Xe3, double Aee_k[][NE_1D], double Aee_k_F[][NE_1D], double Aee_damp[][NE_1D], double *Xe2_k, double *Xe3_k, double ST[][6], double ST_F[][6], double ST_damp[][6], double *Sc1, double *Sc2, double stiff_val[][5],\
//   double stiff_val_F[][5], double stiff_val_damp[][5], double Ag1[][4], double Bk1[][4], double Ck2[][4], double Dk3[][4], int *thermal_I, double *cte, double temperature[], double NT[], double NT_F[], char *error,int len1, int len2);

//   void output_(char *inp_name, double mass[][6], double *area, double *xm2, double *xm3, double mass_mc[][6], double *I22, double *I33, double *mass_angle, double *Xg2, double *Xg3,\
//   double Aee[][NE_1D], double Aee_F[][NE_1D], double *Xe2, double *Xe3, double Aee_k[][NE_1D], double Aee_k_F[][NE_1D], double Aee_damp[][NE_1D], double *Xe2_k, double *Xe3_k, double ST[][6], double ST_F[][6], double ST_damp[][6], double *Sc1, double *Sc2,\
//   double stiff_val[][5], double stiff_val_F[][5], double stiff_val_damp[][5], double Ag1[][4], double Bk1[][4], double Ck2[][4], double Dk3[][4], int *thermal_I, double *cte, double temperature[],\
//   double NT[], double NT_F[], int *Vlasov_I, int *damping_I, int *curved_I, int *Timoshenko_I, int *trapeze_I, char *error);
// }









#ifdef __linux__
std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
      // if (fgets(buffer.data(), 128, pipe.get()))
      result += buffer.data();
  }
  return result;
}
#endif









void runVABS(const std::string &file_name, const std::vector<std::string> &args, Message *pmessage) {
  // PLOG(info) << "running VABS...";

  std::vector<std::string> vs;
  vs = SplitFileName(file_name);
  std::string s_cmd;
  s_cmd = "\"" + vs[1] + vs[2] + "\"";
  for (auto arg : args) {
    s_cmd = s_cmd + " " + arg;
  }

  // std::string fn_all = fn_geo + " " + fn_msh + " " + fn_opt;


#ifdef __linux__
  // s_cmd = "VABS " + s_cmd;
  s_cmd = "VABS \"" + file_name + "\"";
  for (auto arg : args) {
    s_cmd = s_cmd + " " + arg;
  }
  PLOG(info) << pmessage->message("running: VABS " + s_cmd);
  std::string result;
  result = exec(s_cmd.c_str());
  std::cout << result << std::endl;
#elif _WIN32
  SHELLEXECUTEINFO ShExecInfo = {0};
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
  ShExecInfo.hwnd = NULL;
  ShExecInfo.lpVerb = NULL;
  ShExecInfo.lpFile = "vabs";
  ShExecInfo.lpParameters = s_cmd.c_str();
  ShExecInfo.lpDirectory = vs[0].c_str();
  ShExecInfo.nShow = SW_SHOW;
  ShExecInfo.hInstApp = NULL;
  ShellExecuteEx(&ShExecInfo);
  WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
#endif

  return;
}









void runSC(const std::string &file_name, const std::vector<std::string> &args, Message *pmessage) {

  // common func;
  std::vector<std::string> vs;
  vs = SplitFileName(file_name);
  std::string s_cmd;
  s_cmd = "\"" + vs[1] + vs[2] + "\"";
  for (auto arg : args) {
    s_cmd = s_cmd + " " + arg;
  }

  PLOG(info) << pmessage->message("running: SwfitComp " + s_cmd);

#ifdef __linux__
  s_cmd = "SwiftComp " + s_cmd;
  std::string result;
  result = exec(s_cmd.c_str());
  std::cout << result << std::endl;
#elif _WIN32
  SHELLEXECUTEINFO ShExecInfo = {0};
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
  ShExecInfo.hwnd = NULL;
  ShExecInfo.lpVerb = NULL;
  ShExecInfo.lpFile = "swiftcomp";
  ShExecInfo.lpParameters = s_cmd.c_str();
  ShExecInfo.lpDirectory = vs[0].c_str();
  ShExecInfo.nShow = SW_SHOW;
  ShExecInfo.hInstApp = NULL;
  ShellExecuteEx(&ShExecInfo);
  WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
#endif

  return;
}









void runIntegratedVABS(const std::string &file_name, PModel *model, Message *pmessage) {
//   // VABS input filename
//   char inp_name[256];
//   const int MAX_LEN1 = 256;
//   const int MAX_LEN2 = 300;
//   memset(inp_name, ' ', MAX_LEN1);
//   file_name.copy(inp_name, MAX_LEN1);

//   // VABS new format input
//   int format_I = 1;

//   // number of layer types
//   int nlayer =  model->cs()->getNumOfUsedLayerTypes();
//   // material type and layup angle for each layer type;
//   int mat_type_layer[nlayer];
//   double layup_angle[nlayer];
  
//   int index = 0;
//   for (auto lt : model->cs()->getUsedLayerTypes()) {
//     mat_type_layer[index] = lt->material()->id();
//     layup_angle[index] = lt->angle();
//     index++;
//   }
  
//   // a parameter corresponds to new format of VABS
//   int LAY_CONST = 1;

//   // some control parameters
//   int Timoshenko_I = model->analysisModel();
//   int curved_I = model->analysisCurvature();
//   int oblique_I = model->analysisOblique();
//   int trapeze_I = model->analysisTrapeze();
//   int Vlasov_I = model->analysisVlasov();
//   int damping_I = 0;
//   int thermal_I = 0;

//   // initial curvatures
//   double kb[3];
//   std::copy(model->curvatures().begin(),model->curvatures().end(),kb);

//   // obliques
//   double beta[3];
//   std::copy(model->obliques().begin(),model->obliques().end(),beta);

//   // number of nodes
//   int nnode = model->gmodel()->indexMeshVertices(true,0);

//   // number of elements
//   int nelem = model->indexGmshElements();

//   // number of materials
//   int nmate = model->cs()->getNumOfUsedMaterials();

//   // coordinates of nodes; transposed
//   double coord[2][nnode];
  
//   index = 0;
//   std::vector<GEntity *> gentities;
//   model->gmodel()->getEntities(gentities);
//   for (unsigned int i = 0; i < gentities.size(); ++i) {
//     for (unsigned int j = 0; j < gentities[i]->mesh_vertices.size(); ++j) {
//       if (gentities[i]->mesh_vertices[j]->getIndex() > 0) {
//                 coord[0][index] = gentities[i]->mesh_vertices[j]->y();
//                 coord[1][index] = gentities[i]->mesh_vertices[j]->z();
//                 index++;
//       } else {
//         continue;
//       }
//     }
//   }

//   // connectivity of elements; transposed
//   int element[9][nelem];
//   memset(element, 0, sizeof(element));
//   // orientation of elements; transposed
//   double layup[LAY_CONST][nelem];
//   // layup type of elements;
//   int mat_type[nelem];

//   index = 0;
//   for (auto f : model->dcel()->faces()) {
//     if (f->gface() != nullptr) {
//       for (auto elem : f->gface()->triangles) {
//         layup[0][index] = f->theta1deg();
//         mat_type[index] = f->layertype()->id();

//         for (unsigned int i = 0; i < 3; ++i) {
//           element[i][index] = elem->getVertex(i)->getIndex();
//         }
//         index++;
//       }
//     }
//   }

//   // material stiffness
//   double material[21][nmate];
//   memset(material, 0, sizeof(material));
//   // isotropy
//   int orth[nmate];
//   // density
//   double density[nmate];
//   index = 0;

//   for (auto m : model->cs()->getUsedMaterials()) {
//     std::vector<double> melastic = m->getElastic();
//     for (unsigned int i = 0; i < melastic.size(); i++) {
//       material[i][index] = melastic[i];
//     }
//     if (m->getType() == "isotropic") {
//       orth[index] = 0;
//     } else if (m->getType() == "orthotropic") {
//       orth[index] = 1;
//     } else if (m->getType() == "anisotropic") {
//       orth[index] = 2;
//     }
//     density[index] = m->getDensity();
//     index++;
//   }
//   // unused features
//   double damping[nmate];
//   double damping_layer[nlayer];
//   double cte[6][nmate];
//   double temperature[nnode];

//   // output variables
//   double mass[6][6];
//   double area;
//   double xm2, xm3;
//   double mass_mc[6][6];
//   double I22, I33;
//   double mass_angle;
//   double Xg2, Xg3;
//   double Aee[NE_1D][NE_1D], Aee_F[NE_1D][NE_1D];
//   double Xe2, Xe3;
//   double Aee_k[NE_1D][NE_1D], Aee_k_F[NE_1D][NE_1D];
//   double Aee_damp[NE_1D][NE_1D];
//   double Xe2_k, Xe3_k;
//   double ST[6][6], ST_F[6][6];
//   double ST_damp[6][6];
//   double Sc1, Sc2;
//   double stiff_val[5][5], stiff_val_F[5][5];
//   double stiff_val_damp[5][5];
//   double Ag1[4][4], Bk1[4][4], Ck2[4][4], Dk3[4][4];
//   double NT[4];
//   double NT_F[4];
//   char error[MAX_LEN2];
//   memset(error, ' ', MAX_LEN2);

//   // Finally, call the Fortran library function
//   constitutivemodeling_(inp_name, &format_I, mat_type_layer, layup_angle, &LAY_CONST, &nlayer,\
//   &Timoshenko_I, &curved_I, &oblique_I, &trapeze_I, &Vlasov_I, &damping_I, kb, beta,\
//   &nnode, &nelem, &nmate,  &(coord[0][0]), &(element[0][0]), &(layup[0][0]), mat_type, &(material[0][0]), orth, density, damping,\
//   damping_layer, mass, &area, &xm2, &xm3, mass_mc, &I22, &I33, &mass_angle, &Xg2, &Xg3, Aee, Aee_F, &Xe2,\
//   &Xe3, Aee_k, Aee_k_F, Aee_damp, &Xe2_k, &Xe3_k, ST, ST_F, ST_damp, &Sc1, &Sc2, stiff_val,\
//   stiff_val_F, stiff_val_damp, Ag1, Bk1, Ck2, Dk3, &thermal_I, &(cte[0][0]), temperature, NT, NT_F, error, MAX_LEN1, MAX_LEN2);
  
//   std::string error_string(error);

//   if (error[0] != ' ') {
//     std::cout << error_string << std::endl; 
//     exit(1);
//   }

//   output_(inp_name, mass, &area, &xm2, &xm3, mass_mc, &I22, &I33, &mass_angle, &Xg2, &Xg3,\
//   Aee, Aee_F, &Xe2, &Xe3, Aee_k, Aee_k_F, Aee_damp, &Xe2_k, &Xe3_k, ST, ST_F, ST_damp, &Sc1, &Sc2,\
//   stiff_val, stiff_val_F, stiff_val_damp, Ag1, Bk1, Ck2, Dk3, &thermal_I, &(cte[0][0]), temperature,\
//   NT, NT_F, &Vlasov_I, &damping_I, &curved_I, &Timoshenko_I, &trapeze_I, error);

  return;

}









void runGmsh(const std::string &fn_geo, const std::string &fn_msh,
             const std::string &fn_opt, Message *pmessage) {

  std::string fn_all = fn_geo + " " + fn_msh + " " + fn_opt;

  PLOG(info) << pmessage->message("running: gmsh " + fn_all);

#ifdef __linux__
  std::string s_cmd, result;
  s_cmd = "gmsh " + fn_all;
  result = exec(s_cmd.c_str());
#elif _WIN32
  ShellExecute(NULL, "open", "gmsh.exe", fn_all.c_str(), NULL, SW_SHOWNORMAL);
#endif

  return;
}
