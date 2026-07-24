# Variant of ex_pipe

**Base example:** `share/examples/ex_pipe`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: right arc `discrete by="angle"` 2 -> 4 deg
- geometry: left arc `discrete by="number"` 90 -> 60 segments
- layup: layup_2 layer -45 -> -30 deg
- layup: layup_2 layer 45 -> 30 deg
- material: `nu12` 0.42 -> 0.35

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
