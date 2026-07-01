# t0_dcel — DCEL topology harness

Standalone harness exercising `PDCEL::splitFaceByPolyline` and the
repeated-split *band subdivision* pattern used by
`buildLayeredOffsetAreas` / `splitLayerBandIntoCells`.

For every case it builds a DCEL by hand, runs the split(s), calls
`dcel.validate()`, then writes:

- `out/<case>.dcel_dump.txt` — full DCEL state (`PDCEL::dumpToFile`)
- `out/<case>.svg` — rendering: filled faces, all edges (grey), the split
  path (red), vertices + labels.

No Gmsh required. The source closure mirrors the unit `test_dcel` target
(`PModel.cpp` is gmsh-coupled and excluded; the single free function it
provides, `plotGeoSnapshotImpl`, is stubbed in `t0_dcel.cpp`).

## Build & run

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File test\integration\t0_dcel\build.ps1 full -Run
```

`fast` for incremental, `clean` to drop the build dir. Exit code is non-zero
if any non-stress case fails. Outputs land in `build_msvc/out/`.

## Cases (increasing complexity)

| Case | What it covers |
|---|---|
| 1 `straight_chord` | single 2-vertex chord (≡ `splitFace`) |
| 2 `bent_path` | one interior bend (3-vertex path) |
| 3 `zigzag_multibend` | 5-vertex multi-bend interior polyline |
| 4 `layer_band` | healthy arch band subdivided into 3 cells (repeated splits, tail-endpoint remaining-face strategy) |
| 5 `endpoints_on_vertices` | path endpoints coincide with existing corners (canonicalize reuse branch) |
| 6 `thin_pinch_band` | **stress**: thin/pinched band (degenerate class from the Z concave-corner diagnosis). Reported, not asserted — the dump/SVG document whatever happens |
| 8 `z_layered_spec` | **spec replay**: the t2_z/z 3-layer layered offset, replayed step by step against `dev-docs/dcel/test_split_face_z_expected.md`; each step's bounded-face set is asserted against the expected topology |
| 7 `invalid_inputs` | 4 malformed inputs must return `{}` without mutating the DCEL |

### Case 8 — step-by-step spec replay

Geometry is the **clean-miter** offset from
[test_split_face_z_expected.md](../../../../../rnd/notes/dev/prevabs/dev-docs/dcel/test_split_face_z_expected.md)
(base + offsets at 0.02 / 0.05 / 0.07, 3 layers):

```
base   v1..v4 : (-0.5,0.5) (0,0.5)   (0,-0.5)   (0.5,-0.5)
0.02   v9..v12: (-0.5,0.52)(0.02,0.52)(0.02,-0.48)(0.5,-0.48)
0.05  v13..v16: (-0.5,0.55)(0.05,0.55)(0.05,-0.45)(0.5,-0.45)
shell  v5..v8 : (-0.5,0.57)(0.07,0.57)(0.07,-0.43)(0.5,-0.43)
```

The harness replays `buildLayeredOffsetAreas`' flow and emits a dump + SVG per
step (`case8_z_layered_spec_step{1..5}.*`); `checkFaces` asserts the bounded-face
set after each step (order/rotation/orientation independent):

```
state  expected 1 faces  (shell octagon)
step1  expected 2 faces  (layer1 band + rest)
step2  expected 3 faces  (layer1 + layer2 + layer3 bands)
step3  expected 5 faces  (layer1 -> 3 cells)
step4  expected 7 faces  (layer2 -> 3 cells)
step5  expected 9 faces  (layer3 -> 3 cells)
```

**Finding:** on this clean geometry `splitFaceByPolyline` + the connector
subdivision reproduce the expected topology exactly, with no change to the split
primitive. The earlier `t2_z/z` crash was the *degenerate offset input* (skewed
miter corners → near-collinear connector at a Z bend), not the split algorithm.

2D convention: geometry lives in the y-z plane (x = 0); SVG uses (y, z).

## Adding a case

Add a `case_*()` function in `t0_dcel.cpp`, call `emit(name, dcel, highlights,
title)` to dump + render, and `report(name, ok, detail)` for the pass/fail
line. Then call it from `main()`.
