MH-104 airfoil example with multiple internal cells.

## Notes

This example extends the MH-104 airfoil case to multiple internal
cells. It is mainly useful for regression coverage of more complex
topology and segmentation patterns inside the same outer airfoil shape.

---

## Input

- [`mh104_9webs.xml`](mh104_9webs.xml)
- [`mh104.dat`](mh104.dat)
- [`materials.xml`](materials.xml)

---

## Run the example

```shell
prevabs -i mh104_9webs.xml --hm
```

---

## Output

```{figure} mh104_9webs.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `mh104_9webs.png`
