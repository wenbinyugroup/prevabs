(input-layup-stackseq)=
# Stacking sequence code

This method requires users to provide one lamina name and the stacking sequence code.

A template is shown below.

```xml
<layups>
  ...
  <layup name="..." method="stack sequence">
    <lamina>...</lamina>
    <code>...</code>
  </layup>
  ...
</layups>
```

**Specification**

- `<lamina>` - Name of the lamina used.
- `<code>` - Stacking sequency code. Explained below.

**Rules of writing the stacking sequence code**

- All fiber orientations should be put between a pair of square brackets `[]`
- Different fiber orientations are separated by slash `/`
- After the right bracket, user can add `ns` to indicate symmetry of the layup, where `n` is the number of the symmetry operations needed to generate the complete layup
- Successive laminae with the same fiber orientation can be expressed using colon like `angle:stack`, where `angle` is the fiber orientation and `stack` is the number of plies
- If a group of fiber orientations is repeated, user needs to close them in a pair of round brackets `()`

Examples

| Code | Complete sequence |
|------|------------------|
| `[0/90]2s` | "0, 90, 90, 0, 0, 90, 90, 0" |
| `[(45/-45):2/0:2]s` | "45, -45, 45, -45, 0, 0, 0, 0, -45, 45, -45, 45" |
