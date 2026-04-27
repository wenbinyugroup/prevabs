#include "catch_amalgamated.hpp"

#include "PBaseLine.hpp"
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

// Thin wrapper: parse xml, return the Baseline* from readXMLElementLine.
// p_xn_geo is set to the first_node of a wrapper <geo> element so that
// findPointByName has a sibling context to search.
Baseline *readLineXml(const std::string &xml, PModel &model) {

  // Wrap in <geo> so the geo-context pointer is valid
  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  return readXMLElementLine(line_node, geo, &model);
}

// Call readLineTypeStraight directly.
void readStraightXml(const std::string &xml, Baseline &line, PModel &model) {

  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  readLineTypeStraight(&line, line_node, geo, &model);
}

// Call readLineTypeCircle directly.
void readCircleXml(const std::string &xml, Baseline &line, PModel &model) {

  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  readLineTypeCircle(&line, line_node, geo, &model);
}

// Call readLineTypeArc directly.
void readArcXml(const std::string &xml, Baseline &line, PModel &model) {

  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  readLineTypeArc(&line, line_node, geo, &model);
}

} // namespace

// ---------------------------------------------------------------------------
// readXMLElementLine: unknown type
// ---------------------------------------------------------------------------

TEST_CASE("readXMLElementLine: unknown baseline type throws",
          "[io][error][baseline]") {
  PModel model;
  CHECK_THROWS_WITH(
    readLineXml(
      "<baseline name=\"bl\" type=\"spline\"></baseline>",
      model
    ),
    Catch::Matchers::ContainsSubstring("bl")
      && Catch::Matchers::ContainsSubstring("spline")
  );
}

// ---------------------------------------------------------------------------
// readLineTypeStraight: malformed point expression
// ---------------------------------------------------------------------------

TEST_CASE("readLineTypeStraight: too many ':' separators throws",
          "[io][error][baseline]") {
  PModel model;
  // Add two real points so the parser can reach the expression check
  model.addVertex(new PDCELVertex("p1", 0, 0, 0));
  model.addVertex(new PDCELVertex("p2", 0, 1, 0));
  model.addVertex(new PDCELVertex("p3", 0, 2, 0));

  Baseline line("bl_bad", "straight");
  CHECK_THROWS_WITH(
    readStraightXml(
      "<baseline name=\"bl_bad\" type=\"straight\">"
        "<points>p1:p2:p3</points>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("p1:p2:p3")
      && Catch::Matchers::ContainsSubstring("bl_bad")
  );
}

// ---------------------------------------------------------------------------
// readLineTypeStraight: no definition at all
// ---------------------------------------------------------------------------

TEST_CASE("readLineTypeStraight: no <points> and no <point>+<angle> throws",
          "[io][error][baseline]") {
  PModel model;
  Baseline line("bl_empty", "straight");
  CHECK_THROWS_WITH(
    readStraightXml(
      "<baseline name=\"bl_empty\" type=\"straight\"></baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("bl_empty")
      && Catch::Matchers::ContainsSubstring("straight")
  );
}

// ---------------------------------------------------------------------------
// readLineTypeCircle: no matching case
// ---------------------------------------------------------------------------

TEST_CASE("readLineTypeCircle: no <center>+<radius> or <center>+<point> throws",
          "[io][error][baseline]") {
  PModel model;
  Baseline line("circ_bad", "circle");
  // Only <discrete> node present — no center, radius, or point
  CHECK_THROWS_WITH(
    readCircleXml(
      "<baseline name=\"circ_bad\" type=\"circle\">"
        "<discrete by=\"number\">36</discrete>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("circ_bad")
      && Catch::Matchers::ContainsSubstring("circle")
  );
}

// ---------------------------------------------------------------------------
// readLineTypeArc: no matching case
// ---------------------------------------------------------------------------

TEST_CASE("readLineTypeArc: unrecognized configuration throws",
          "[io][error][baseline]") {
  PModel model;
  // Only <start> given — no center, no end, no radius → none of the 4 cases
  model.addVertex(new PDCELVertex("s1", 0, 0, 0));
  Baseline line("arc_bad", "arc");
  CHECK_THROWS_WITH(
    readArcXml(
      "<baseline name=\"arc_bad\" type=\"arc\">"
        "<start>s1</start>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("arc_bad")
      && Catch::Matchers::ContainsSubstring("arc")
  );
}