# Variant of ex_airfoil_multi-cells

**Base example:** `share/examples/ex_airfoil_multi-cells`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: web2 anchor x 0.2 -> 0.25
- geometry: web4 anchor x 0.4 -> 0.45
- geometry: web6 anchor x 0.6 -> 0.65
- geometry: web8 anchor x 0.8 -> 0.85
- layup: layup1 layer `20:18` -> `20:16`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
