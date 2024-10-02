#include <stdio.h>
#if defined(__CYGWIN__)
#include <sys/cygwin.h>
#endif
#include "gmsh_old/StringUtils.h"
// #include "GmshMessage.h"
#include "OS.h"


std::vector<std::string> SplitFileName(const std::string &fileName)
{
  // JFR DO NOT CHANGE TO std::vector<std::string> s(3), it segfaults while
  // destructor si called
  std::vector<std::string> s; s.resize(3);
  if(fileName.size()){
    // returns [path, baseName, extension]
    int idot = (int)fileName.find_last_of('.');
    int islash = (int)fileName.find_last_of("/\\");
    if(idot == (int)std::string::npos) idot = -1;
    if(islash == (int)std::string::npos) islash = -1;
    if(idot > 0)
      s[2] = fileName.substr(idot);
    if(islash > 0)
      s[0] = fileName.substr(0, islash + 1);
    s[1] = fileName.substr(s[0].size(), fileName.size() - s[0].size() - s[2].size());
  }
  return s;
}
