(example-box-beam)=
# Box beam

## Overview

```{figure} /figures/examplebox0.png
:name: fig_box_draw
:width: 4in
:align: center

Cross section of the box beam.
```

This example is a thin-walled box beam cross-section.
The width $a_2=0.953$ in, height $a_3=0.530$ in, and thickness $t=0.030$ in.
Each wall has six plies of the same composite material and the same fiber orientation of $15^\circ$.
Material properties and layup scheme are listed in {ref}`table_box_materials` and {ref}`table_box_layups`.


```{csv-table} Material properties
:name: table_box_materials
:header-rows: 2
:align: center

"Name", "Density", "{{ e1 }}", "{{ e2 }}", "{{ e3 }}", "{{ g12 }}", "{{ g13 }}", "{{ g23 }}", "{{ nu12 }}", "{{ nu13 }}", "{{ nu23 }}"
, "{{ den_im }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", , ,
"mat_1", 0.0001353, 20.59, 1.42, 1.42, 0.87, 0.87, 0.696, 0.30, 0.30, 0.34
```

```{csv-table} Layups
:name: table_box_layups
:header-rows: 2
:align: center

"Name", "Layer", "Material", "Ply thickness", "Orientation", "Number of plies"
, , , "{{ len_im }}", $\circ$,
"layup1", 1, "mat_1", 0.05, -15, 6
```

---

## PreVABS Input and Output

Input files

- [`box_cus.xml`](../../../../../share/examples/ex_box/box_cus.xml)


```{figure} ../../../../../share/examples/ex_box/box_cus.png
:name: fig_box
:width: 6in
:align: center

Cross-section viewed in Gmsh.
```

---

## Analysis Result

Cross-sectional properties are given in {ref}`table_box_result` and compared with those in Ref. [YU2012].
The tiny differences are due to different meshes.

```{csv-table} Results
:name: table_box_result
:header-rows: 1
:align: center

"Component", "Value", "Reference [YU2012]"
"$S_{11}$ [{{ stf0_im }}]", "$\phantom{-}1.437 \times 10^6$", "$\phantom{-}1.437 \times 10^6$"
"$S_{22}$ [{{ stf0_im }}]", "$\phantom{-}9.026 \times 10^4$", "$\phantom{-}9.027 \times 10^4$"
"$S_{33}$ [{{ stf0_im }}]", "$\phantom{-}3.941 \times 10^4$", "$\phantom{-}3.943 \times 10^4$"
"$S_{14}$ [{{ stf1_im }}]", "$\phantom{-}1.074 \times 10^5$", "$\phantom{-}1.074 \times 10^5$"
"$S_{25}$ [{{ stf1_im }}]", "$-5.201 \times 10^4$", "$-5.201 \times 10^4$"
"$S_{36}$ [{{ stf1_im }}]", "$-5.635 \times 10^4$", "$-5.635 \times 10^4$"
"$S_{44}$ [{{ stf2_im }}]", "$\phantom{-}1.679 \times 10^4$", "$\phantom{-}1.679 \times 10^4$"
"$S_{55}$ [{{ stf2_im }}]", "$\phantom{-}6.621 \times 10^4$", "$\phantom{-}6.621 \times 10^4$"
"$S_{66}$ [{{ stf2_im }}]", "$\phantom{-}1.725 \times 10^5$", "$\phantom{-}1.725 \times 10^5$"
```
