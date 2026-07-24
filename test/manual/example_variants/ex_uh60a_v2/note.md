# Variant of ex_uh60a

**Base example:** `share/examples/ex_uh60a`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: spar LE top point x2 0.9 -> 0.92
- geometry: spar LE bottom point x2 0.9 -> 0.92
- geometry: nsm circle center 0.97 -> 0.95
- geometry: nsm circle `discrete by="angle"` 9 -> 6 deg
- geometry: front-fill seed pt_fill_le 0.95 -> 0.93

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
