# Variant of ex_uh60a_f

**Base example:** `share/examples/ex_uh60a_f`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: spar LE top point p1 x2 -0.127094 -> -0.13
- geometry: spar TE top point p2 x2 -0.353742 -> -0.36
- geometry: TE division top point p7 x2 -0.8 -> -0.78
- geometry: nsm circle `radius` 0.002928 -> 0.0025
- layup: lyp_spar first layer `-20:15` -> `-20:12`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
