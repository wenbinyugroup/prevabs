#pragma once

#include <string>
#include "PModel.hpp"
#include "utilities.hpp"

void runVABS(const std::string &, const std::vector<std::string> &, Message *);
void runSC(const std::string &, const std::vector<std::string> &, Message *);
void runGmsh(const std::string &geo, const std::string &msh, const std::string &opt, Message *);
void runIntegratedVABS(const std::string &, PModel *, Message *);
