#include "catch_amalgamated.hpp"

#include "PModel.hpp"
#include "PModelIO.hpp"
#include "globalVariables.hpp"
#include "utilities.hpp"

#include <string>
#include <vector>
#include <stdexcept>

namespace {

void parseXmlDocument(
  const std::string &xml,
  std::vector<char> &buffer,
  rapidxml::xml_document<> &doc
);

int parseFailureCriterionFromMaterialXml(const std::string &xml) {
  Message message;
  g_msg = &message;

  std::vector<char> buffer(xml.begin(), xml.end());
  buffer.push_back('\0');

  rapidxml::xml_document<> doc;
  doc.parse<0>(buffer.data());

  PModel model;
  Material *material = readXMLElementMaterial(doc.first_node("material"),
                                              nullptr, &model);
  return material->getFailureCriterion();
}

Material *parseMaterialFromXml(
  const std::string &xml,
  PModel &model,
  std::vector<char> &buffer,
  rapidxml::xml_document<> &doc
) {
  Message message;
  g_msg = &message;

  parseXmlDocument(xml, buffer, doc);
  return readXMLElementMaterial(doc.first_node("material"), nullptr, &model);
}

std::string materialXml(
  const std::string &name,
  const std::string &type,
  const std::string &criterion
) {
  return "<material name=\"" + name + "\" type=\"" + type + "\">"
         "<failure_criterion>" + criterion + "</failure_criterion>"
         "</material>";
}

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

TEST_CASE("readXMLElementMaterial: failure criterion aliases follow material type",
          "[io][material][failure]") {
  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("iso_tresca", "isotropic", "tresca")) == 3);
  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("iso_shear_strain", "isotropic", "max shear strain")) == 4);
  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("iso_mises", "isotropic", "mises")) == 5);

  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("ortho_tsai_hill", "orthotropic", "tsai-hill")) == 3);
  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("ti_tsai_wu", "transversely isotropic", "tsai-wu")) == 4);
  CHECK(parseFailureCriterionFromMaterialXml(
          materialXml("aniso_hashin", "anisotropic", "hashin")) == 5);
}

TEST_CASE("readXMLElementMaterial: cross-type failure criterion aliases are rejected",
          "[io][material][failure]") {
  CHECK_THROWS_WITH(
    parseFailureCriterionFromMaterialXml(
      materialXml("iso_bad", "isotropic", "tsai-hill")
    ),
    Catch::Matchers::ContainsSubstring("Unsupported failure criterion")
      && Catch::Matchers::ContainsSubstring("isotropic")
  );

  CHECK_THROWS_WITH(
    parseFailureCriterionFromMaterialXml(
      materialXml("ortho_bad", "orthotropic", "tresca")
    ),
    Catch::Matchers::ContainsSubstring("Unsupported failure criterion")
      && Catch::Matchers::ContainsSubstring("orthotropic")
  );
}

TEST_CASE("readXMLElementMaterial: lamina symmetry keyword is rejected",
          "[io][material][type]") {
  CHECK_THROWS_WITH(
    parseFailureCriterionFromMaterialXml(
      materialXml("legacy_lamina", "lamina", "tsai-wu")
    ),
    Catch::Matchers::ContainsSubstring("type='lamina' is no longer supported")
      && Catch::Matchers::ContainsSubstring("transversely isotropic")
  );
}

TEST_CASE("readXMLElementMaterial: transversely isotropic maps to orthotropic internally",
          "[io][material][type]") {
  PModel model;
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;

  Material *material = parseMaterialFromXml(
    "<material name=\"ti\" type=\"transversely isotropic\">"
      "<elastic>"
        "<e1>10</e1>"
        "<e2>3</e2>"
        "<nu12>0.25</nu12>"
        "<g12>2</g12>"
      "</elastic>"
      "<failure_criterion>tsai-wu</failure_criterion>"
      "<strength>"
        "<xt>100</xt>"
        "<yt>20</yt>"
        "<xc>80</xc>"
        "<s>10</s>"
      "</strength>"
    "</material>",
    model, buffer, doc
  );

  REQUIRE(material != nullptr);
  CHECK(material->getSymmetryType() == "transversely isotropic");
  CHECK(material->getType() == "orthotropic");

  std::vector<double> elastic = material->getElastic();
  REQUIRE(elastic.size() == 9);
  CHECK(elastic[0] == Catch::Approx(10.0));
  CHECK(elastic[1] == Catch::Approx(3.0));
  CHECK(elastic[2] == Catch::Approx(3.0));
  CHECK(elastic[3] == Catch::Approx(2.0));
  CHECK(elastic[4] == Catch::Approx(2.0));
  CHECK(elastic[6] == Catch::Approx(0.25));
  CHECK(elastic[7] == Catch::Approx(0.25));
  CHECK(elastic[8] == Catch::Approx(0.3));

  Strength strength = material->getStrength();
  CHECK(strength._type == "transversely isotropic");
}

TEST_CASE("readXMLElementMaterial: reused material gets a default layer type",
          "[io][material][layertype]") {
  Message message;
  g_msg = &message;

  PModel model;
  Material *material = new Material("mat_reuse");
  model.addMaterial(material);
  REQUIRE(model.getLayerTypeByMaterialAngle(material, 0.0) == nullptr);

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(
    "<material name=\"mat_reuse\" type=\"isotropic\"></material>",
    buffer, doc
  );

  Material *parsed = readXMLElementMaterial(doc.first_node("material"),
                                            nullptr, &model);
  REQUIRE(parsed == material);
  CHECK(model.getLayerTypeByMaterialAngle(material, 0.0) != nullptr);
}

TEST_CASE("readLayups: layup reading repairs missing layer types on reused materials",
          "[io][material][layertype][layup]") {
  Message message;
  g_msg = &message;

  PModel model;
  Material *material = new Material("mat");
  material->setType("orthotropic");
  model.addMaterial(material);

  Lamina *lamina = new Lamina("lam", material, 0.25);
  model.addLamina(lamina);

  REQUIRE(model.getLayerTypeByMaterialAngle(material, 0.0) == nullptr);

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(
    "<layups>"
      "<layup name=\"lay\">"
        "<layer lamina=\"lam\">0</layer>"
      "</layup>"
    "</layups>",
    buffer, doc
  );

  REQUIRE(readLayups(doc.first_node("layups"), &model) == 0);

  Layup *layup = model.getLayupByName("lay");
  REQUIRE(layup != nullptr);

  std::vector<Layer> layers = layup->getLayers();
  REQUIRE(layers.size() == 1);
  REQUIRE(layers[0].getLayerType() != nullptr);
  CHECK(layers[0].getLayerType()->material() == material);
  CHECK(model.getLayerTypeByMaterialAngle(material, 0.0)
        == layers[0].getLayerType());
}

TEST_CASE("readLayups: lamina without material fails before layer creation",
          "[io][material][layertype][layup]") {
  Message message;
  g_msg = &message;

  PModel model;
  Lamina *lamina = new Lamina("lam", nullptr, 0.25);
  model.addLamina(lamina);

  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(
    "<layups>"
      "<layup name=\"lay\">"
        "<layer lamina=\"lam\">0</layer>"
      "</layup>"
    "</layups>",
    buffer, doc
  );

  CHECK_THROWS_WITH(
    readLayups(doc.first_node("layups"), &model),
    Catch::Matchers::ContainsSubstring("lamina 'lam'")
      && Catch::Matchers::ContainsSubstring("has no material")
  );
}

TEST_CASE("readMaterials: null materials node is rejected",
          "[io][material][hygiene]") {
  Message message;
  g_msg = &message;

  PModel model;
  CHECK_THROWS_WITH(
    readMaterials(nullptr, &model),
    Catch::Matchers::ContainsSubstring(
      "Missing required XML element <materials>"
    )
  );
}

TEST_CASE("readLayups: layer without lamina or layup attribute fails clearly",
          "[io][material][layup][hygiene]") {
  Message message;
  g_msg = &message;

  PModel model;
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(
    "<layups>"
      "<layup name=\"lay\">"
        "<layer angle=\"0\">0</layer>"
      "</layup>"
    "</layups>",
    buffer, doc
  );

  CHECK_THROWS_WITH(
    readLayups(doc.first_node("layups"), &model),
    Catch::Matchers::ContainsSubstring(
      "Missing required XML attribute 'lamina' or 'layup'"
    )
      && Catch::Matchers::ContainsSubstring("<layer> in <layup name='lay'>")
  );
}

TEST_CASE("readLayups: layer cannot define lamina and layup together",
          "[io][material][layup][hygiene]") {
  Message message;
  g_msg = &message;

  PModel model;
  std::vector<char> buffer;
  rapidxml::xml_document<> doc;
  parseXmlDocument(
    "<layups>"
      "<layup name=\"lay\">"
        "<layer lamina=\"lam\" layup=\"sub\">0</layer>"
      "</layup>"
    "</layups>",
    buffer, doc
  );

  CHECK_THROWS_WITH(
    readLayups(doc.first_node("layups"), &model),
    Catch::Matchers::ContainsSubstring(
      "cannot define both 'lamina' and 'layup'"
    )
      && Catch::Matchers::ContainsSubstring("<layer> in <layup name='lay'>")
  );
}

// ==================================================================
// readMaterialsFile: required flag distinguishes explicit vs. auto path
// ==================================================================

TEST_CASE("readMaterialsFile: missing auto-detected path is only a warning",
          "[io][material][error]") {
  PModel model;
  // required=false (default): missing file returns 1 without throwing.
  int rc = readMaterialsFile("/no/such/path/MaterialDB.xml", &model);
  CHECK(rc == 1);
}

TEST_CASE("readMaterialsFile: missing explicitly-provided path throws",
          "[io][material][error]") {
  using Catch::Matchers::ContainsSubstring;
  PModel model;
  // required=true: missing file must throw with the path in the message.
  CHECK_THROWS_WITH(
    readMaterialsFile("/no/such/path/UserMaterials.xml", &model, true),
    ContainsSubstring("cannot open material file")
      && ContainsSubstring("UserMaterials.xml")
  );
}
