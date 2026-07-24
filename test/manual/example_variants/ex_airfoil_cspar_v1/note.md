# Variant of ex_airfoil_cspar

**Base example:** `share/examples/ex_airfoil_cspar`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: main-spar aft top point p3 x2 0.6 -> 0.55
- geometry: main-spar aft bottom point p4 x2 0.6 -> 0.55
- geometry: TE division top point p7 x2 0.2 -> 0.25
- segment: lyp_spar_3 range begin/end 0.2/0.8 -> 0.15/0.85
- layup: lyp_spar_1 layer `45:8` -> `45:10`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
