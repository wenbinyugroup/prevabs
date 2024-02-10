#include "PModel.hpp"
#include "PModelIO.hpp"

#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

void PModel::plotDehomo(Message *pmessage) {
  pmessage->printBlank();
  PLOG(info) << pmessage->message("post-processing recover results");


  std::string fn_base = config.file_directory + config.file_base_name;

  // if (config.analysis_tool == 1) {
    // postVABS(pmessage);
  // Read sg input (.sg)
  readSG(config.file_name_vsc, this, pmessage);

  // Read dehomogenization output
  readOutputDehomo(config.file_name_vsc, this, pmessage);

  // Write Gmsh
  writeGmsh(fn_base, pmessage);
  // }
  // else if (config.analysis_tool == 2) {
  //   postSCDehomo();
  // }



  PLOG(info) << pmessage->message("post-processing recover results -- done");
  pmessage->printBlank();

  return;
}

