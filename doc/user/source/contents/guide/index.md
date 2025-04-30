(section-prevabs_guide)=
# Input Guide

In PreVABS, a cross section is defined through two aspects: components and global configuration:
![fig-csfiles1](../../figures/chart1.png)

Components are built from geometry and materials.
The geometry aspect comprises definitions of base points and base lines.
The material aspect includes material properties, lamina thicknesses, layup stacking sequences, etc.
The global configuration contains the files included, transformation, meshing options, and analysis settings.


To create the cross section, at least four input files are needed.
Definitions of the geometry, materials and layups are stored in three files separately.
A top level cross section file stores the definitions of components and global configurations.
In some cases, a separate file storing base points is needed (such as an airfoil).
Except the base points file, which has a file extension .dat, all other files use an XML format.

A top level cross section file stores all information that will be discussed in the following sections.
The input syntax for this main file is:
```xml
<cross_section name="" format="">
  <include>...</include>
  <analysis>...</analysis>
  <general>...</general>
  <baselines>...</baselines>
  <layups>...</layups>
  <component>...</component>
  <component>...</component>
  ...
</cross_section>
```

Data of geometry and layup can be arranged in two ways, controlled by the attribute `format`:

* If omitted or set to 0, then the code will look for definitions of shapes and layups in separated files, specified in the element `<include>`.
* If set to 1, then the code will look for the definitions in the current main input file.

**Specification**

- `<cross_section>`
  - `name` - Name of the cross-section.
  - `format` - Format of the input file. See explaination above.

- `<include>` - File names of separately stored data.
- `<analysis>` - Configurations of cross-sectional analysis.
- `<general>` - Overall settings of the cross-section.
- `<baselines>` - Definitions of geometry and shape.
- `<layups>` - Definitions of layups.
- `<component>` - Definitions of cross-sectional components.
- `<recover>` - Inputs for recovery.




```{toctree}
:maxdepth: 2
:caption: Subtopics

pre_coordinate.md
pre_shape.md
pre_material.md
pre_component.md
pre_recover.md
pre_overall.md
```

