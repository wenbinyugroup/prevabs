#pragma once

#include <string>
#include "PModel.hpp"
#include "utilities.hpp"

void runVABS(const std::string &, const std::vector<std::string> &);
void runSC(const std::string &, const std::vector<std::string> &);
void runGmsh(const std::string &geo, const std::string &msh, const std::string &opt);
