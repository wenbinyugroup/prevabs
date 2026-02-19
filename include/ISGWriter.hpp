#pragma once

#include "globalConstants.hpp"

#include <cstdio>
#include <memory>

class PModel;
struct WriterConfig;

// Interface for writing SG (structural genome) input files in a specific solver format.
// Concrete implementations (VABSWriter, SwiftCompWriter) handle format-specific output.
//
// Usage:
//   auto writer = makeSGWriter(wcfg.tool);
//   writer->writeSettings(file, model, wcfg);
//   writer->writeMaterials(file, model);
//   writer->writeExtra(file, model);   // omega or any format-specific trailer

class ISGWriter {
public:
  virtual void writeSettings(FILE* file, PModel* model, const WriterConfig& wcfg) = 0;
  virtual void writeMaterials(FILE* file, PModel* model) = 0;
  // Write any format-specific trailing data after the materials block.
  // No-op in formats that have no trailer.
  virtual void writeExtra(FILE* file, PModel* model) = 0;
  virtual ~ISGWriter() = default;
};

// Factory: construct the correct ISGWriter for the given tool.
std::unique_ptr<ISGWriter> makeSGWriter(AnalysisTool tool);
