# Example Variants

Parameter-variation test cases derived from `share/examples`. For every example
there are **two variants**, each a self-contained subdirectory in which
**exactly 5 parameters** have been adjusted. Only **geometry**,
**material/layup**, and **segment/component/topology** parameters are changed —
general settings (mesh size, scale, translate, rotate, element type/shape) are
kept identical to the base example. Each variant's `note.md` lists its 5
changes. Parameter meanings follow the user guide under
`doc/user/source/contents/guide`.

## Layout

```
<example>_v1/
  variant.json     # base name, main input, prevabs args
  note.md          # human-readable list of the changed parameters
  <input>.xml      # the modified input
  <support files>  # copied .dat / material .xml the input needs
  run.log          # written by the runner (git-ignored output)
<example>_v2/
  ...
```

`variant.json` schema:

```json
{
  "base": "ex_box",
  "input": "box_cus.xml",
  "args": ["-i", "box_cus.xml", "--hm", "--nopopup"]
}
```

Every variant runs in homogenization **build** mode (`--hm --nopopup`, no `-e`),
so it exercises geometry construction, offsetting and meshing / SG generation
without requiring VABS. The recover (`ex_airfoil_r`) and failure
(`ex_uh60a_f`) bases are run in the same build mode; their global loads are kept
as an additional varied parameter but only matter for the recovery/failure step.

## Running

```powershell
# From repo root — run all variants, write variants-report.md
test\manual\example_variants\run-variants.cmd

# Only a subset (regex on the variant directory name)
test\manual\example_variants\run-variants.cmd -Filter airfoil
test\manual\example_variants\run-variants.cmd -Filter "box_v1$"

# Longer per-variant timeout
test\manual\example_variants\run-variants.cmd -TimeoutSec 120
```

A variant **PASS**es when prevabs exits 0 and freshly writes its SG input file
(`<input>.sg`); otherwise it is `FAIL`, `TIMEOUT`, or `ERROR`. Failures are
logged to `<variant>/run.log` and **skipped** — a failing variant never blocks
the ones that follow. A Markdown summary is written to `variants-report.md`, and
the script exits 0 regardless so a wrapper does not treat expected variant
failures as a hard error; check the report for per-variant status.

Requires `build_msvc\Release\prevabs.exe` (build with `tools\build-msvc.cmd fast`).
