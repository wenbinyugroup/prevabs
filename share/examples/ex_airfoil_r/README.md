Dehomogenization workflow built on top of the MH-104 example.

## Overview

This example continues from the previous one to demonstrate the
dehomogenization analysis. It is assumed that a 1D beam analysis has
been finished, and data of global deformations and loads have been added
to the main input file correctly ({ref}`See the recover section
<section-recover>`). Suppose that the results in
{ref}`table_mh104_1dresults` are used and default values are kept for
others. All files generated from the VABS homogenization analysis are
kept in the same place as input files.

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

## Output note

`--dh` writes the global-load auxiliary file used by VABS for recovery,
`-e` executes the recovery analysis, and `-v` opens Gmsh to display the
recovered fields. To only generate the auxiliary file without executing
or plotting, drop the `-e -v` flags.

The key additional output is `mh104.sg.glb`.

---

## Input

- [`mh104.xml`](mh104.xml)
- [`mh104.dat`](mh104.dat)
- [`materials.xml`](materials.xml)
- [`mh104.sg`](mh104.sg)
- [`mh104.sg.K`](mh104.sg.K)
- [`mh104.sg.opt`](mh104.sg.opt)
- [`mh104.sg.ech`](mh104.sg.ech)

---

## Run the example

```shell
prevabs -i mh104.xml --dh -e -v
```

---

## Output

- `mh104.sg.glb`
