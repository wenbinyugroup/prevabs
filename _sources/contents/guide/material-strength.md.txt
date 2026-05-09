(input-material-strength)=
# Strength properties

Inputs for strength properties for all types of materials have the same syntax.
For each material, users can input at least one or at most nine properties.
The overall syntax for each material is:

```xml
<material name="..." type="...">
  <failure_criterion>...</failure_criterion>
  <strength>
    <xt>...</xt>
    <yt>...</yt>
    <zt>...</zt>
    <xc>...</xc>
    <yc>...</yc>
    <zc>...</zc>
    <r>...</r>
    <t>...</t>
    <s>...</s>
  </strength>
</material>
```

Although all properties will be stored internally, only some of them will be used in analysis depending on the failure criterion.

**Specification**

- `<xt>` - Tensile strength in $x_1$ direction. Alternative tags: `<t1>` or `<x>`. (Required)
- `<yt>` - Tensile strength in $x_2$ direction. Alternative tags: `<t2>` or `<y>`. (Optional. Default = `<xt>`.)
- `<zt>` - Tensile strength in $x_3$ direction. Alternative tags: `<t3>` or `<z>`. (Optional. Default = `<yt>`.)
- `<xc>` - Compressive strength in $x_1$ direction. Alternative tag: `<c1>`. (Optional. Default = `<xt>`.)
- `<yc>` - Compressive strength in $x_2$ direction. Alternative tag: `<c2>`. (Optional. Default = `<yt>`.)
- `<zc>` - Compressive strength in $x_3$ direction. Alternative tag: `<c3>`. (Optional. Default = `<zt>`.)
- `<s>` - Shear strength in $x_1$-$x_2$ plane. Alternative tag: `<s12>`. (Required for specific failure criteria.)
- `<t>` - Shear strength in $x_1$-$x_3$ plane. Alternative tag: `<s13>`. (Optional. Default = `<s>`)
- `<r>` - Shear strength in $x_2$-$x_3$ plane. Alternative tag: `<s23>`. (Optional. Default = (`<yt>` + `<yc>`) / 4)

