# Segment build report

Generated: 2026-06-24 17:05:17

Segments: **17**  —  PASS **10** / WARN **6** / FAIL **1**

| status | case | segment | route | closed | base | layers | faces | inv | sliver | degen | sec | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ✅ PASS | airfoil_skin | sgm_1 | layered | False | 12 | 3 | 33 | 0 | 0 | 0 | 0.13 |  |
| ⚠️ WARN | airfoil_skin_closed_ah79k132 | sg_le | legacy | True | 97 | 0 | 96 | 0 | 15 | 0 | 0.15 | 15 sliver |
| ⚠️ WARN | airfoil_skin_closed_mh104 | sg_le | legacy | True | 67 | 0 | 201 | 0 | 5 | 0 | 0.21 | 5 sliver |
| ❌ FAIL | airfoil_te_adaptive | - | - | - | 0 | 0 | 0 | 0 | 0 | 0 | 0.55 | exit=1 |
| ✅ PASS | arc_coarse | sgm_1 | layered | False | 10 | 3 | 27 | 0 | 0 | 0 | 0.14 |  |
| ✅ PASS | arc_fine | sgm_1 | layered | False | 91 | 4 | 360 | 0 | 0 | 0 | 0.18 |  |
| ✅ PASS | arc_semicircle | sgm_1 | layered | False | 37 | 3 | 108 | 0 | 0 | 0 | 0.14 |  |
| ✅ PASS | circle_closed | sgm_1 | legacy | True | 37 | 0 | 108 | 0 | 0 | 0 | 0.15 |  |
| ⚠️ WARN | ibeam_flanges | sgbottom | legacy | False | 3 | 0 | 2 | 0 | 0 | 0 | 0.15 | open fell back to legacy |
| ⚠️ WARN | ibeam_flanges | sgtop | legacy | False | 3 | 0 | 2 | 0 | 0 | 0 | 0.15 | open fell back to legacy |
| ✅ PASS | line_1layer | sgm_1 | layered | False | 2 | 1 | 1 | 0 | 0 | 0 | 0.12 |  |
| ✅ PASS | line_4layer | sgm_1 | layered | False | 2 | 4 | 4 | 0 | 0 | 0 | 0.15 |  |
| ⚠️ WARN | line_collinear_3pt | sgm_1 | legacy | False | 3 | 0 | 7 | 0 | 0 | 0 | 0.17 | open fell back to legacy |
| ✅ PASS | line_nonuniform | sgm_1 | layered | False | 2 | 3 | 3 | 0 | 0 | 0 | 0.14 |  |
| ✅ PASS | line_thin_many | sgm_1 | layered | False | 2 | 8 | 8 | 0 | 0 | 0 | 0.24 |  |
| ✅ PASS | vbend_multibend | sgm_1 | layered | False | 4 | 3 | 9 | 0 | 0 | 0 | 0.12 |  |
| ⚠️ WARN | vbend_offset_out | sg_v | legacy | False | 3 | 0 | 21 | 0 | 0 | 0 | 0.2 | open fell back to legacy |

Per-segment visuals: `<case>.<segment>.segment.svg`  ·  gmsh mesh: `<case>.msh`

## Build paths

Which conditional branch each segment actually took (full per-segment file: `<case>.<segment>.segment.path.txt`).

### airfoil_skin / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=12)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: open multi-layer route-ii (n_layers=3)
5. layered: tiled all layers (SUCCESS, faces=33)
6. buildAreas: layered OK -> AreasBuilt
```

### airfoil_skin_closed_ah79k132 / sg_le — WARN
```
1. buildAreas: enter (closed=Y, base_verts=97)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=1, side=1)
4. layered: shell/base vertex mismatch (base=96, shell=75) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=96 -> AreasBuilt
```

### airfoil_skin_closed_mh104 / sg_le — WARN
```
1. buildAreas: enter (closed=Y, base_verts=67)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: shell/base vertex mismatch (base=66, shell=60) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=67 -> AreasBuilt
```

### airfoil_te_adaptive / - — FAIL
```
(no path: exit=1)
```

### arc_coarse / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=10)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: open multi-layer route-ii (n_layers=3)
5. layered: tiled all layers (SUCCESS, faces=27)
6. buildAreas: layered OK -> AreasBuilt
```

### arc_fine / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=91)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=4, side=1)
4. layered: open multi-layer route-ii (n_layers=4)
5. layered: tiled all layers (SUCCESS, faces=360)
6. buildAreas: layered OK -> AreasBuilt
```

### arc_semicircle / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=37)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: open multi-layer route-ii (n_layers=3)
5. layered: tiled all layers (SUCCESS, faces=108)
6. buildAreas: layered OK -> AreasBuilt
```

### circle_closed / sgm_1 — PASS
```
1. buildAreas: enter (closed=Y, base_verts=37)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: closed multi-layer (no closed-loop tiling yet) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=36 -> AreasBuilt
```

### ibeam_flanges / sgbottom — WARN
```
1. buildAreas: enter (closed=N, base_verts=3)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=2, side=1)
4. layered: shell/base vertex mismatch (base=3, shell=2) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=1 -> AreasBuilt
```

### ibeam_flanges / sgtop — WARN
```
1. buildAreas: enter (closed=N, base_verts=3)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=2, side=1)
4. layered: shell/base vertex mismatch (base=3, shell=2) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=1 -> AreasBuilt
```

### line_1layer / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=2)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=1, side=1)
4. layered: n_layers==1 -> single face (SUCCESS)
5. buildAreas: layered OK -> AreasBuilt
```

### line_4layer / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=2)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=4, side=1)
4. layered: open multi-layer route-ii (n_layers=4)
5. layered: tiled all layers (SUCCESS, faces=4)
6. buildAreas: layered OK -> AreasBuilt
```

### line_collinear_3pt / sgm_1 — WARN
```
1. buildAreas: enter (closed=N, base_verts=3)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=7, side=1)
4. layered: shell/base vertex mismatch (base=3, shell=2) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=1 -> AreasBuilt
```

### line_nonuniform / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=2)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: open multi-layer route-ii (n_layers=3)
5. layered: tiled all layers (SUCCESS, faces=3)
6. buildAreas: layered OK -> AreasBuilt
```

### line_thin_many / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=2)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=8, side=1)
4. layered: open multi-layer route-ii (n_layers=8)
5. layered: tiled all layers (SUCCESS, faces=8)
6. buildAreas: layered OK -> AreasBuilt
```

### vbend_multibend / sgm_1 — PASS
```
1. buildAreas: enter (closed=N, base_verts=4)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=3, side=1)
4. layered: open multi-layer route-ii (n_layers=3)
5. layered: tiled all layers (SUCCESS, faces=9)
6. buildAreas: layered OK -> AreasBuilt
```

### vbend_offset_out / sg_v — WARN
```
1. buildAreas: enter (closed=N, base_verts=3)
2. buildAreas: useLayeredOffset=yes
3. layered: enter (n_layers=7, side=-1)
4. layered: shell/base vertex mismatch (base=3, shell=4) -> FALLBACK
5. buildAreas: layered FELL BACK -> legacy
6. legacy: built areas=3 -> AreasBuilt
```
