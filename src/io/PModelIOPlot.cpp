#include "PModel.hpp"
#include "PModelIO.hpp"

#include "globalVariables.hpp"
#include "utilities.hpp"
#include "plog.hpp"

void PModel::plotDehomo() {
  MESSAGE_SCOPE(g_msg);

  g_msg->printBlank();
  PLOG(info) << g_msg->message("post-processing recover results");


  std::string fn_base = config.file_directory + config.file_base_name;

  // if (config.analysis_tool == 1) {
    // postVABS(pmessage);
  // Read sg input (.sg)
  WriterConfig wcfg{config.tool, config.isDehomo(), config.tool_ver, config.file_name_vsc};
  readSG(config.file_name_vsc, this, wcfg);

  // Read dehomogenization output
  readOutputDehomo(config.file_name_vsc, this);

  // Write Gmsh
  writeGmsh(fn_base);
  // }
  // else if (config.analysis_tool == 2) {
  //   postSCDehomo();
  // }



  PLOG(info) << g_msg->message("post-processing recover results -- done");
  g_msg->printBlank();

  return;
}

