# Variant of ex_channel

**Base example:** `share/examples/ex_channel`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: bottom-left point y -12.7e-3 -> -15.0e-3
- geometry: top-left point y 12.7e-3 -> 15.0e-3
- geometry: top-right point (12.7,12.7)e-3 -> (15.0,15.0)e-3
- geometry: bottom-right point (25.4,-12.7)e-3 -> (28.0,-15.0)e-3
- material: isotropic `e` 206.843e9 -> 70.0e9

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
