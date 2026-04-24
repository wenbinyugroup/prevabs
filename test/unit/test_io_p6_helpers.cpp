#include "catch_amalgamated.hpp"

#include "CrossSection.hpp"
#include "PComponent.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"

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

}  // namespace

TEST_CASE("parseXmlBoolValue: accepts canonical true and false aliases",
          "[io][p6][bool]") {
  CHECK(parseXmlBoolValue("true", "test bool") == true);
  CHECK(parseXmlBoolValue(" YES ", "test bool") == true);
  CHECK(parseXmlBoolValue("0", "test bool") == false);
  CHECK(parseXmlBoolValue("Off", "test bool") == false);
}

TEST_CASE("parseXmlBoolValue: invalid value reports context",
          "[io][p6][bool]") {
  CHECK_THROWS_WITH(
    parseXmlBoolValue("maybe", "<component>@cycle"),
    Catch::Matchers::ContainsSubstring("Invalid boolean value")
      && Catch::Matchers::ContainsSubstring("<component>@cycle")
  );
}

TEST_CASE("parseFormatVersionValue: accepts legacy decimal integer strings",
          "[io][p6][format]") {
  CHECK(parseFormatVersionValue("0", "format attr") == 0);
  CHECK(parseFormatVersionValue("1.0", "format attr") == 1);
  CHECK(parseFormatVersionValue(" 2.000 ", "format attr") == 2);
}

TEST_CASE("parseFormatVersionValue: rejects non-integer versions",
          "[io][p6][format]") {
  CHECK_THROWS_WITH(
    parseFormatVersionValue("1.5", "root attribute 'format'"),
    Catch::Matchers::ContainsSubstring("integer value")
      && Catch::Matchers::ContainsSubstring("root attribute 'format'")
  );
}

TEST_CASE("readXMLElementComponent: invalid cycle boolean is rejected",
          "[io][p6][component]") {
  Message message;
  g_msg = &message;

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument("<component cycle=\"maybe\"/>", buffer, doc);

  std::vector<std::vector<std::string>> dependents_all;
  std::vector<Layup *> layups;
  int num_combined_layups = 0;
  CrossSection cross_section("cs");
  PModel model;

  CHECK_THROWS_WITH(
    readXMLElementComponent(
      doc.first_node("component"),
      dependents_all,
      layups,
      num_combined_layups,
      &cross_section,
      &model
    ),
    Catch::Matchers::ContainsSubstring("Invalid boolean value")
      && Catch::Matchers::ContainsSubstring("<component>@cycle/@cyclic")
  );
}
