(example-airfoil-recover)=
# Airfoil (Recover)

## Overview

This example continues from the previous one to demonstrate the dehomogenization analysis.
It is assumed that a 1D beam analysis has been finished, and data of global deformations and loads have been added to the main input file correctly ({ref}`See the recover section <section-recover>`).
Suppose that the results in {ref}`table_mh104_1dresults` are used and default values are kept for others.
All files generated from the VABS homogenization analysis are kept in the same place as input files.

```{csv-table} Sectional forces and moments
:name: table_mh104_1dresults
:header-rows: 1
:align: center

Quantity, Value
"$F_1$, N", 1
"$F_2$, N", 2
"$F_3$, N", 3
"$M_1$, Nm", 4
"$M_2$, Nm", 5
"$M_3$, Nm", 6
```

---

## PreVABS Input and Output

Input

- [`mh104.xml`](../../../../../share/examples/ex_airfoil_r/mh104.xml)
- [`mh104.dat`](../../../../../share/examples/ex_airfoil_r/mh104.dat)
- [`materials.xml`](../../../../../share/examples/ex_airfoil_r/materials.xml)


Running

```
prevabs -i mh104.xml --dh
```

Output

- `mh104.sg.glb`


---

## Analysis Result

```{figure} /figures/examplemh104r.png
:name: fig_mh104r
:width: 6in
:align: center

Contour plot of recovered nodal stress SN11.
```
