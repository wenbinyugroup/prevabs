# Variant of ex_airfoil_r

**Base example:** `share/examples/ex_airfoil_r`

Only geometry, material/layup, and segment/component/topology parameters are
changed (general settings such as mesh size, scale, translate, rotate and
element type/shape are kept as in the base). **5 changes:**

- geometry: LE top boundary l12 x2 0.004054 -> 0.005
- geometry: LPS_2/LPS_3 top boundary l23 x2 0.114740 -> 0.12
- geometry: rear web x 0.511 -> 0.53
- layup: layup3 layer `30:38` -> `30:40`
- layup: layup1 layer `20:18` -> `20:16`

Run mode: `prevabs -i <input>.xml --hm -v --nopopup -e` (build + VABS).
