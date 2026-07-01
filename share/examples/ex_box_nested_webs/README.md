Box example with nested webs defined directly in the XML input.

## Overview

```{figure} box.png
:name: fig_box_nested_webs_draw
:width: 4in
:align: center

Cross section of the box beam with nested internal webs.
```

This example is a closed box cross-section built entirely from
`<baseline>` elements in `box.xml`, with no `<include>` files. The outer
contour (`bsl_all`) is a rectangle spanning $a_2=1.618$ in in width and
$a_3=1.0$ in in height. Six internal webs (`lweb1`&ndash;`lweb6`) are then
added as single `<line>` baselines, each given by a start point and a
departure angle.

The webs are nested rather than all attached to the outer contour directly:
`web1` depends only on `main`, but `web2` depends on `main,web1`, `web3` on
`main,web1,web2`, and so on through `web6`, which depends on all five
preceding webs. Each web line is trimmed against every component it depends
on, so this chain of `depend` lists builds up the stepped, nested set of
internal compartments visible on the right side of {ref}`fig_box_nested_webs_draw`
(a single full-height web at $x=-0.191$, followed by progressively smaller
nested pockets).

Every component uses the same material and layup, listed in
{ref}`table_box_nested_webs_materials` and {ref}`table_box_nested_webs_layups`.

```{csv-table} Material properties
:name: table_box_nested_webs_materials
:header-rows: 2
:align: center

"Name", "Density", "{{ e1 }}", "{{ e2e3 }}", "{{ g12g13 }}", "{{ nu12nu13 }}"
, "{{ den_im }}", "{{ mod_im }}", "{{ mod_im }}", "{{ mod_im }}",
"mtr_lamina", 10, 4, 5, 6, 0.40
```

```{csv-table} Layups
:name: table_box_nested_webs_layups
:header-rows: 2
:align: center

"Name", "Layer", "Lamina", "Ply thickness", "Orientation"
, , , "{{ len_im }}", $\circ$
"layup1", 1, "la_mtr_lamina_0.01", 0.02, 0
"layup1", 2, "la_mtr_lamina_0.01", 0.02, 90
```

The global mesh size is $0.01$ in, with quadratic (default) triangular
elements.

---

## Input

- [`box.xml`](box.xml)

---

## Run the example

```shell
prevabs -i box.xml --hm
```

---

## Output

```{figure} box.png
:width: 4in
:align: center

Cross section viewed in gmsh.
```

- `box.png`
