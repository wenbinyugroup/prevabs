#include "catch_amalgamated.hpp"

#include "CrossSection.hpp"
#include "PComponent.hpp"
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

// Drive readXMLElementComponentLaminate with an in-memory XML snippet.
// Returns through the throw; the caller is expected to CHECK_THROWS_WITH.
void readLaminateXml(
  const std::string &xml,
  PComponent &component,
  PModel &model,
  CrossSection &cs
) {
  Message message;
  g_msg = &message;

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  std::vector<std::vector<std::string>> dependents_all;
  std::vector<std::string> depend_names;
  std::vector<Layup *> layups;
  int num_combined_layups = 0;

  readXMLElementComponentLaminate(
    &component,
    doc.first_node("component"),
    dependents_all,
    depend_names,
    layups,
    num_combined_layups,
    &cs,
    &model
  );
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Verify that a missing baseline reference throws a runtime_error whose
// message contains both the baseline name and the segment name.
// In the full pipeline, this error propagates through readInputMain and
// is rethrown by homogenize() with the "homogenization failed:" prefix,
// then caught and reported by the outer catch in main().
TEST_CASE("component laminate: missing baseline throws with baseline and segment name",
          "[io][error][pipeline]") {
  PModel model;
  PComponent component;
  CrossSection cs("test_cs");

  const std::string xml =
    "<component name=\"comp1\">"
      "<segment name=\"seg_a\">"
        "<baseline>ghost_baseline</baseline>"
        "<layup>any_layup</layup>"
      "</segment>"
    "</component>";

  CHECK_THROWS_WITH(
    readLaminateXml(xml, component, model, cs),
    Catch::Matchers::ContainsSubstring("ghost_baseline")
      && Catch::Matchers::ContainsSubstring("seg_a")
  );
}
