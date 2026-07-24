# Variant of ex_airfoil_cspar

**Base example:** `share/examples/ex_airfoil_cspar`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: main-spar fwd top point p1 x2 0.8 -> 0.85
- geometry: main-spar fwd bottom point p2 x2 0.8 -> 0.85
- geometry: aft-fill seed pf1 x 0.1 -> 0.12
- layup: lyp_spar_2 layer `0:8` -> `0:10`
- segment: lyp_spar_2 range begin/end 0.1/0.9 -> 0.05/0.95

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
