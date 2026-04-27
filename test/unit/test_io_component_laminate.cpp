#include "catch_amalgamated.hpp"

#include "CrossSection.hpp"
#include "Material.hpp"
#include "PBaseLine.hpp"
#include "PComponent.hpp"
#include "PDCELVertex.hpp"
#include "PModel.hpp"
#include "PModelIO.hpp"
#include "PSegment.hpp"
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

Baseline *makeStraightBaseline(PModel &model, const std::string &name) {
  Baseline *baseline = new Baseline(name, "straight");
  baseline->addPVertex(new PDCELVertex(0.0, 0.0, 0.0));
  baseline->addPVertex(new PDCELVertex(0.0, 1.0, 0.0));
  model.addBaseline(baseline);
  return baseline;
}

Layup *makeSingleLayerLayup(PModel &model, const std::string &name) {
  Material *material = new Material(name + "_mat");
  material->setType("orthotropic");
  model.addMaterial(material);

  LayerType *layertype = new LayerType(0, material, 0.0);
  model.addLayerType(layertype);

  Lamina *lamina = new Lamina(name + "_lamina", material, 0.25);
  model.addLamina(lamina);

  Layup *layup = new Layup(name);
  layup->addLayer(Layer(lamina, 0.0, 1, layertype));
  model.addLayup(layup);

  return layup;
}

int readLaminateComponentXml(
  const std::string &xml,
  PComponent &component,
  PModel &model,
  CrossSection &cross_section
) {

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  std::vector<std::vector<std::string>> dependents_all;
  std::vector<std::string> depend_names;
  std::vector<Layup *> layups;
  int num_combined_layups = 0;

  return readXMLElementComponentLaminate(
    &component,
    doc.first_node("component"),
    dependents_all,
    depend_names,
    layups,
    num_combined_layups,
    &cross_section,
    &model
  );
}

// Variant that passes a non-empty depend_names to reach the split_by code path.
int readLaminateComponentXmlDependent(
  const std::string &xml,
  PComponent &component,
  PModel &model,
  CrossSection &cross_section,
  const std::vector<std::string> &depend_names_in
) {

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  std::vector<std::vector<std::string>> dependents_all;
  std::vector<std::string> depend_names{depend_names_in};
  std::vector<Layup *> layups;
  int num_combined_layups = 0;

  return readXMLElementComponentLaminate(
    &component,
    doc.first_node("component"),
    dependents_all,
    depend_names,
    layups,
    num_combined_layups,
    &cross_section,
    &model
  );
}

std::vector<std::string> getSegmentNames(PComponent &component) {
  std::vector<std::string> names;
  const std::vector<Segment *> segments = component.segments();
  for (Segment *segment : segments) {
    names.push_back(segment->getName());
  }
  return names;
}

}  // namespace

TEST_CASE("readXMLElementComponentLaminate: empty <segments> is skipped safely",
          "[io][component][laminate]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");

  PComponent component;
  CrossSection cross_section("cs");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segments>"
        "<baseline>bsl</baseline>"
      "</segments>"
    "</component>",
    component,
    model,
    cross_section
  ) == 0);

  CHECK(component.segments().empty());
}

TEST_CASE("readXMLElementComponentLaminate: zero-length layup range is skipped safely",
          "[io][component][laminate]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");
  makeSingleLayerLayup(model, "lay");

  PComponent component;
  CrossSection cross_section("cs");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segments>"
        "<baseline>bsl</baseline>"
        "<layup begin=\"0.5\" end=\"0.5\">lay</layup>"
      "</segments>"
    "</component>",
    component,
    model,
    cross_section
  ) == 0);

  CHECK(component.segments().empty());
}

TEST_CASE("readXMLElementComponentLaminate: single layup still creates one segment",
          "[io][component][laminate]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");
  Layup *layup = makeSingleLayerLayup(model, "lay");

  PComponent component;
  CrossSection cross_section("cs");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segments>"
        "<baseline>bsl</baseline>"
        "<layup>lay</layup>"
      "</segments>"
    "</component>",
    component,
    model,
    cross_section
  ) == 0);

  std::vector<Segment *> segments = component.segments();
  REQUIRE(segments.size() == 1);
  CHECK(segments.front()->getLayup() == layup);
}

TEST_CASE("readXMLElementComponentLaminate: auto names are stable across parsing paths",
          "[io][component][laminate][naming]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model_a;
  makeStraightBaseline(model_a, "bsl_single");
  makeStraightBaseline(model_a, "bsl_multi");
  makeSingleLayerLayup(model_a, "lay");

  PComponent component_a;
  CrossSection cross_section_a("cs_a");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segment>"
        "<baseline>bsl_single</baseline>"
        "<layup>lay</layup>"
      "</segment>"
      "<segments>"
        "<baseline>bsl_multi</baseline>"
        "<layup>lay</layup>"
      "</segments>"
    "</component>",
    component_a,
    model_a,
    cross_section_a
  ) == 0);

  PModel model_b;
  makeStraightBaseline(model_b, "bsl_single");
  makeStraightBaseline(model_b, "bsl_multi");
  makeSingleLayerLayup(model_b, "lay");

  PComponent component_b;
  CrossSection cross_section_b("cs_b");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segments>"
        "<baseline>bsl_multi</baseline>"
        "<layup>lay</layup>"
      "</segments>"
      "<segment>"
        "<baseline>bsl_single</baseline>"
        "<layup>lay</layup>"
      "</segment>"
    "</component>",
    component_b,
    model_b,
    cross_section_b
  ) == 0);

  CHECK(getSegmentNames(component_a)
        == std::vector<std::string>{"sgm_1", "sgm_blk_1"});
  CHECK(getSegmentNames(component_b)
        == std::vector<std::string>{"sgm_1", "sgm_blk_1"});
}

TEST_CASE("readXMLElementComponentLaminate: <segments name> becomes stable prefix",
          "[io][component][laminate][naming]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");
  makeSingleLayerLayup(model, "lay1");
  makeSingleLayerLayup(model, "lay2");

  PComponent component;
  CrossSection cross_section("cs");

  REQUIRE(readLaminateComponentXml(
    "<component>"
      "<segments name=\"skin\">"
        "<baseline>bsl</baseline>"
        "<layup begin=\"0.0\" end=\"0.5\">lay1</layup>"
        "<layup begin=\"0.5\" end=\"1.0\">lay2</layup>"
      "</segments>"
    "</component>",
    component,
    model,
    cross_section
  ) == 0);

  CHECK(getSegmentNames(component)
        == std::vector<std::string>{"skin_1", "skin_2"});
}

// ---------------------------------------------------------------------------
// split_by error paths (reached only when a dependent component exists)
// ---------------------------------------------------------------------------

TEST_CASE("readXMLElementComponentLaminate: unknown <split by=...> throws",
          "[io][error][component][laminate]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");
  makeSingleLayerLayup(model, "lay1");

  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readLaminateComponentXmlDependent(
      "<component name=\"comp1\">"
        "<segment name=\"seg_a\">"
          "<baseline>bsl</baseline>"
          "<layup>lay1</layup>"
          "<split by=\"pixel\">0.5</split>"
        "</segment>"
      "</component>",
      component, model, cross_section,
      {"comp0"}
    ),
    Catch::Matchers::ContainsSubstring("pixel")
      && Catch::Matchers::ContainsSubstring("seg_a")
  );
}

TEST_CASE("readXMLElementComponentLaminate: <split by=name> with missing point throws",
          "[io][error][component][laminate]") {
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;
  Segment::count_tmp = 0;

  PModel model;
  makeStraightBaseline(model, "bsl");
  makeSingleLayerLayup(model, "lay1");

  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readLaminateComponentXmlDependent(
      "<component name=\"comp1\">"
        "<segment name=\"seg_b\">"
          "<baseline>bsl</baseline>"
          "<layup>lay1</layup>"
          "<split by=\"name\">ghost_pt</split>"
        "</segment>"
      "</component>",
      component, model, cross_section,
      {"comp0"}
    ),
    Catch::Matchers::ContainsSubstring("ghost_pt")
      && Catch::Matchers::ContainsSubstring("seg_b")
  );
}

TEST_CASE("readXMLElementComponentLaminate: unknown <location> point throws",
          "[io][component][laminate][error]") {
  using Catch::Matchers::ContainsSubstring;

  PModel model;
  makeStraightBaseline(model, "bsl");
  makeSingleLayerLayup(model, "lay1");

  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readLaminateComponentXml(
      "<component name=\"c1\">"
        "<location>no_such_point</location>"
        "<segment name=\"s1\">"
          "<baseline>bsl</baseline>"
          "<layup>lay1</layup>"
        "</segment>"
      "</component>",
      component, model, cross_section
    ),
    ContainsSubstring("cannot find") && ContainsSubstring("no_such_point")
  );
}

// ==================================================================
// Filling component lookup-null checks (readXMLElementComponentFilling)
// ==================================================================

namespace {

Material *makeIsotropicMaterial(PModel &model, const std::string &name) {
  Material *m = new Material(name);
  m->setType("isotropic");
  model.addMaterial(m);
  LayerType *lt = new LayerType(0, m, 0.0);
  model.addLayerType(lt);
  return m;
}

int readFillingComponentXml(
  const std::string &xml,
  PComponent &component,
  PModel &model,
  CrossSection &cross_section
) {

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(xml, buffer, doc);

  return readXMLElementComponentFilling(
    &component,
    doc.first_node("component"),
    &cross_section,
    &model
  );
}

}  // namespace

TEST_CASE("readXMLElementComponentFilling: unknown material throws",
          "[io][component][fill][error]") {
  using Catch::Matchers::ContainsSubstring;
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;

  PModel model;
  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readFillingComponentXml(
      "<component type=\"fill\"><material>ghost_mat</material></component>",
      component, model, cross_section
    ),
    ContainsSubstring("cannot find") && ContainsSubstring("ghost_mat")
  );
}

TEST_CASE("readXMLElementComponentFilling: unknown <location> point throws",
          "[io][component][fill][error]") {
  using Catch::Matchers::ContainsSubstring;
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;

  PModel model;
  makeIsotropicMaterial(model, "mat1");
  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readFillingComponentXml(
      "<component type=\"fill\">"
        "<material>mat1</material>"
        "<location>no_such_pt</location>"
      "</component>",
      component, model, cross_section
    ),
    ContainsSubstring("cannot find") && ContainsSubstring("no_such_pt")
  );
}

TEST_CASE("readXMLElementComponentFilling: unknown <baseline> throws",
          "[io][component][fill][error]") {
  using Catch::Matchers::ContainsSubstring;
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;

  PModel model;
  makeIsotropicMaterial(model, "mat1");
  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readFillingComponentXml(
      "<component type=\"fill\">"
        "<material>mat1</material>"
        "<baseline>ghost_bsl</baseline>"
      "</component>",
      component, model, cross_section
    ),
    ContainsSubstring("cannot find") && ContainsSubstring("ghost_bsl")
  );
}

TEST_CASE("readXMLElementComponentFilling: unknown <mesh_size at=...> point throws",
          "[io][component][fill][error]") {
  using Catch::Matchers::ContainsSubstring;
  CrossSection::used_material_index = 0;
  CrossSection::used_layertype_index = 0;

  PModel model;
  makeIsotropicMaterial(model, "mat1");
  PComponent component;
  CrossSection cross_section("cs");

  CHECK_THROWS_WITH(
    readFillingComponentXml(
      "<component type=\"fill\">"
        "<material>mat1</material>"
        "<mesh_size at=\"ghost_pt\">0.1</mesh_size>"
      "</component>",
      component, model, cross_section
    ),
    ContainsSubstring("cannot find") && ContainsSubstring("ghost_pt")
  );
}