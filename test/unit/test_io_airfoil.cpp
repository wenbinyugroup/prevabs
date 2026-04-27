#include "catch_amalgamated.hpp"

#include "jsonc_loader.hpp"
#include "PBaseLine.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"
#include "nlohmann/json.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace {

void parseXmlDocument(
  const std::string &xml,
  std::vector<char> &buffer,
  rapidxml::xml_document<> &doc
) {
  buffer.assign(xml.begin(), xml.end());
  buffer.push_back('\0');
  doc.parse<0>(buffer.data());
}

void readAirfoilLineXml(
  const std::string &file,
  const std::string &format,
  int header_rows,
  PModel &model,
  Baseline &line,
  const std::string &line_extras = ""
) {

  const std::string xml =
    "<line name=\"" + line.getName() + "\" type=\"airfoil\">"
      "<points data=\"file\" format=\"" + format + "\" header=\""
        + std::to_string(header_rows) + "\">"
        + file +
      "</points>"
      + line_extras +
    "</line>";

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  line.setType("airfoil");
  readLineTypeAirfoil(&line, doc.first_node("line"), nullptr, &model, 1e-12);
}

std::string airfoilDataDir() {
  return std::string(TEST_DATA_DIR) + "/../data/airfoils/";
}

std::string airfoilIndexPath() {
  return airfoilDataDir() + "index.json";
}

int headerRowsForFormat(const std::string &format) {
  const std::string normalized = lowerString(trim(format));
  if (normalized == "selig") {
    return 1;
  }
  if (normalized == "lednicer") {
    return 2;
  }
  throw std::runtime_error("Unknown indexed airfoil format: " + format);
}

std::string fileStem(const std::string &file) {
  std::size_t slash = file.find_last_of("/\\");
  std::size_t start = (slash == std::string::npos) ? 0 : slash + 1;
  std::size_t dot = file.find_last_of('.');
  if (dot == std::string::npos || dot < start) {
    dot = file.size();
  }
  return file.substr(start, dot - start);
}

std::string sanitizeName(const std::string &value) {
  std::string out;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (std::isalnum(c)) {
      out.push_back(static_cast<char>(std::tolower(c)));
    } else {
      out.push_back('_');
    }
  }
  return out;
}

const nlohmann::json &airfoilIndex() {
  static const nlohmann::json index = loadJsonc(airfoilIndexPath());
  return index;
}

}  // namespace

TEST_CASE("readLineTypeAirfoil: indexed standard airfoils parse into closed baselines",
          "[io][airfoil][dataset]") {
  config.file_directory = airfoilDataDir();

  const nlohmann::json &index = airfoilIndex();
  REQUIRE(index.is_array());
  REQUIRE_FALSE(index.empty());

  for (std::size_t i = 0; i < index.size(); ++i) {
    const std::string file = index[i].at("file").get<std::string>();
    const std::string format = index[i].at("format").get<std::string>();
    const std::string line_name = "af_" + sanitizeName(fileStem(file));

    DYNAMIC_SECTION("file=" << file << ", format=" << format) {
      PModel model;
      Baseline line(line_name, "airfoil");

      readAirfoilLineXml(
        file, format, headerRowsForFormat(format), model, line
      );

      REQUIRE(line.vertices().size() > 10);
      CHECK(line.vertices().front() == line.vertices().back());
      CHECK(line.vertices().front()->name() == line_name + "_te");
      CHECK(model.getPointByName(line_name + "_le") != nullptr);
      CHECK(model.getPointByName(line_name + "_te") != nullptr);
    }
  }
}

TEST_CASE("readLineTypeAirfoil: generated vertex names stay unique across indexed files",
          "[io][airfoil][naming]") {
  config.file_directory = airfoilDataDir();

  PModel model;

  std::vector<std::string> baseline_names;
  const nlohmann::json &index = airfoilIndex();
  for (std::size_t i = 0; i < index.size(); ++i) {
    const std::string file = index[i].at("file").get<std::string>();
    const std::string format = index[i].at("format").get<std::string>();
    const std::string line_name = "af_" + sanitizeName(fileStem(file));
    baseline_names.push_back(line_name);

    Baseline *line = new Baseline(line_name, "airfoil");
    readAirfoilLineXml(
      file, format, headerRowsForFormat(format), model, *line
    );
  }

  for (std::size_t i = 0; i < baseline_names.size(); ++i) {
    REQUIRE(model.getPointByName(baseline_names[i] + "_le") != nullptr);
    REQUIRE(model.getPointByName(baseline_names[i] + "_te") != nullptr);
  }
  CHECK(model.getPointByName("ple") == nullptr);
  CHECK(model.getPointByName("pte") == nullptr);
}

TEST_CASE("readLineTypeAirfoil: flip axis x3 mirrors thickness coordinates",
          "[io][airfoil][flip]") {
  config.file_directory = airfoilDataDir();

  PModel model_ref;
  Baseline line_ref("af_mh104_ref", "airfoil");
  readAirfoilLineXml("mh104.dat", "selig", 1, model_ref, line_ref);

  PModel model_flip;
  Baseline line_flip("af_mh104_flip", "airfoil");
  readAirfoilLineXml(
    "mh104.dat", "selig", 1, model_flip, line_flip,
    "<flip axis=\"3\" loc=\"0\">1</flip>"
  );

  REQUIRE(line_ref.vertices().size() == line_flip.vertices().size());
  for (std::size_t i = 0; i + 1 < line_ref.vertices().size(); ++i) {
    CHECK(line_flip.vertices()[i]->y() == Catch::Approx(line_ref.vertices()[i]->y()));
    CHECK(line_flip.vertices()[i]->z() == Catch::Approx(-line_ref.vertices()[i]->z()));
  }
}

TEST_CASE("readLineTypeAirfoil: Lednicer rejects a single coordinate block",
          "[io][airfoil][lednicer][error]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_bad_led", "airfoil");

  CHECK_THROWS_WITH(
    readAirfoilLineXml("mh104.dat", "lednicer", 1, model, line),
    Catch::Matchers::ContainsSubstring("exactly two coordinate blocks")
  );
}