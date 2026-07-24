# Variant of ex_airfoil_dspar

**Base example:** `share/examples/ex_airfoil_dspar`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: D-spar web arc `curvature` 0 -> 0.1 (bulged web)
- geometry: TE division bottom point p8 x2 0.2 -> 0.25
- geometry: spar aft top point p3 x2 0.6 -> 0.55
- layup: lyp_te layer `43:8` -> `43:6`
- layup: lyp_le angle `51:1` -> `45:1`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
