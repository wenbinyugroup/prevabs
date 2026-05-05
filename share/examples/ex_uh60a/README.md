UH-60A rotor-blade section example for homogenization.

## Overview

This case models a UH-60A rotor-blade cross section using the SC1095
airfoil coordinates and an external material database. It serves as the
baseline homogenization example for the related failure-analysis case.

The XML input combines:

- included airfoil coordinates from `sc1095.dat`
- included material data from `uh60a_material.xml`
- multiple shell components for spar, leading edge, and trailing edge
- fill components for the non-structural mass and foam regions

---

## Input

- [`uh60a_section.xml`](uh60a_section.xml)
- [`sc1095.dat`](sc1095.dat)
- [`uh60a_material.xml`](uh60a_material.xml)

---

## Run the example

```shell
prevabs -i uh60a_section.xml --hm
```

---

## Output

```{figure} uh60a_section.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `uh60a_section.png`
