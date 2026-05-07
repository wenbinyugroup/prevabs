(input-other-settings)=
# Other input settings

There are three groups of these settings.

## Included files

This part contains file names for base lines, materials (local) and layups as shown in [](#code-include).
The `<material>` sub-element is optional.
If this is included, PreVABS will read extra materials from this local file, besides the global material database (`MaterialDB.xml`).
If there are materials and laminae with the same names, data in the local material file will overwrite those in the global database.

- `path` in the `<include>` element is the relative path to the main input file;
- File extensions `.xml` can be omitted.

```{code-block} xml
:linenos:
:name: code-include
:caption: Input syntax for the included files.

<include>
  <baseline> path/baseline_file_name </baseline>
  <material> path/material_file_name </material>
  <layup> path/layup_file_name </layup>
</include>
```

**Specification**

- `<baseline>` - Name of the included base line file.
- `<material>` - Name of the included local material file.
- `<layup>` - Name of the included layup file. Multiple `<layup>` elements can be listed to merge several layup files.

---

## Analysis options

The second part contains settings for the analysis in VABS.
`<model>` can be 0 for classical beam model, or 1 for refined (Timoshenko) model.

```{code-block} xml
:linenos:
:name: code-analysis
:caption: A template for the analysis options in a main input file.

<analysis>
  <model> 1 </model>
</analysis>
```

**Specification**

- `<model_dim>` - Dimension of the structural model. `1` (default) for beam.
- `<model>` - Beam model. Choose one from `0` (classical) and `1` (refined / Timoshenko).
- `<damping>`, `<thermal>`, `<trapeze>`, `<vlasov>` - Optional integer flags passed through to the solver. Default `0`.
- `<initial_twist>`, `<initial_curvature_2>`, `<initial_curvature_3>` - Optional initial twist / curvature components. Setting any of them to a non-zero value enables the curvature flag.
- `<oblique_y1>`, `<oblique_y2>` - Optional direction-cosine components
  enabling the oblique flag when they differ from `(1, 0)`.
- `<physics>` - Optional physics setting forwarded to the analysis backend.


---

## Global shape and mesh settings

The last part contains optional global geometry and meshing settings, which are all stored in a `<general>` sub-element.

User can set the global transformations of the cross section.
The three transformation operations have been discussed in [](#coordinate-systems).

- The order of transformation operation is: translating, scaling, and rotating;
- All operations are performed on the cross section, not the frame;
- The scaling operation will only scale the shape (base lines), and have
  no effect on the thicknesses of laminae;
- The rotating angle starts from the positive half of the $x_2$ axis,
  and increases positively counter-clockwise (right-hand rule).

There are two basic meshing options, the global mesh size `<mesh_size>` and the polynomial order `<element_type>`.
In addition, the mesh element shape can be switched between triangles and quadrilaterals via `<element_shape>`, and a number of optional Gmsh transfinite/recombination controls are available.

- If not setting, the global meshing size will be the minimum layer thickness by default;
- Two options for the element type are linear and quadratic.
- Two options for the element shape are triangle and quadrilateral.

```{code-block} xml
:linenos:
:name: code_general
:caption: A template for the global shape and mesh settings in a main input file.

<general>
  <translate> e2  e3 </translate>
  <scale> scaling_factor </scale>
  <rotate> angle </rotate>
  <mesh_size> a </mesh_size>
  <element_type> quadratic </element_type>
  <element_shape> 3 </element_shape>
  <tolerance> 1e-12 </tolerance>
</general>
```

**Specification**

- `<translate>` - Horizontal and vertical translation of the cross-section. The origin will be moved to (-e2, -e3).
- `<scale>` - Scaling factor of the cross-section.
- `<rotate>` - Rotation angle of the cross-section.
- `<mesh_size>` - Global mesh size.
- `<element_type>` - Order of elements. `linear` (or `1`) or `quadratic` (or `2`, default).
- `<element_shape>` - Shape of mesh elements. `3`, `tri`, or `triangle` (default) for triangles; `4`, `quad`, or `quadrilateral` for quadrilaterals.
- `<tolerance>` - Tolerance used in geometric computation. Optional. Default value is 1e-12.
- `<omega>` - Optional global scaling factor passed to the solver. Default `1.0`.

When `<element_shape>` selects quadrilaterals, the following Gmsh-driven controls become available:

- `<transfinite_auto>` - `1`/`true` to enable Gmsh's automatic transfinite meshing of structured regions.
- `<transfinite_corner_angle>` - Angle threshold (radians) used by the transfinite-auto algorithm to decide which corners to fix.
- `<transfinite_recombine>` - `1`/`true` to recombine triangles into quads after transfinite meshing.
- `<recombine>` - `1`/`true` to enable Gmsh's general recombine pass.
- `<recombine_angle>` - Angle threshold (degrees) used by the recombine algorithm.

Interface tracking between layers is also configurable here:

- `<track_interface>` - Integer flag to write per-interface marker output.
- `<interface_theta1_diff_threshold>`, `<interface_theta3_diff_threshold>` - Angle thresholds (degrees) used to decide whether two adjacent layers share an interface for tracking.
