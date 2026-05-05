Airfoil example with a D-spar internal structure.

## Notes

This case builds an airfoil cross-section with a D-spar layout. It is a
useful comparison point against the C-spar example when checking
baseline partitioning and mesh quality.

---

## Input

- [`cs_main.xml`](cs_main.xml)
- [`sc1095.dat`](sc1095.dat)
- [`material_database_us_ft.xml`](material_database_us_ft.xml)

---

## Run the example

```shell
prevabs -i cs_main.xml --hm
```

---

## Output

```{figure} cs_main.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `cs_main.png`
