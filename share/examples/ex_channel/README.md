Cross-ply channel example with a generated Gmsh preview.

## Overview

```{figure} ex_channel_0.png
:name: fig_channel0
:width: 3in
:align: center

Cross section of the pipe [CHEN2010].
```

This example has a cross-section of a highly heterogeneous channel.
This cross-section geometry can be defined as shown in
{numref}`fig_channel0` [CHEN2010].
The isotropic material properties are given in
{ref}`table_channel_materials`.
The layup is defined having a single layer with the thickness
0.001524 m.

```{csv-table} Material properties
:name: table_channel_materials
:header-rows: 2
:align: center

"Name", "Type", "Density", "{{ e }}", "{{ nu }}"
, , "{{ den_si }}", "{{ mod_si_g }}",
"mtr1", "isotropic", 1068.69, 206.843, 0.49
```

```{csv-table} Layups
:name: table_channel_layups
:header-rows: 2
:align: center

"Name", "Layer", "Material", "Ply thickness", "Orientation", "Number of plies"
, , , "{{ len_si }}", "$\circ$",
"layup1", 1, "mtr1", 0.001524, 0, 1
```

---

## Input

- [`channel.xml`](channel.xml)

---

## Run the example

```shell
prevabs -i channel.xml --hm
```

---

## Output

```{figure} channel.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `channel.png`

---

## Analysis Result

```{csv-table} Results
:name: table_channel_result
:align: center

"$\phantom{-}1.906\times 10^7$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$-4.779\times 10^4$", "$-1.325\times 10^5$"
"$0.0$", "$\phantom{-}2.804\times 10^6$", "$\phantom{-}2.417\times 10^5$", "$\phantom{-}2.128\times 10^4$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$0.0$", "$\phantom{-}2.417\times 10^5$", "$\phantom{-}2.146\times 10^6$", "$-7.663\times 10^3$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$0.0$", "$\phantom{-}2.128\times 10^4$", "$-7.663\times 10^3$", "$\phantom{-}2.091\times 10^2$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$-4.779\times 10^4$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$\phantom{-}2.011\times 10^3$", "$\phantom{-}9.104\times 10^2$"
"$-1.325\times 10^5$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$\phantom{-}9.104\times 10^2$", "$\phantom{-}1.946\times 10^3$"
```

```{csv-table} Results from reference [CHEN2010]
:name: table_channel_result_ref
:align: center

"$\phantom{-}1.903\times 10^7$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$-4.778\times 10^4$", "$-1.325\times 10^5$"
"$0.0$", "$\phantom{-}2.791\times 10^6$", "$\phantom{-}2.364\times 10^5$", "$\phantom{-}2.122\times 10^4$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$0.0$", "$\phantom{-}2.364\times 10^5$", "$\phantom{-}2.137\times 10^6$", "$-7.679\times 10^3$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$0.0$", "$\phantom{-}2.122\times 10^4$", "$-7.679\times 10^3$", "$\phantom{-}2.086\times 10^2$", "$\phantom{-}0.0$", "$\phantom{-}0.0$"
"$-4.778\times 10^4$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$\phantom{-}2.010\times 10^3$", "$\phantom{-}9.102\times 10^2$"
"$-1.325\times 10^5$", "$0.0$", "$\phantom{-}0.0$", "$\phantom{-}0.0$", "$\phantom{-}9.102\times 10^2$", "$\phantom{-}1.944\times 10^3$"
```
