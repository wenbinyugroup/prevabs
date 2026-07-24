# Variant of ex_box_nested_webs

**Base example:** `share/examples/ex_box_nested_webs`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: left-top corner (-0.809,0.5) -> (-0.9,0.6)
- geometry: right-top corner (0.809,0.5) -> (0.9,0.6)
- material: lamina `thickness` 0.02 -> 0.025
- layup: first layer 0 -> 45 deg
- material: `e2` 5 -> 6

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
