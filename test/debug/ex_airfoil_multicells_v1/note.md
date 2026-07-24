# Variant of ex_airfoil_multi-cells

**Base example:** `share/examples/ex_airfoil_multi-cells`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: web1 anchor x 0.1 -> 0.15
- geometry: web3 anchor x 0.3 -> 0.35
- geometry: web5 anchor x 0.5 -> 0.55
- geometry: web9 anchor x 0.9 -> 0.85
- layup: layup1 layer `20:18` -> `20:22`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
