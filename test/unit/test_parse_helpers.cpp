#include "catch_amalgamated.hpp"

#include "PBaseLine.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// parseRequiredInt
// ---------------------------------------------------------------------------

TEST_CASE("parseRequiredInt: empty string throws 'Missing'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredInt("", "<test>"),
    Catch::Matchers::ContainsSubstring("Missing")
      && Catch::Matchers::ContainsSubstring("<test>")
  );
}

TEST_CASE("parseRequiredInt: whitespace-only throws 'Missing'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredInt("   ", "<test>"),
    Catch::Matchers::ContainsSubstring("Missing")
  );
}

TEST_CASE("parseRequiredInt: non-numeric string throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredInt("abc", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
      && Catch::Matchers::ContainsSubstring("abc")
  );
}

TEST_CASE("parseRequiredInt: trailing garbage throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredInt("3abc", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
      && Catch::Matchers::ContainsSubstring("3abc")
  );
}

TEST_CASE("parseRequiredInt: valid integer parses correctly",
          "[parse][hygiene]") {
  CHECK(parseRequiredInt("42", "<test>") == 42);
  CHECK(parseRequiredInt("-7", "<test>") == -7);
  CHECK(parseRequiredInt("  10  ", "<test>") == 10);
}

// ---------------------------------------------------------------------------
// parseRequiredDouble
// ---------------------------------------------------------------------------

TEST_CASE("parseRequiredDouble: empty string throws 'Missing'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredDouble("", "<test>"),
    Catch::Matchers::ContainsSubstring("Missing")
      && Catch::Matchers::ContainsSubstring("<test>")
  );
}

TEST_CASE("parseRequiredDouble: non-numeric string throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredDouble("abc", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
      && Catch::Matchers::ContainsSubstring("abc")
  );
}

TEST_CASE("parseRequiredDouble: trailing garbage throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredDouble("3.14abc", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
      && Catch::Matchers::ContainsSubstring("3.14abc")
  );
}

TEST_CASE("parseRequiredDouble: 'nan' throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredDouble("nan", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
  );
}

TEST_CASE("parseRequiredDouble: 'inf' throws 'Invalid'",
          "[parse][error][hygiene]") {
  CHECK_THROWS_WITH(
    parseRequiredDouble("inf", "<test>"),
    Catch::Matchers::ContainsSubstring("Invalid")
  );
}

TEST_CASE("parseRequiredDouble: valid double parses correctly",
          "[parse][hygiene]") {
  CHECK(parseRequiredDouble("3.14", "<test>") == Catch::Approx(3.14));
  CHECK(parseRequiredDouble("-2.5e3", "<test>") == Catch::Approx(-2500.0));
  CHECK(parseRequiredDouble("  1.0  ", "<test>") == Catch::Approx(1.0));
}

// ---------------------------------------------------------------------------
// Field-level positivity checks via exposed IO functions
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

void readCircleXml(const std::string &xml, Baseline &line, PModel &model) {
  Message message;
  g_msg = &message;

  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  readLineTypeCircle(&line, line_node, geo, &model);
}

void readArcXml(const std::string &xml, Baseline &line, PModel &model) {
  Message message;
  g_msg = &message;

  const std::string wrapped = "<geo>" + xml + "</geo>";
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(wrapped, buffer, doc);

  rapidxml::xml_node<> *geo = doc.first_node("geo");
  rapidxml::xml_node<> *line_node = geo->first_node();
  readLineTypeArc(&line, line_node, geo, &model);
}

} // namespace

TEST_CASE("readLineTypeCircle: discrete number zero throws",
          "[io][error][baseline][hygiene]") {
  PModel model;
  model.addVertex(new PDCELVertex("c1", 0, 0.0, 0.0));
  Baseline line("circ_bad_n", "circle");
  CHECK_THROWS_WITH(
    readCircleXml(
      "<baseline name=\"circ_bad_n\" type=\"circle\">"
        "<center>c1</center>"
        "<radius>1.0</radius>"
        "<discrete by=\"number\">0</discrete>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("positive")
      && Catch::Matchers::ContainsSubstring("circ_bad_n")
  );
}

TEST_CASE("readLineTypeCircle: discrete number non-numeric throws",
          "[io][error][baseline][hygiene]") {
  PModel model;
  model.addVertex(new PDCELVertex("c1", 0, 0.0, 0.0));
  Baseline line("circ_bad_str", "circle");
  CHECK_THROWS_WITH(
    readCircleXml(
      "<baseline name=\"circ_bad_str\" type=\"circle\">"
        "<center>c1</center>"
        "<radius>1.0</radius>"
        "<discrete by=\"number\">abc</discrete>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("Invalid")
      && Catch::Matchers::ContainsSubstring("abc")
  );
}

TEST_CASE("readLineTypeArc: discrete angle zero throws",
          "[io][error][baseline][hygiene]") {
  PModel model;
  model.addVertex(new PDCELVertex("s1", 0, 0.0, 0.0));
  model.addVertex(new PDCELVertex("e1", 0, 1.0, 0.0));
  model.addVertex(new PDCELVertex("r1", 0, 0.5, 0.5));
  Baseline line("arc_bad_da", "arc");
  CHECK_THROWS_WITH(
    readArcXml(
      "<baseline name=\"arc_bad_da\" type=\"arc\">"
        "<start>s1</start>"
        "<end>e1</end>"
        "<radius>1.0</radius>"
        "<discrete by=\"angle\">0</discrete>"
      "</baseline>",
      line, model
    ),
    Catch::Matchers::ContainsSubstring("positive")
      && Catch::Matchers::ContainsSubstring("arc_bad_da")
  );
}
