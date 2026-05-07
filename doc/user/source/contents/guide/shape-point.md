(shape-point)=
# Base points

Base points are used to draw base lines, which form the skeleton of a cross section.
They can be either actually on the base lines, or not, as reference points, for example the center of a circle.
Points that are directly referred to in the definitions of base lines are called key points, such as starting and ending points of a line or an arc, or the center of a circle.
The rest are normal points.
The coordinates provided in the input file are defined in the basic frame **z** and then transformed into the cross-sectional frame **x**, through processes like translating, scaling and rotating.
If none of those operations are needed, then those data also define the position of each point in the frame **x**.

Users can define points using the sub-element `<point>` one-by-one inside `<baselines>`, or list all points in a separate file and include it.

## Points defined in the main XML file

Using the XML format, points can be defined in the following ways:

1. Specify the complete coordinates with two numbers separated by spaces.
2. Confine the point on a line and specify the horizontal coordinate ($z_2$ or $x_2$):

   `<point name="M" on="L" by="x2">3</point>`

   When the host line is an `airfoil` line that has both an upper and a lower
   surface at the requested $x_2$ location, use the `which` attribute to pick
   which surface to land on:

   `<point name="M_top"    on="L" by="x2" which="top">0.3</point>`
   `<point name="M_bottom" on="L" by="x2" which="bottom">0.3</point>`

3. Constrain a point as the midpoint of two existing points:

   `<point name="M" constraint="middle">p1 p2</point>`

![fig-point_on_line](../../figures/point_on_line.png)


**Specification**

- `<point>`: Coordinates of the point. For the first method, two numbers are needed and separated by blanks. For the second method, only one number is needed.
  - Attributes
    - `name`: Name of the point. Required.
    - `on`: Name of the line confining the point. Optional.
    - `by`: Axis along which the coordinate is specified. Required if the point is defined on a line. Currently `x2` (alias `y`) is the only implemented axis.
    - `which`: When `on` references an airfoil-type line, choose `top` or `bottom` to disambiguate the upper / lower surface. Default is `top`.
    - `constraint`: `middle` to place the point at the midpoint of two named reference points listed as the element value. Default is `none`.



## Points defined in a data file

The file storing these data is a plain text file, with a file extension `.dat`.
This block of data has three columns for the name and coordinate in the cross-sectional plane.

```
label_1  z2  z3
label_2  z2  z3
label_3  z2  z3
...
```

To include the point data file from inside `<baselines>`:

```xml
<baselines>
  <basepoints>
    <include>point_list_file_name</include>
  </basepoints>
  ...
</baselines>
```

The optional `scale` attribute on `<include>` multiplies all coordinates by a constant when the file is read:

```xml
<include scale="0.5">point_list_file_name</include>
```

```{note} Note
`<basepoints>` is retained for backward compatibility but is deprecated.
Prefer placing `<point>` elements directly inside `<baselines>`, or use the built-in `<line type="airfoil">` to consume a standard airfoil data file (see below).
```

**Specification**

- Three columns are separated by spaces.
- `label` can be the combination of any letters, numbers and underscores "_".

```{note} Note
Normal points' names can be less meaningful, even identical.
```


```{note} Note
If a base line is defined using the range method (explained below), e.g. 'a:b', then all points from 'a' to 'b' will be used.
In this case, the order of points is important.
Otherwise, points can be arranged arbitrarily.
```

