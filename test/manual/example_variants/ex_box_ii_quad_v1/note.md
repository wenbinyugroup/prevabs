# Variant of ex_box_ii_quad

**Base example:** `share/examples/ex_box_ii_quad`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: top-left corner y 0.2 -> 0.28
- geometry: top-right corner y 0.2 -> 0.28
- layup: layup1 layer `90:2` -> `45:2`
- layup: layup2 layer `0:3` -> `0:4`
- layup: layup2 layer 90 -> 60 deg

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
