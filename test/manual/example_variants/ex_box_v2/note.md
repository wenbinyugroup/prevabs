# Variant of ex_box

**Base example:** `share/examples/ex_box`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: bottom-left corner (-0.4765,-0.265) -> (-0.4,-0.22)
- geometry: bottom-right corner (0.4765,-0.265) -> (0.4,-0.22)
- material: `e2` 1.42e6 -> 1.6e6
- material: `nu12` 0.30 -> 0.25
- layup: layer `15:6` -> `45:4`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
