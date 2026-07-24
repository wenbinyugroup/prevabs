# Variant of ex_tube

**Base example:** `share/examples/ex_tube`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: circle `radius` 10 -> 8
- geometry: circle `discrete by="angle"` 3 -> 5 deg
- geometry: circle center (0,0) -> (1,0)
- material: `e1` 1.03e10 -> 1.2e10
- layup: stack code `[(45/-45):2/0:2/90]s` -> `[0/90/45/-45]s`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
