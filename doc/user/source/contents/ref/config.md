(section-config-reference)=
# Configuration file reference

This page documents every field of the PreVABS JSON configuration file. For an
introduction to what configuration files are, where they are loaded from, and
how the levels are merged, see the guide page [](../guide/config.md).

A configuration file is a JSON object. All sections and all fields are optional;
any field that is omitted keeps its value from a lower-priority level (or its
built-in default). Unknown sections and fields are ignored.

```{code-block} json
:name: code-config-full
:caption: All configuration sections with their default values.

{
  "numerics": { "tol": 1e-12, "geo_tol": 1e-9 },
  "output":   { "log_level": 2, "gmsh_verbosity": 2 },
  "tools":    { "vabs": "VABS", "swiftcomp": "SwiftComp", "gmsh": "gmsh" },
  "paths":    { "material_db": "" },
  "gmsh":     { "general": {}, "homogenization": {}, "recovery": {} }
}
```

---

## `numerics`

Numerical tolerances used in geometric computation.

- `tol` - Intersection-parameter tolerance. Number. Default `1e-12`. Equivalent
  to the XML `<general><tolerance>` value.
- `geo_tol` - Initial geometry tolerance. Number. Default `1e-9`. The
  cross-section reader replaces this with a model-scale value before component
  geometry is built, so this is only the starting point.

---

## `output`

Logging and Gmsh message verbosity.

- `log_level` - Verbosity of the PreVABS log. Integer `0`–`5`, where
  `0` = trace, `1` = debug, `2` = info, `3` = warning, `4` = error,
  `5` = fatal. Default `2` (info).
- `gmsh_verbosity` - Verbosity of Gmsh messages. Integer, one of
  `0` (silent), `1` (errors), `2` (warnings), `3` (info), `5` (debug).
  Default `2`. The command-line flag `--gmsh-verbosity` overrides this.

---

## `tools`

Executable command names or paths for the external programs PreVABS launches.
Each value may be a bare command name — resolved through the system `PATH` — or
an absolute path, which is used verbatim.

- `vabs` - VABS executable. String. Default `"VABS"`.
- `swiftcomp` - SwiftComp executable. String. Default `"SwiftComp"`.
- `gmsh` - Gmsh executable. String. Default `"gmsh"`.

---

## `paths`

Default file locations applied on every run.

- `material_db` - Full path to a material-database XML file read on **every**
  run. String. Default `""` (empty). When empty, PreVABS uses the built-in
  default `MaterialDB.xml` located next to the executable (read only if
  present). When set, the configured file is read and a missing file is an
  **error**. In either case this database does **not** replace the input's
  `<include><material>` database, which is still read independently; materials
  and laminae with the same name defined later override earlier ones.

---

## `gmsh`

Gmsh visualization options for the generated `.opt` file, grouped into
sub-sections. Object. Default `{}`.

- `general` - Options written for every analysis.
- `homogenization` - Options written additionally for a homogenization run.
- `recovery` - Options written additionally for a dehomogenization / failure
  (recovery) run.

Each sub-section is an object mapping a raw Gmsh option name (for example
`"Mesh.ColorCarousel"`) to its value. For a run PreVABS writes the `general`
block followed by the block for the current analysis. Values are written
verbatim into the generated `.opt` — numeric values as-is, string values quoted.
Entries merge per key across config levels (a higher level updates or adds
individual options without discarding those from lower levels).

The **default** view options ship in the `prevabs.json` located next to the
executable (config level 2); edit or empty its `gmsh` section to change or
suppress them. See [](../guide/config.md) for details.

PreVABS does **not** validate the option names or values; it only passes them
through. An invalid option is reported by Gmsh itself when the `.opt` file is
loaded (which only happens when visualizing with `-v`), and does not affect the
analysis results.
