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

**Customize the Gmsh view.** Set `gmsh_opt.template_file` to a Gmsh `.opt` file.
Its contents become the base of the generated view options; the analysis-specific
mesh visibility is still appended automatically.

## When a config file is wrong

- A missing config file (at any auto-discovered location, or a missing
  `--config` file) is reported as a warning and ignored.
- A file that exists but is not valid JSON is reported as a warning and ignored;
  the run continues with the values from the other levels.
- An explicitly configured `paths.material_db` that does not exist stops the run
  with an error, because a missing material database would change the results.
- An explicitly configured `gmsh_opt.template_file` that does not exist only
  produces a warning; PreVABS falls back to the built-in view defaults, since
  visualization does not affect analysis results.
