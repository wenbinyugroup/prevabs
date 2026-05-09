(input-lamina)=
# Lamina

Each lamina element has a name, and contains a material name and a value for thickness.
The input syntax is:

```xml
<materials>
  ...
  <lamina name="lamina1">
    <material> orth1 </material>
    <thickness>...</thickness>
  </lamina>
  ...
</materials>
```

**Specification**

- `<lamina>` - Root element for the definition of each lamina.

   - `name` - Name of the lamina.

- `<material>` - Name of the material of the lamina.
- `<thickness>` - Thickness of the lamina.

