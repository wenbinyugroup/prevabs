Box example meshed with quadrilateral elements.

## Notes

This case reuses the box-beam layout but switches the mesh to
quadrilateral elements:

```xml
<element_shape>quad</element_shape>
<element_type>linear</element_type>
```

It is useful as a compact regression case when changes affect mesh
generation or quadrilateral element handling.

---

## Input

- [`box_II.xml`](box_II.xml)
- [`materials.xml`](materials.xml)

---

## Run the example

```shell
prevabs -i box_II.xml --hm
```

---

## Output

```{figure} box_II.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `box_II.png`
