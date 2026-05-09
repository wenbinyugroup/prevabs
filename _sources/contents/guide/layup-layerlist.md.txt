(input-layup-layerlist)=
# Explicit list

This method requires user to write down the lamina name, fiber orientation
and number of successive laminas with the same fiber orientation, layer
by layer. Alternatively, a layer can also be defined by the name of a sublayup.
A sublayup must appear before the layup where it is referenced.

A template for one layer is:

```xml
<layups>
  ...
  <layup name="...">
    <layer lamina="lamina_name"> angle:stack </layer>
    <layer lamina="lamina_name"> angle:stack </layer>
    <layer lamina="lamina_name"> angle:stack </layer>
    <layer layup="sublayup_name"/>
    ...
  </layup>
  ...
</layups>
```

**Specification**

- `<layer>` - Material orientation and number of plies separated by a colon 'angle:stack'. Default values are 0 for 'angle' and 1 for 'stack'. If there is only one number presented in this element, then it is read in as 'angle', not 'stack', which is 1 by default.

  - `lamina` - Optional. Name of the lamina used in the current layer.
  - `layup` - Optional. Name of the sublayup used in the current layer. One and only one of `lamina` and `layup` should be used. 'angle' and 'stack' are not needed when `layup` is used.
