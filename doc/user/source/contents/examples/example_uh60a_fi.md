(section-uh60a-fi)=
# UH-60A (Initial Failure Analysis)

## Run the example

```text
prevabs -i uh60a.xml --fi -e -v
```

## Problem description

```{csv-table} Load case
:name: table_uh60a_loadcases
:header-rows: 1
:align: center

"$F_1$ [lb]", "$F_2$ [lb]", "$F_3$ [lb]", "$M_1$ [lb.in]", "$M_2$ [lb.in]", "$M_3$ [lb.in]"
46777, 250.762, 414.939, -306.1, -16131.9, 2727.67
```

## Result

With this load case, the structure fails. The minimum strength ratio is
0.814. The element that has this value is 18252.

```{figure} /figures/ex-uh60a_fi_fail_2.png
:name: fig_uh60a_fi_fail
:width: 6in
:align: center

Contour plot of strength ratio.
```

```{figure} /figures/ex-uh60a_fi_fail_2_zoomin.png
:name: fig_uh60a_fi_fail_zoomin
:width: 6in
:align: center

Contour plot of strength ratio (zoom in).
```
