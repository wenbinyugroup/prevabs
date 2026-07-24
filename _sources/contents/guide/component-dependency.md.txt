(input-component-dependency)=
# Components dependency

If two components are connected in the 'T' type ([](#fig-joints)), then the order of creation of the two components must be specified.
This is done by specifying the dependency.
For the case shown in the figure, the creation of the thin vertical component (purple) is dependent on the creation of the thick horizontal component (blue).
Hence, in the definition of the purple component, a `depend` attribute should be specified as shown below.

```{code-block} xml
:linenos:
:name: code_depend
:caption: Dependency.

<component name="cmp_blue" type="laminate">...</component>
<component name="cmp_purple" type="laminate" depend="cmp_blue">...</component>
```

If a component is dependent on multiple components, their names shoule all be listed, delimited by comma.
