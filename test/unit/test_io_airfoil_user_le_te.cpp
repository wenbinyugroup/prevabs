#include "catch_amalgamated.hpp"

#include "PBaseLine.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
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

double ua79sffScale() {
  return 1.0 - 0.65;
}

PDCELVertex *requirePoint(PModel &model, const std::string &name) {
  PDCELVertex *point = model.getPointByName(name);
  REQUIRE(point != nullptr);
  return point;
}

std::size_t countVertexNameOnLine(Baseline &line, const std::string &name) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < line.vertices().size(); ++i) {
    if (line.vertices()[i]->name() == name) {
      ++count;
    }
  }
  return count;
}

struct WarningCaptureSink : public spdlog::sinks::base_sink<std::mutex> {
  std::vector<std::string> warnings;
protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    if (msg.level == spdlog::level::warn) {
      warnings.push_back(
        std::string(msg.payload.data(), msg.payload.size())
      );
    }
  }
  void flush_() override {}
};

struct ScopedPrevabsLogger {
  explicit ScopedPrevabsLogger(const std::shared_ptr<spdlog::logger> &logger) {
    spdlog::drop("prevabs");
    spdlog::register_logger(logger);
  }

  ~ScopedPrevabsLogger() {
    spdlog::drop("prevabs");
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
    logger->set_level(spdlog::level::warn);
    spdlog::register_logger(logger);
  }
};

}  // namespace

TEST_CASE("readLineTypeAirfoil: default ua79sff path keeps default edge names",
          "[io][airfoil][user-edge][default]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_default", "airfoil");
  readAirfoilLineXml("ua79sff.dat", "selig", 1, model, line);

  REQUIRE(model.getPointByName("af_ua79_default_le") != nullptr);
  REQUIRE(model.getPointByName("af_ua79_default_te") != nullptr);
  CHECK(model.getPointByName("ple") == nullptr);
  CHECK(model.getPointByName("pte") == nullptr);
}

TEST_CASE("readLineTypeAirfoil: exact-hit leading edge can be renamed",
          "[io][airfoil][user-edge][rename]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_hit", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<leading_edge name=\"ple\">0.650000 -0.018400</leading_edge>"
  );

  PDCELVertex *ple = requirePoint(model, "ple");
  CHECK(ple->y() == Catch::Approx(0.65));
  CHECK(ple->z() == Catch::Approx(-0.0184));
  CHECK(model.getPointByName("af_ua79_hit_le") == nullptr);
  CHECK(countVertexNameOnLine(line, "ple") == 1);
}

TEST_CASE("readLineTypeAirfoil: user leading edge inserts a new vertex when needed",
          "[io][airfoil][user-edge][insert]") {
  config.file_directory = airfoilDataDir();

  PModel model_ref;
  Baseline line_ref("af_ua79_ref", "airfoil");
  readAirfoilLineXml("ua79sff.dat", "selig", 1, model_ref, line_ref);

  PModel model;
  Baseline line("af_ua79_insert", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<leading_edge name=\"ple_insert\">0.652000 -0.020000</leading_edge>"
  );

  PDCELVertex *ple = requirePoint(model, "ple_insert");
  CHECK(ple->y() == Catch::Approx(0.652));
  CHECK(ple->z() == Catch::Approx(-0.02));
  CHECK(line.vertices().size() == line_ref.vertices().size() + 1);
  CHECK(countVertexNameOnLine(line, "ple_insert") == 1);
}

TEST_CASE("readLineTypeAirfoil: edge node with only name keeps auto-detected geometry",
          "[io][airfoil][user-edge][name-only]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_name_only", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<leading_edge name=\"ple_only\"/>"
  );

  PDCELVertex *ple = requirePoint(model, "ple_only");
  CHECK(ple->y() == Catch::Approx(0.65));
  CHECK(ple->z() == Catch::Approx(-0.0184));
  CHECK(model.getPointByName("af_ua79_name_only_le") == nullptr);
}

TEST_CASE("readLineTypeAirfoil: edge node with only coordinates keeps default name",
          "[io][airfoil][user-edge][coord-only]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_coord_only", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<leading_edge>0.650000 -0.018400</leading_edge>"
  );

  PDCELVertex *ple = requirePoint(model, "af_ua79_coord_only_le");
  CHECK(ple->y() == Catch::Approx(0.65));
  CHECK(ple->z() == Catch::Approx(-0.0184));
  CHECK(model.getPointByName("ple") == nullptr);
}

TEST_CASE("readLineTypeAirfoil: normalize rescales ua79sff x-range to [0, 1]",
          "[io][airfoil][user-edge][normalize]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_norm", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<normalize>1</normalize>"
  );

  double min_x = line.vertices().front()->y();
  double max_x = line.vertices().front()->y();
  for (std::size_t i = 1; i < line.vertices().size(); ++i) {
    min_x = (std::min)(min_x, line.vertices()[i]->y());
    max_x = (std::max)(max_x, line.vertices()[i]->y());
  }

  PDCELVertex *ple = requirePoint(model, "af_ua79_norm_le");
  CHECK(min_x == Catch::Approx(0.0));
  CHECK(max_x == Catch::Approx(1.0));
  CHECK(ple->y() == Catch::Approx(0.0));
  CHECK(ple->z() == Catch::Approx(-0.0184 / ua79sffScale()));
}

TEST_CASE("readLineTypeAirfoil: normalize also transforms user-specified edge coordinates",
          "[io][airfoil][user-edge][normalize][coord]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_norm_coord", "airfoil");
  readAirfoilLineXml(
    "ua79sff.dat", "selig", 1, model, line,
    "<normalize>1</normalize>"
    "<leading_edge name=\"ple_norm\">0.650000 -0.018400</leading_edge>"
  );

  PDCELVertex *ple = requirePoint(model, "ple_norm");
  CHECK(ple->y() == Catch::Approx(0.0));
  CHECK(ple->z() == Catch::Approx(-0.0184 / ua79sffScale()));
  CHECK(model.getPointByName("af_ua79_norm_coord_le") == nullptr);
}

TEST_CASE("readLineTypeAirfoil: inconsistent user-specified edge emits a warning",
          "[io][airfoil][user-edge][warning]") {
  config.file_directory = airfoilDataDir();

  auto sink = std::make_shared<WarningCaptureSink>();
  auto logger = std::make_shared<spdlog::logger>("prevabs", sink);
  logger->set_level(spdlog::level::warn);
  ScopedPrevabsLogger scoped_logger(logger);

  PModel model;
  Baseline line("af_warn", "airfoil");
  readAirfoilLineXml(
    "warning_zigzag.dat", "selig", 1, model, line,
    "<leading_edge>0.550000 0.140000</leading_edge>"
  );

  bool found_warning = false;
  for (std::size_t i = 0; i < sink->warnings.size(); ++i) {
    if (sink->warnings[i].find("user-specified LE") != std::string::npos
        && sink->warnings[i].find("geometrically inconsistent")
             != std::string::npos) {
      found_warning = true;
      break;
    }
  }
  CHECK(found_warning);
}

TEST_CASE("readLineTypeAirfoil: conflicting user edge name throws",
          "[io][airfoil][user-edge][name-conflict]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  model.addVertex(new PDCELVertex("ple", 0.0, 9.0, 9.0));

  Baseline line("af_ua79_conflict", "airfoil");
  CHECK_THROWS_WITH(
    readAirfoilLineXml(
      "ua79sff.dat", "selig", 1, model, line,
      "<leading_edge name=\"ple\">0.650000 -0.018400</leading_edge>"
    ),
    Catch::Matchers::ContainsSubstring("ple")
      && Catch::Matchers::ContainsSubstring("af_ua79_conflict")
  );
}

TEST_CASE("readLineTypeAirfoil: malformed edge coordinate payload throws",
          "[io][airfoil][user-edge][format][error]") {
  config.file_directory = airfoilDataDir();

  PModel model;
  Baseline line("af_ua79_bad_coord", "airfoil");

  CHECK_THROWS_WITH(
    readAirfoilLineXml(
      "ua79sff.dat", "selig", 1, model, line,
      "<leading_edge>1.0</leading_edge>"
    ),
    Catch::Matchers::ContainsSubstring("expected exactly two numbers")
  );
}
