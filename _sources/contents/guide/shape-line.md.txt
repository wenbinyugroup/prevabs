(input-shape-line)=
# Base lines


PreVABS can handle four types of base lines: straight, arc, circle and airfoil:
![fig-baselinetypes](../../figures/baselinetypes.png)

Some types have several ways to define the base line.
In PreVABS, all curved base lines are in the end converted into a polyline.
User can define polylines directly for spline, arc and circle.
Or, for arc or circle, user can use simple rules to draw the shape first and then PreVABS will discretize it.
The `airfoil` type reads a standard airfoil coordinate file (Selig or Lednicer format) and creates a closed polyline starting and ending at the trailing edge.

Each `<line>` element is a definition of a base line.
Each one has a unique `name` and an optional `type`, which can be `straight` (default), `arc`, `circle`, or `airfoil`.
Inside the `line` element, the `straight` and `arc` types have several different ways of definition, and thus the arrangements of data are different, which will be explained in details below.


**Specification**

- `<line>` (or `<baseline>`): Definition of a base line (explained below).
  - Attributes
    - `name`: Name of the base line. Required.
    - `type`: Type of the base line. Optional. Choose one from `straight` (default), `arc`, `circle`, `airfoil`.
    - `method`: `direct` (default) or `join`. With `method="join"`, the
      element value lists names of existing lines (one per child element) to
      be concatenated into a new line.

```{toctree}
:maxdepth: 2
:caption: Subtopics

shape-line-polyline
shape-line-arc
shape-line-airfoil
```
