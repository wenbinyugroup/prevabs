#include "catch_amalgamated.hpp"

#include "AppConfigIO.hpp"

#include <cstdio>
#include <fstream>

using prevabs::applyJson;
using prevabs::toJson;
using prevabs::tryLoadConfigFile;
using prevabs::ConfigFileStatus;
using nlohmann::json;

namespace {
// Write `content` to `path`; RAII-remove on scope exit.
struct TempFile {
  std::string path;
  explicit TempFile(const std::string &p, const std::string &content) : path(p) {
    std::ofstream f(path);
    f << content;
  }
  ~TempFile() { std::remove(path.c_str()); }
};
}  // namespace

TEST_CASE("AppConfig defaults are unchanged", "[config]") {
  AppConfig cfg;
  CHECK(cfg.tol == Catch::Approx(1e-12));
  CHECK(cfg.geo_tol == Catch::Approx(1e-9));
  CHECK(cfg.log_level == LOG_LEVEL_INFO);
  CHECK(cfg.gmsh_verbosity == 2);
  CHECK(cfg.tools.vabs == "VABS");
  CHECK(cfg.tools.swiftcomp == "SwiftComp");
  CHECK(cfg.tools.gmsh == "gmsh");
  CHECK(cfg.paths.material_db.empty());
  CHECK(cfg.gmsh.empty());
}

TEST_CASE("applyJson only overrides mentioned fields", "[config]") {
  AppConfig cfg;
  const json j = {
      {"tools", {{"vabs", "C:/tools/VABSIII.exe"}}},
      {"paths", {{"material_db", "C:/prevabs/materials"}}},
  };
  applyJson(cfg, j);

  // Mentioned fields updated ...
  CHECK(cfg.tools.vabs == "C:/tools/VABSIII.exe");
  CHECK(cfg.paths.material_db == "C:/prevabs/materials");
  // ... unmentioned fields keep their defaults.
  CHECK(cfg.tools.swiftcomp == "SwiftComp");
  CHECK(cfg.tools.gmsh == "gmsh");
  CHECK(cfg.gmsh.empty());
  CHECK(cfg.tol == Catch::Approx(1e-12));
}

TEST_CASE("applyJson reads all new sections", "[config]") {
  AppConfig cfg;
  const json j = {
      {"numerics", {{"tol", 1e-10}, {"geo_tol", 1e-8}}},
      {"output", {{"log_level", 1}, {"gmsh_verbosity", 3}}},
      {"tools", {{"vabs", "vabs4"}, {"swiftcomp", "sc"}, {"gmsh", "/opt/gmsh"}}},
      {"paths", {{"material_db", "/db"}}},
      {"gmsh", {
          {"general", {{"General.BackgroundGradient", 0}, {"Geometry.LineWidth", 1}}},
          {"homogenization", {{"Mesh.SurfaceFaces", 1}}}
      }},
  };
  applyJson(cfg, j);

  CHECK(cfg.tol == Catch::Approx(1e-10));
  CHECK(cfg.geo_tol == Catch::Approx(1e-8));
  CHECK(cfg.log_level == 1);
  CHECK(cfg.gmsh_verbosity == 3);
  CHECK(cfg.tools.vabs == "vabs4");
  CHECK(cfg.tools.swiftcomp == "sc");
  CHECK(cfg.tools.gmsh == "/opt/gmsh");
  CHECK(cfg.paths.material_db == "/db");
  // gmsh options stored per section as JSON text, ready to write into the .opt.
  CHECK(cfg.gmsh.at("general").at("General.BackgroundGradient") == "0");
  CHECK(cfg.gmsh.at("general").at("Geometry.LineWidth") == "1");
  CHECK(cfg.gmsh.at("homogenization").at("Mesh.SurfaceFaces") == "1");
}

TEST_CASE("gmsh options merge per key within a section, strings quoted",
          "[config]") {
  AppConfig cfg;
  applyJson(cfg, json{{"gmsh", {{"general", {
      {"Mesh.ColorCarousel", 2},
      {"General.Axes", 3}
  }}}}});
  // Higher level overrides one key, adds another, keeps the rest.
  applyJson(cfg, json{{"gmsh", {{"general", {
      {"General.Axes", 0},
      {"General.Text", "hello"}
  }}}}});

  CHECK(cfg.gmsh.at("general").at("Mesh.ColorCarousel") == "2");   // retained
  CHECK(cfg.gmsh.at("general").at("General.Axes") == "0");         // overridden
  CHECK(cfg.gmsh.at("general").at("General.Text") == "\"hello\""); // string quoted
}

TEST_CASE("later config overrides earlier, field by field", "[config]") {
  AppConfig cfg;
  // Level A: sets tool paths.
  applyJson(cfg, json{{"tools", {{"vabs", "A_vabs"}, {"gmsh", "A_gmsh"}}}});
  // Level B (higher priority): overrides only vabs.
  applyJson(cfg, json{{"tools", {{"vabs", "B_vabs"}}}});

  CHECK(cfg.tools.vabs == "B_vabs");  // overridden by B
  CHECK(cfg.tools.gmsh == "A_gmsh");  // retained from A
}

TEST_CASE("tryLoadConfigFile reports NotFound for a missing file", "[config]") {
  AppConfig cfg;
  CHECK(tryLoadConfigFile(cfg, "no_such_prevabs_config_file.json") ==
        ConfigFileStatus::NotFound);
  // Config untouched.
  CHECK(cfg.tools.vabs == "VABS");
}

TEST_CASE("tryLoadConfigFile loads a valid file and applies it", "[config]") {
  TempFile tf("test_cfg_valid.json",
              R"({"tools": {"vabs": "from_file"}})");
  AppConfig cfg;
  CHECK(tryLoadConfigFile(cfg, tf.path) == ConfigFileStatus::Loaded);
  CHECK(cfg.tools.vabs == "from_file");
}

TEST_CASE("tryLoadConfigFile reports ParseError and leaves config unchanged",
          "[config]") {
  TempFile tf("test_cfg_broken.json", "{ this is not valid json ");
  AppConfig cfg;
  cfg.tools.vabs = "sentinel";
  CHECK(tryLoadConfigFile(cfg, tf.path) == ConfigFileStatus::ParseError);
  // Malformed file must not corrupt existing values.
  CHECK(cfg.tools.vabs == "sentinel");
}

TEST_CASE("toJson round-trips through applyJson", "[config]") {
  AppConfig original;
  original.tol = 2e-11;
  original.tools.vabs = "custom_vabs";
  original.paths.material_db = "/mat/db";
  original.gmsh["general"]["General.Axes"] = "0";
  original.gmsh["general"]["General.Text"] = "\"hi\"";
  original.gmsh["recovery"]["Mesh.SurfaceFaces"] = "0";

  AppConfig restored;
  applyJson(restored, toJson(original));

  CHECK(restored.tol == Catch::Approx(original.tol));
  CHECK(restored.tools.vabs == original.tools.vabs);
  CHECK(restored.tools.swiftcomp == original.tools.swiftcomp);
  CHECK(restored.tools.gmsh == original.tools.gmsh);
  CHECK(restored.paths.material_db == original.paths.material_db);
  CHECK(restored.gmsh == original.gmsh);
}
