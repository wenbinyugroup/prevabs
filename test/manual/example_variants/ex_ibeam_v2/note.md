# Variant of ex_ibeam

**Base example:** `share/examples/ex_ibeam`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: web anchor (0.04,-2) -> (0.0,-1.5)
- geometry: top-right flange x 1 -> 1.3
- geometry: bottom-right flange x 1.5 -> 1.8
- layup: layup_1 layer `90:2` -> `45:2`
- layup: web layer `0:4` -> `30:4`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
