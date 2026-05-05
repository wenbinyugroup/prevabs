I-beam example using an external material definition.

## Overview

```{figure} exampleibeam0.png
:name: fig_ibeam_draw
:width: 6in
:align: center

Cross-section of the I-beam.
```

This example has an I-shape cross-section.
The dimensions shown in {numref}`fig_ibeam_draw` are $w_1=2.0$ m,
$w_2=3.0$ m, $h=3.0$ m, $t_1=0.11$ m, $t_2=0.065$ m, $t_w=0.08$ m.
Materials and layups are given in {ref}`table_ibeam_materials` and
{ref}`table_ibeam_layups`.

```{csv-table} Material properties
:name: table_ibeam_materials
:header-rows: 2
:align: center

"Name", "Type", "Density", "{{ e1 }}", "{{ e2 }}", "{{ e3 }}", "{{ g12 }}", "{{ g13 }}", "{{ g23 }}", "{{ nu12 }}", "{{ nu13 }}", "{{ nu23 }}"
, , "{{ den_si_k }}", "{{ mod_si_g }}", "{{ mod_si_g }}", "{{ mod_si_g }}", "{{ mod_si_g }}", "{{ mod_si_g }}", "{{ mod_si_g }}", , ,
"iso5_1", "orthotropic", 1.86, 37.00, 9.00, 9.00, 4.00, 4.00, 4.00, 0.28, 0.28, 0.28
"iso5_2", "orthotropic", 1.83, 10.30, 10.30, 10.30, 8.00, 8.00, 8.00, 0.30, 0.30, 0.30
"iso5_3", "orthotropic", 1.83, 1e-8, 1e-8, 1e-8, 1e-9, 1e-9, 1e-9, 0.30, 0.30, 0.30
"iso5_4", "orthotropic", 1.664, 10.30, 10.30, 10.30, 8.00, 8.00, 8.00, 0.30, 0.30, 0.30
"iso5_5", "orthotropic", 0.128, 0.01, 0.01, 0.01, 2e-4, 2e-4, 2e-4, 0.30, 0.30, 0.30
```

```{csv-table} Layups
:name: table_ibeam_layups
:header-rows: 2
:align: center

"Name", "Layer", "Material", "Ply thickness", "Orientation", "Number of plies"
, , , "{{ len_si }}", "$\circ$",
"layup1", 1, "iso5_1", 0.03, 90, 2
, 2, "iso5_2", 0.05, 0, 1
"layup2", 1, "iso5_3", 0.015, 0, 3
, 2, "iso5_4", 0.02, 90, 1
"layup_web", 1, "iso5_5", 0.02, 0, 4
```

```{figure} exampleibeam1.png
:name: fig_ibeam1
:width: 6in
:align: center

*Base point*s, *Base line*s and *Segment*s of the I beam cross section.
```

---

## Input

- [`i_web.xml`](i_web.xml)
- [`materials.xml`](materials.xml)

---

## Run the example

```shell
prevabs -i i_web.xml --hm
```

---

## Output

```{figure} i_web.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `i_web.png`

---

## Analysis Result

```{csv-table} Results
:name: table_ibeam_result
:align: center

"$\phantom{-}2.749\times 10^9$", "$-4.763\times 10^{-8}$", "$-1.505\times 10^{-14}$", "$-5.734\times 10^{-8}$", "$-1.945\times 10^9$", "$\phantom{-}2.779\times 10^3$"
"$-4.763\times 10^{-8}$", "$\phantom{-}1.362\times 10^9$", "$\phantom{-}4.309\times 10^2$", "$\phantom{-}1.645\times 10^9$", "$\phantom{-}1.277\times 10^{-7}$", "$-4.362\times 10^{-14}$"
"$-1.505\times 10^{-14}$", "$\phantom{-}4.309\times 10^2$", "$\phantom{-}4.729\times 10^4$", "$\phantom{-}5.201\times 10^2$", "$\phantom{-}4.038\times 10^{-14}$", "$-4.775\times 10^{-13}$"
"$-5.734\times 10^{-8}$", "$\phantom{-}1.645\times 10^9$", "$\phantom{-}5.201\times 10^2$", "$\phantom{-}1.990\times 10^9$", "$\phantom{-}1.541\times 10^{-7}$", "$\phantom{-}7.025\times 10^{-14}$"
"$-1.945\times 10^9$", "$\phantom{-}1.277\times 10^{-7}$", "$\phantom{-}4.038\times 10^{-14}$", "$\phantom{-}1.541\times 10^{-7}$", "$\phantom{-}5.376\times 10^9$", "$-5.274\times 10^2$"
"$\phantom{-}2.779\times 10^3$", "$-4.362\times 10^{-14}$", "$-4.775\times 10^{-13}$", "$\phantom{-}7.025\times 10^{-14}$", "$-5.274\times 10^2$", "$\phantom{-}1.173\times 10^9$"
```
