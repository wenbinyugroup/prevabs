(input-shape-line-airfoil)=
# Airfoil

`type="airfoil"` consumes a standard airfoil coordinate file and produces a
closed polyline that starts and ends at the trailing edge and passes through
the leading edge. Both Selig (single block) and Lednicer (upper / blank line /
lower) formats are supported.

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
- `<leading_edge>`: Optional. Override the airfoil leading-edge point used for
  surface splitting.
  - Attribute
    - `name`: Optional name for the leading-edge vertex. When omitted, the
      default name is `<linename>_le`.
  - Value
    - Optional `x y` coordinates in the raw airfoil-data coordinate system.
      When omitted, PreVABS keeps the auto-detected geometry and only renames
      the vertex.
- `<trailing_edge>`: Optional. Override the airfoil trailing-edge point used
  for surface splitting.
  - Attribute
    - `name`: Optional name for the trailing-edge vertex. When omitted, the
      default name is `<linename>_te`.
  - Value
    - Optional `x y` coordinates in the raw airfoil-data coordinate system.
- `<normalize>`: Optional boolean. If true, normalize the raw airfoil data
  along the x-direction before edge detection and splitting.
  - The transform is
    - `x' = (x - x_min) / (x_max - x_min)`
    - `y' = y / (x_max - x_min)`
  - User-specified `<leading_edge>` / `<trailing_edge>` coordinates are
    transformed by the same rule automatically.
- `<flip>`: Optional. Mirror the airfoil after construction. The element value
  is a boolean (`1`/`0`/`true`/`false`); attributes `axis` (`2`/`x2`/`y` or
  `3`/`x3`/`z`, default `2`) and `loc` (mirror location, default `0.5`)
  control the reflection.
- `<reverse>`: Optional. If true, reverse the order of vertices on the line
  after all other transforms.

If `name` is not provided, the leading-edge and trailing-edge vertices are
named `<linename>_le` and `<linename>_te`.
Interior vertices are named `<linename>_p1`, `<linename>_p2`, ….

## Non-standard Airfoil Data

Some airfoil datasets do not place the leading edge at `(0, 0)` or the
trailing edge at `(1, 0)`. For example, the manual example
`test/manual/airfoil/ua79sff.dat` has its main-element leading edge near
`(0.65, -0.0184)` in the original file coordinates.

For these cases you can either:

- explicitly provide `<leading_edge>` / `<trailing_edge>` coordinates, or
- enable `<normalize>` so the raw x-range is mapped to `[0, 1]` before
  splitting

Example:

```xml
<line name="ln_af" type="airfoil">
    <points data="file" format="1" header="1">ua79sff.dat</points>
    <leading_edge name="ple">0.650000 -0.018400</leading_edge>
    <trailing_edge>1.000000 0.000000</trailing_edge>
</line>
```

