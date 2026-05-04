#include "catch_amalgamated.hpp"

#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

PDCELVertex *readPointXml(
  const std::string &point_xml,
  PModel &model
) {

  // Wrap in <geo> so p_xn_geo is a valid parent context
  const std::string wrapped = "<geo>" + point_xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *pnode = geo->first_node("point");
  return readXMLElementPoint(pnode, geo, &model);
}

} // namespace

// ---------------------------------------------------------------------------
// Tests: explicit-coordinate branch (no 'on' attribute)
// ---------------------------------------------------------------------------

TEST_CASE("readXMLElementPoint: unrecognized constraint throws",
          "[io][error][basepoint]") {
  PModel model;
  CHECK_THROWS_WITH(
    readPointXml(
      "<point name=\"pA\" constraint=\"fixed\">0.5 0.5</point>",
      model
    ),
    Catch::Matchers::ContainsSubstring("pA")
      && Catch::Matchers::ContainsSubstring("fixed")
  );
}

TEST_CASE("readXMLElementPoint: constraint=middle with missing first reference throws",
          "[io][error][basepoint]") {
  PModel model;
  // pRef1 does NOT exist; pRef2 could also be missing but the first check fires
  CHECK_THROWS_WITH(
    readPointXml(
      "<point name=\"pMid\" constraint=\"middle\">pRef1 pRef2</point>",
      model
    ),
    Catch::Matchers::ContainsSubstring("pMid")
      && Catch::Matchers::ContainsSubstring("pRef1")
  );
}

TEST_CASE("readXMLElementPoint: constraint=middle with missing second reference throws",
          "[io][error][basepoint]") {
  PModel model;
  // pRef1 exists, pRef2 does not
  model.addVertex(new PDCELVertex("pRef1", 0, 0.0, 0.0));
  CHECK_THROWS_WITH(
    readPointXml(
      "<point name=\"pMid\" constraint=\"middle\">pRef1 pRef2</point>",
      model
    ),
    Catch::Matchers::ContainsSubstring("pMid")
      && Catch::Matchers::ContainsSubstring("pRef2")
  );
}

// ---------------------------------------------------------------------------
// Tests: 'on' branch — unrecognized 'by' attribute
// ---------------------------------------------------------------------------

TEST_CASE("readXMLElementPoint: unrecognized 'by' attribute throws",
          "[io][error][basepoint]") {
  PModel model;

  // Build a minimal straight baseline for the 'on' reference
  Baseline *bl = new Baseline("bl_ref", "straight");
  bl->addPVertex(new PDCELVertex("v0", 0, 0.0, 0.0));
  bl->addPVertex(new PDCELVertex("v1", 0, 1.0, 0.0));
  model.addBaseline(bl);

  CHECK_THROWS_WITH(
    readPointXml(
      "<point name=\"pOn\" on=\"bl_ref\" by=\"diagonal\">0.5</point>",
      model
    ),
    Catch::Matchers::ContainsSubstring("pOn")
      && Catch::Matchers::ContainsSubstring("diagonal")
  );
}