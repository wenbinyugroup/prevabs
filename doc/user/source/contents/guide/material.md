(input-material)=
# Material

PreVABS uses the keyword `material` for the physical properties attached to any materials, while `lamina` for material plus thickness, which in a sense is fixed by manufacturers.
This can be thought as the basic commercially available "material", such as a composite preprag.
A layer is a stack of laminae with the same fiber orientation.
The thickness of a layer can only be a multiplier of the lamina thickness.
Layup is several layers stacked together in a specific order.
This relationship is illustrated as:
![fig-layup](../../figures/layup.png)


Both materials and laminae are stored in one XML file (or directly in the main cross-section file under a `<materials>` block).
Each material must have a name and type. Under each `<material>` element, there are a `<density>` element and an `<elastic>` element.
If failure analysis is wanted, users need to provide extra data including `<strength>` and `<failure_criterion>`.

A template of this file is shown below:

```xml
<materials>
  ...
  <material name="..." type="...">
    <density>...</density>
    <elastic>...</elastic>
    <strength>...</strength>
    <failure_criterion>...</failure_criterion>
  </material>
  ...
</materials>
```

**Specification**

- `<material>` - Root element for each material.

  - `name` - Name of the material.
  - `type` - Material symmetry class. Choose one from `isotropic`,
    `transversely isotropic`, `orthotropic` (alias `engineering`), and
    `anisotropic`.
  - `<density>` - Density of the material. Default is 1.0.
  - `<elastic>` - Elastic properties of the material. Specifications are different for different types.
  - `<strength>` - Strength properties of the material. Specifications are different for different types and different failure criterion.
  - `<failure_criterion>` - Failure criterion of the material. Options are different for different types.

```{note} Note
`type="lamina"` is no longer accepted as a material symmetry type.
Use `type="transversely isotropic"` for a material with transverse isotropy (the four engineering constants $E_1$, $E_2$, $\nu_{12}$, $G_{12}$ are required, the rest are derived as described below).
The `<lamina>` element is now used **only** to bind a material to a thickness, see the [Lamina](#lamina) section.
```


```{toctree}
:maxdepth: 2
:caption: Subtopics

material-elastic
material-strength
material-failure-criterion
lamina
```
