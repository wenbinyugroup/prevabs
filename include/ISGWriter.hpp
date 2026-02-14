#pragma once

#include <cstdio>
#include <memory>

class PModel;
struct WriterConfig;

// Interface for writing SG (structural genome) input files in a specific solver format.
// Concrete implementations (VABSWriter, SwiftCompWriter) handle format-specific output,
// eliminating analysis_tool == N conditional branches from the write path.
//
// Usage:
//   auto writer = makeSGWriter(wcfg.analysis_tool);
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

// Factory: construct the correct ISGWriter for the given analysis_tool code.
// analysis_tool == 1 → VABSWriter; analysis_tool == 2 → SwiftCompWriter.
std::unique_ptr<ISGWriter> makeSGWriter(int analysis_tool);
