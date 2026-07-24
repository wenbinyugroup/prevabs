# Variant of ex_tube

**Base example:** `share/examples/ex_tube`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: circle `radius` 10 -> 12
- geometry: circle `discrete by="angle"` 3 -> 2 deg
- material: `density` 1.664e3 -> 1.8e3
- material: `g12` 8.00e9 -> 7.0e9
- layup: stack code `[(45/-45):2/0:2/90]s` -> `[(30/-30):2/0:4/90]s`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
