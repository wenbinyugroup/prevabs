Failure-index example for the UH-60A rotor-blade section.

## Problem description

```{csv-table} Load case
:name: table_uh60a_loadcases
:header-rows: 1
:align: center

"$F_1$ [lb]", "$F_2$ [lb]", "$F_3$ [lb]", "$M_1$ [lb.in]", "$M_2$ [lb.in]", "$M_3$ [lb.in]"
467770, 250.762, 414.939, -306.1, -1613.9, 2727.67
```

---

## Input

- [`uh60a.xml`](uh60a.xml)
- [`sc1095.dat`](sc1095.dat)
- [`material_database.xml`](material_database.xml)
- [`uh60a.sg`](uh60a.sg)
- [`uh60a.sg.K`](uh60a.sg.K)
- [`uh60a.sg.opt`](uh60a.sg.opt)
- [`uh60a.sg.ech`](uh60a.sg.ech)

---

## Run the example

```shell
prevabs -i uh60a.xml --fi -e -v
```

---

## Output

```{figure} uh60a_fi.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `uh60a.sg.glb`
- `uh60a.sg.fi`
- `uh60a_local_case_1.msh`
- `uh60a_local.opt`
