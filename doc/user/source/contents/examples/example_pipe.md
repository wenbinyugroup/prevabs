(example-pipe)=
# Pipe

## Overview

```{figure} /figures/examplepipe0.png
:name: fig_pipe_draw
:width: 6in
:align: center

Cross section of the pipe [YU2005].
```

This example has the cross-section as shown in {numref}`fig_pipe_draw` [YU2005].
This cross-section has two straight walls and two half circular walls.
$r=1.0$ in. and other dimensions are shown in the figure.
Each wall has the layup having two layers made from one material.
Fiber orientations for each layer are also given in the figure.
Material properties and layups are given in {ref}`table_pipe_materials` and {ref}`table_pipe_layups`.

```{csv-table} Material properties
:name: table_pipe_materials
:header-rows: 2
:align: center

"Name", "Density", "{{ e1 }}", "{{ e2 }}", "{{ e3 }}", "{{ g12 }}", "{{ g13 }}", "{{ g23 }}", "{{ nu12 }}", "{{ nu13 }}", "{{ nu23 }}"
, "{{ den_im }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", "{{ mod_im_m }}", , ,
"mat_1", 0.057, 20.59, 1.42, 1.42, 0.87, 0.87, 0.87, 0.42, 0.42, 0.42
```

```{csv-table} Layups
:name: table_pipe_layups
:header-rows: 2
:align: center

"Name", "Layer", "Material", "Ply thickness", "Orientation", "Number of plies"
, , , "{{ len_im }}", "$\circ$",
"layup_1", 1, "mat_1", 0.1, 0, 1
, 2, "mat_1", 0.1, 90, 1
"layup_2", 1, "mat_1", 0.1, -45, 1
, 2, "mat_1", 0.1, 45, 1
```

---

## PreVABS Input and Output

Input files

- [`pipe.xml`](../../../../../share/examples/ex_tube/pipe.xml)


```{figure} /figures/examplepipe1.png
:name: fig_pipe1
:width: 6in
:align: center

*Base point*s and *Base line*s of the pipe cross section.
```

```{figure} /figures/examplepipe2.png
:name: fig_pipe2
:width: 6in
:align: center

*Segment*s of the pipe cross section.
```

```{figure} ../../../../../share/examples/ex_pipe/pipe.png
:name: fig_pipe
:width: 6in
:align: center

Cross-section viewed in Gmsh.
```


---

## Analysis Result

```{csv-table} Results
:name: table_pipe_result
:header-rows: 1
:align: center

"Component", "Value", "Reference [YU2005]"
"$S_{11}$ [{{ stf0_im }}]", "$\phantom{-}1.03892 \times 10^7$", "$\phantom{-}1.03890 \times 10^7$"
"$S_{22}$ [{{ stf0_im }}]", "$\phantom{-}7.85800 \times 10^5$", "$\phantom{-}7.84299 \times 10^5$"
"$S_{33}$ [{{ stf0_im }}]", "$\phantom{-}3.31330 \times 10^5$", "$\phantom{-}3.29002 \times 10^5$"
"$S_{14}$ [{{ stf1_im }}]", "$\phantom{-}9.74568 \times 10^4$", "$\phantom{-}9.82878 \times 10^4$"
"$S_{25}$ [{{ stf1_im }}]", "$-8.02785 \times 10^3$", "$-8.18782 \times 10^3$"
"$S_{36}$ [{{ stf1_im }}]", "$-5.14533 \times 10^4$", "$-5.18541 \times 10^4$"
"$S_{44}$ [{{ stf2_im }}]", "$\phantom{-}6.89600 \times 10^5$", "$\phantom{-}6.86973 \times 10^5$"
"$S_{55}$ [{{ stf2_im }}]", "$\phantom{-}1.88230 \times 10^6$", "$\phantom{-}1.88236 \times 10^6$"
"$S_{66}$ [{{ stf2_im }}]", "$\phantom{-}5.38985 \times 10^6$", "$\phantom{-}5.38972 \times 10^6$"
```
