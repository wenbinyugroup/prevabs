(input-shape)=
# Geometry and shapes

The overall geometry and shape of a cross-section is defined by a series of base lines, forming the skeleton of it.
The basic geometric elements are points and lines.
The overall arrangement of the data is:
```xml
<baselines>
  <point name="p1">...</point>
  <point name="p2">...</point>
  ...
  <line name="name1" type="straight">...</line>
  <line name="name2" type="arc">...</line>
  <line name="name3" type="circle">...</line>
  <line name="name4" type="airfoil">...</line>
  ...
</baselines>
```
The root element is `<baselines>`.
It can contain arbitrary number of `<point>` and `<line>` elements.


```{note} Note
The element name `<baseline>` is still accepted as an alias of `<line>` for backward compatibility, but `<line>` is preferred in new inputs.
```


```{toctree}
:maxdepth: 2
:caption: Subtopics

shape-point
shape-line
```
