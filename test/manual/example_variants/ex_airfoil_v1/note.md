# Variant of ex_airfoil

**Base example:** `share/examples/ex_airfoil`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: LPS_3/TE top boundary l34 x2 0.536616 -> 0.5
- geometry: front web x 0.161 -> 0.2
- geometry: rear web x 0.511 -> 0.45
- layup: layup1 layer `20:18` -> `20:20`
- layup: layup2 layer `20:33` -> `20:30`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
