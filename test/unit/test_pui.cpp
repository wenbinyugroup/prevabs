#include "catch_amalgamated.hpp"

#include "pui.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// Capture std::cout into a stringstream for the duration of the scope.
class CoutCapture {
 public:
  CoutCapture() : _saved(std::cout.rdbuf(_buf.rdbuf())) {}
  ~CoutCapture() { std::cout.rdbuf(_saved); }

  std::string str() const { return _buf.str(); }

 private:
  std::stringstream  _buf;
  std::streambuf*    _saved;
};

// Read a file's full contents.
std::string readFile(const std::string &path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Generate a unique temporary log file path.
std::string tempLogPath() {
  static int counter = 0;
  std::string path = std::string("test_pui_") + std::to_string(++counter) + ".log";
  std::remove(path.c_str());
  return path;
}

}  // namespace


TEST_CASE("pui: plain output (no color) carries fixed prefix",
          "[pui]") {
  pui::init();
  pui::setColorEnabled(false);

  CoutCapture cap;
  pui::info("hello");
  pui::success("done");
  pui::warn("careful");
  pui::error("nope");

  const std::string out = cap.str();

  CHECK(out.find("==> hello") != std::string::npos);
  CHECK(out.find("OK  done")  != std::string::npos);
  CHECK(out.find("!!  careful") != std::string::npos);
  CHECK(out.find("xx  nope")  != std::string::npos);

  // No ANSI escape sequences (no ESC byte) in plain mode.
  CHECK(out.find('\x1b') == std::string::npos);

  pui::shutdown();
}


TEST_CASE("pui: color mode injects ANSI escape sequences",
          "[pui]") {
  pui::init();
  pui::setColorEnabled(true);

  CoutCapture cap;
  pui::info("colored");
  const std::string out = cap.str();

  // ANSI escape character is 0x1B; appears at least once in colored output.
  CHECK(out.find('\x1b') != std::string::npos);
  // The message body still appears.
  CHECK(out.find("colored") != std::string::npos);

  pui::shutdown();
}


TEST_CASE("pui: title bold-only, no prefix",
          "[pui]") {
  pui::init();
  pui::setColorEnabled(false);

  CoutCapture cap;
  pui::title("PreVABS 2.0.0");
  const std::string out = cap.str();

  // Plain title: no ASCII prefix decoration.
  CHECK(out.find("==>") == std::string::npos);
  CHECK(out.find("OK ") == std::string::npos);
  CHECK(out.find("PreVABS 2.0.0") != std::string::npos);

  pui::shutdown();
}


TEST_CASE("pui: messages mirror to log file without color codes",
          "[pui]") {
  const std::string path = tempLogPath();
  pui::init(path);
  pui::setColorEnabled(true);   // even with color, file must stay plain

  CoutCapture cap;
  pui::info("read airfoil");
  pui::success("homogenization done");

  pui::shutdown();   // closes the file

  const std::string contents = readFile(path);

  CHECK(contents.find("==> read airfoil") != std::string::npos);
  CHECK(contents.find("OK  homogenization done") != std::string::npos);
  CHECK(contents.find('\x1b') == std::string::npos);

  std::remove(path.c_str());
}


TEST_CASE("pui::Stream: collects << output and emits on destruction",
          "[pui]") {
  pui::init();
  pui::setColorEnabled(false);

  CoutCapture cap;
  {
    PUI_INFO << "loading " << 42 << " points";
  }
  const std::string out = cap.str();

  CHECK(out.find("==> loading 42 points") != std::string::npos);

  pui::shutdown();
}


TEST_CASE("pui: setColorEnabled is observable via isColorEnabled",
          "[pui]") {
  pui::init();

  pui::setColorEnabled(true);
  CHECK(pui::isColorEnabled() == true);

  pui::setColorEnabled(false);
  CHECK(pui::isColorEnabled() == false);

  pui::shutdown();
}
