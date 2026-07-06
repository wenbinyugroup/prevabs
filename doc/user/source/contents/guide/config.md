(guide-config)=
# Configuration files

Besides the per-model XML input, PreVABS reads optional **JSON configuration
files** that set persistent run parameters — things you usually want the same
across many runs, such as the location of the VABS / Gmsh executables, a default
material database, geometry tolerances, and Gmsh visualization defaults.

Configuration files are entirely separate from the cross-section XML input:

- the **XML input** describes *one* cross-section (geometry, materials, layup);
- a **config file** describes *how PreVABS runs* on your machine, and applies to
  every run until you change it.

## Where config files are read from

On each run PreVABS looks for config files in the following locations, from
lowest to highest priority. Values from later locations override earlier ones,
**field by field** (an unspecified field keeps the value from a lower level):

1. Built-in defaults.
2. `prevabs.json` next to the `prevabs` executable.
3. `.prevabs.json` in your home directory (`%USERPROFILE%` on Windows).
4. `.prevabs.json` in the directory of the XML input file.
5. A file given explicitly with `--config <path>`.
6. Command-line flags (for example `--gmsh-verbosity`) — applied last.

All config files are optional. A location that has no file is simply skipped.
This lets you keep, for example, a machine-wide default next to the executable,
a personal override in your home directory, and a project-specific override
beside a particular model.

## File format

A config file is a JSON object with a handful of optional sections. Every field
is optional; include only the ones you want to change. A full template with the
default values is shipped as `prevabs.example.json` in the repository root.

```{code-block} json
:name: code-config-example
:caption: A config file setting the solver paths and a default material database.

{
  "tools": {
    "vabs": "C:/tools/VABSIII.exe",
    "gmsh": "gmsh"
  },
  "paths": {
    "material_db": "C:/prevabs/MaterialDB.xml"
  }
}
```

For the meaning and default of every field, see the reference page
[](../ref/config.md).

## Using `--config`

`--config <path>` loads an extra config file on top of the auto-discovered ones,
as the highest-priority *file* layer (command-line flags still win). It does not
replace the automatic search — it is merged on top of it.

```{code-block} bash
:caption: Run with an explicit config file.

prevabs -i model.xml --hm --config C:/prevabs/hpc.json
```

## Common tasks

**Point PreVABS at a specific VABS or Gmsh executable.** Set `tools.vabs`,
`tools.swiftcomp`, or `tools.gmsh` to an absolute path. A bare name (the
default) is resolved through the system `PATH`; an absolute path is used as-is.

**Load a default material database on every run.** Set `paths.material_db` to
the full path of a material-database XML file. It is read on every run, in
addition to (not instead of) any `<include><material>` database referenced from
the XML input. This is simply the configurable form of the built-in
`MaterialDB.xml` (see [](#input-other-settings)); when you set it, a
missing file is treated as an error.

**Customize the Gmsh view.** Add Gmsh options under the `gmsh` section (see
below). Since the `gmsh` section merges across config levels like every other
field, an entry you set in your home or project config overrides the shipped
default for that option.

## Gmsh view options (the `gmsh` section)

The default Gmsh view (rotation, scale, mesh colouring, edge/face visibility)
is not hard-coded — it lives in the `gmsh` section of the `prevabs.json` that
ships next to the executable (config level 2). Each time PreVABS writes the
`.opt` file it emits these options.

The `gmsh` section is grouped into one mode-independent block plus one block per
analysis type. Keys are raw Gmsh option names (https://gmsh.info/doc/texinfo/gmsh.html#Gmsh-options); values are written verbatim into
the `.opt` (numbers as-is, strings quoted). PreVABS does not check the option
names or values — an invalid one is only reported by Gmsh itself, when the view
is opened with `-v`.

```{code-block} json
:name: code-gmsh-section
:caption: The gmsh section of a prevabs.json config file.

{
  "gmsh": {
    "general":        { "General.Axes": 3, "Mesh.ColorCarousel": 2 },
    "homogenization": { "Mesh.SurfaceFaces": 1 },
    "recovery":       { "Mesh.Points": 0, "Mesh.SurfaceFaces": 0 }
  }
}
```

For each run PreVABS writes the `general` block followed by the block for the
current analysis (`homogenization`, or `recovery` for dehomogenization /
failure). Edit the shipped values, or add options in your own config file, to
change the default view.

**Stop PreVABS from writing any options.** If you keep your own Gmsh option
settings and do not want PreVABS to override them, empty the `gmsh` section
(`"gmsh": {}`) in the shipped `prevabs.json`, or delete that file. PreVABS then
writes an empty `.opt`, leaving your Gmsh configuration untouched. (A
higher-level config can only add or change options, not remove a shipped
default, so suppression is done by editing the shipped file.)

## When a config file is wrong

- A missing config file (at any auto-discovered location, or a missing
  `--config` file) is reported as a warning and ignored.
- A file that exists but is not valid JSON is reported as a warning and ignored;
  the run continues with the values from the other levels.
- An explicitly configured `paths.material_db` that does not exist stops the run
  with an error, because a missing material database would change the results.
- Entries in the `gmsh` section are passed through to Gmsh without validation.
  A wrong option name or value is not caught by PreVABS; Gmsh reports it when the
  `.opt` is loaded (only when visualizing with `-v`), and does not affect the
  analysis results.
