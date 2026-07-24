# Variant of ex_box_ii_quad

**Base example:** `share/examples/ex_box_ii_quad`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: bottom-left corner y -0.2 -> -0.15
- geometry: bottom-right corner y -0.2 -> -0.15
- layup: layup3 middle layer 0 -> 30 deg
- layup: layup1 layer 0 -> 45 deg
- component: wall_bottom layup layup2 -> layup1

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
