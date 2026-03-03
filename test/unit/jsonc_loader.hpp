#pragma once
// Simple module for loading JSON/JSONC files (JSON with // and /* */ comments).
// Requires nlohmann JSON >= 3.9.

#include <fstream>
#include <stdexcept>
#include <string>

#include "nlohmann/json.hpp"

// Read a JSON or JSONC file and return the parsed document.
// Comments are stripped automatically (nlohmann 3.9+ feature).
// Throws std::runtime_error if the file cannot be opened.
// Throws nlohmann::json::parse_error on malformed JSON.
inline nlohmann::json loadJsonc(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    throw std::runtime_error("loadJsonc: cannot open file: " + path);
  }
  std::string content(
    (std::istreambuf_iterator<char>(f)),
     std::istreambuf_iterator<char>());
  return nlohmann::json::parse(content,
    /*callback=*/nullptr,
    /*allow_exceptions=*/true,
    /*ignore_comments=*/true);
}
