# Variant of ex_channel

**Base example:** `share/examples/ex_channel`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: top flange tip x 12.7e-3 -> 18.0e-3
- geometry: bottom flange tip x 25.4e-3 -> 30.0e-3
- material: isotropic `e` 206.843e9 -> 180.0e9
- material: `nu` 0.49 -> 0.33
- material: lamina `thickness` 0.001524 -> 0.002

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
