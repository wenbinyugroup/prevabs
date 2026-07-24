# Variant of ex_pipe

**Base example:** `share/examples/ex_pipe`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: right arc `discrete by="angle"` 2 -> 3 deg
- geometry: left arc `discrete by="number"` 90 -> 120 segments
- layup: layup_1 layer 0 -> 15 deg
- layup: layup_1 layer 90 -> 75 deg
- material: `e2` 1.42e6 -> 1.6e6

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
