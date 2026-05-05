Airfoil example with a C-spar internal structure.

## Notes

This case builds an airfoil cross-section with a C-spar layout. It is a
compact example for validating spar topology and the interaction between
the airfoil outline and internal structural members.

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
