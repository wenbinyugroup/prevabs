(section-xml)=
# Introduction to XML


- The content of an XML file is composed of elements. Each element is marked by a pair of tags ``<tag>...</tag>``.
- The hierarchical structure of elements is important. At the same level of hierarchy, the arrangement order of elements is not important.
- The tag names are keywords, which should not be changed.
- Each element can have multiple attributes, which are ``name="value"`` pairs. The names are also keywords. The values must be surrounded by double quotes.



## XML Schema

A starter XML schema for PreVABS input files is shipped at
[prevabs.xsd](prevabs.xsd). It captures the broad structure of:

- Cross-section definition
- Material properties
- Component configurations
- Analysis settings

You can use this schema to validate your input files using any XML validator. For example, using xmllint:

```bash
xmllint --schema prevabs.xsd your_input.xml --noout
```

> [!WARNING]
> The bundled XSD currently lags behind the parser in a few places — for
> example it does not yet describe `type="airfoil"` lines, the new
> `<element_shape>`/transfinite controls in `<general>`, the unified
> recovery `<global>` block, the `transversely isotropic` material symmetry
> class, or the `method="join"` line composition. The
> [Input Guide](../guide/index.md) is the authoritative reference; the
> schema is convenient for catching common mistakes but should not be
> treated as exhaustive.
