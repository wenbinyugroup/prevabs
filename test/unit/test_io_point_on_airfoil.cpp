#include "catch_amalgamated.hpp"

#include "PBaseLine.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"

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

std::string pointOnAirfoilDataDir() {
  return std::string(TEST_DATA_DIR) + "/../debug/point_on_line/";
}

int readPointOnAirfoilBaselines(PModel &model) {
  Message message;
  g_msg = &message;

  config.file_directory = pointOnAirfoilDataDir();

  const std::string xml =
    "<baselines>"
      "<line name=\"ln_af\" type=\"airfoil\">"
        "<points data=\"file\" format=\"1\" direction=\"-1\">"
          "CXTX_0400_110.dat"
        "</points>"
        "<reverse>1</reverse>"
      "</line>"
      "<point name=\"p6\" on=\"ln_af\" by=\"x2\" which=\"top\">0.030000</point>"
      "<point name=\"p7\" on=\"ln_af\" by=\"x2\" which=\"top\">0.300000</point>"
      "<point name=\"p8\" on=\"ln_af\" by=\"x2\" which=\"top\">0.980000</point>"
      "<line name=\"l_lp5\">"
        "<points>p8:p7</points>"
      "</line>"
      "<line name=\"l_lp6\">"
        "<points>p7:p6</points>"
      "</line>"
    "</baselines>";

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  return readBaselines(
    doc.first_node("baselines"), &model, pointOnAirfoilDataDir(),
    0.0, 0.0, 0.0, 1.0, 0.0
  );
}

}  // namespace

TEST_CASE("readBaselines: exact-hit points on an airfoil keep distinct link targets",
          "[io][airfoil][point-on-line]") {
  PModel model;
  REQUIRE(readPointOnAirfoilBaselines(model) == 0);

  PDCELVertex *p7 = model.getPointByName("p7");
  PDCELVertex *p8 = model.getPointByName("p8");

  REQUIRE(p7 != nullptr);
  REQUIRE(p8 != nullptr);
  REQUIRE(model.vertexData(p7).link_to != nullptr);
  REQUIRE(model.vertexData(p8).link_to != nullptr);

  CHECK(model.vertexData(p7).link_to != model.vertexData(p8).link_to);
  CHECK(model.vertexData(p7).link_to->name() != "_tmp");
  CHECK(model.vertexData(p8).link_to->name() != "_tmp");
}

TEST_CASE("readBaselines: airfoil sub-baselines do not collapse when endpoints hit existing vertices",
          "[io][airfoil][point-on-line][subline]") {
  PModel model;
  REQUIRE(readPointOnAirfoilBaselines(model) == 0);

  PDCELVertex *p6 = model.getPointByName("p6");
  PDCELVertex *p7 = model.getPointByName("p7");
  PDCELVertex *p8 = model.getPointByName("p8");
  Baseline *l_lp5 = model.getBaselineByName("l_lp5");
  Baseline *l_lp6 = model.getBaselineByName("l_lp6");

  REQUIRE(p6 != nullptr);
  REQUIRE(p7 != nullptr);
  REQUIRE(p8 != nullptr);
  REQUIRE(l_lp5 != nullptr);
  REQUIRE(l_lp6 != nullptr);
  REQUIRE(model.vertexData(p7).link_to != nullptr);
  REQUIRE(model.vertexData(p8).link_to != nullptr);

  REQUIRE(l_lp5->vertices().size() > 1);
  REQUIRE(l_lp6->vertices().size() > 1);

  CHECK(l_lp5->vertices().front() == model.vertexData(p8).link_to);
  CHECK(l_lp5->vertices().back() == model.vertexData(p7).link_to);
  CHECK(l_lp6->vertices().front() == model.vertexData(p7).link_to);
  CHECK(l_lp6->vertices().back() == p6);
}
