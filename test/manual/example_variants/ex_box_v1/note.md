# Variant of ex_box

**Base example:** `share/examples/ex_box`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: top-left corner (-0.4765,0.265) -> (-0.5,0.28)
- geometry: top-right corner (0.4765,0.265) -> (0.5,0.28)
- material: `e1` 20.59e6 -> 22.0e6
- material: lamina `thickness` 0.005 -> 0.006
- layup: layer `15:6` -> `30:8`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
