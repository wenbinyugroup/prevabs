# Variant of ex_uh60a_f

**Base example:** `share/examples/ex_uh60a_f`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: LE division top point p5 x2 -0.02 -> -0.03
- geometry: spar TE bottom point p3 x2 -0.353742 -> -0.34
- geometry: nsm circle center -0.035771 -> -0.035
- geometry: nsm circle `discrete by="angle"` 9 -> 7 deg
- layup: lyp_le layer `5:4` -> `5:6`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
