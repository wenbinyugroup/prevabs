# Variant of ex_box_nested_webs

**Base example:** `share/examples/ex_box_nested_webs`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: web1 anchor (-0.191,0) -> (-0.3,0)
- geometry: web3 anchor (0.191,0.191) -> (0.25,0.15)
- material: `e1` 4 -> 6
- layup: second layer 90 -> 60 deg
- geometry: left-bottom corner (-0.809,-0.5) -> (-0.85,-0.6)

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
