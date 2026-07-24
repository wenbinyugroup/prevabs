# Variant of ex_ibeam

**Base example:** `share/examples/ex_ibeam`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: top-left flange x -1 -> -1.2
- geometry: top-right flange x 1 -> 1.2
- geometry: bottom-right depth y -3 -> -3.5
- geometry: bottom-left depth y -3 -> -3.5
- layup: web layer `0:4` -> `0:6`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
