(input-shape-line-airfoil)=
# Airfoil

`type="airfoil"` consumes a standard airfoil coordinate file and produces a closed polyline that starts and ends at the trailing edge $(1, 0)$ and passes through the leading edge $(0, 0)$.
Both Selig (single block) and Lednicer (upper / blank line / lower) formats are supported.

```xml
<baselines>
    <line name="ln_af" type="airfoil">
        <points data="file" format="1" header="1">mh104.dat</points>
        <flip>1</flip>
        <reverse>1</reverse>
    </line>
</baselines>
```

**Specification**

- `<points>`: Path to the airfoil data file (relative to the main XML file).
  - Attributes
    - `data`: Source of the data. Currently only `file` is supported.
    - `format`: Airfoil file format. `1` / `selig` / `s*` for Selig, `2` / `lednicer` / `l*` for Lednicer. Default is `2`.
    - `header`: Number of header rows in the data file to skip. Default is `0`.
    - `direction`: `1`/`ccw`/`forward` or `-1`/`cw`/`reverse`. Default is `1`.
- `<flip>`: Optional. Mirror the airfoil after construction. The element value
  is a boolean (`1`/`0`/`true`/`false`); attributes `axis` (`2`/`x2`/`y` or
  `3`/`x3`/`z`, default `2`) and `loc` (mirror location, default `0.5`)
  control the reflection.
- `<reverse>`: Optional. If true, reverse the order of vertices on the line
  after all other transforms.

Two canonical leading-edge and trailing-edge vertices are always created and named `<linename>_le` and `<linename>_te`.
Interior vertices are named `<linename>_p1`, `<linename>_p2`, ….

