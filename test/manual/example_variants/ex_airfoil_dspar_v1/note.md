# Variant of ex_airfoil_dspar

**Base example:** `share/examples/ex_airfoil_dspar`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: spar aft top point p3 x2 0.6 -> 0.65
- geometry: spar aft bottom point p4 x2 0.6 -> 0.65
- geometry: TE division top point p7 x2 0.2 -> 0.15
- geometry: aft-fill seed pfte1 x 0.5 -> 0.45
- layup: lyp_spar first layer `-34:8` -> `-34:10`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
