# Variant of ex_uh60a

**Base example:** `share/examples/ex_uh60a`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: spar LE top point x2 0.9 -> 0.88
- geometry: spar LE bottom point x2 0.9 -> 0.88
- geometry: spar TE top point x2 0.6 -> 0.62
- geometry: spar TE bottom point x2 0.6 -> 0.62
- geometry: non-structural-mass circle `radius` 0.006 -> 0.005

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
