(input-material-elastic)=
# Elastic properties

Four types are supported.

## Isotropic

`type="isotropic"`

```xml
<materials>
  ...
  <material name="iso1" type="isotropic">
    <elastic>
      <e>...</e>
      <nu>...</nu>
    </elastic>
  </material>
  ...
</materials>
```

**Specification**

- 2 constants: `e` and `nu`.


## Transversely isotropic

`type="transversely isotropic"`

For the `transversely isotropic` material, the code internally fills the
remaining five orthotropic constants as follows when they are not provided
explicitly:

- `e3   = e2`
- `nu13 = nu12`
- `nu23 = 0.3` (arbitrary fallback — supply it explicitly for accuracy)
- `g13  = g12`
- `g23  = e3 / (2 * (1 + nu23))`

```xml
<materials>
  ...
  <material name="lam1" type="transversely isotropic">
    <elastic>
      <e1>...</e1>
      <e2>...</e2>
      <nu12>...</nu12>
      <g12>...</g12>
      <!-- e3, nu13, nu23, g13, g23 are optional; defaults are derived -->
    </elastic>
  </material>
  ...
</materials>
```


**Specification**

- 4 required constants: `e1`, `e2`, `g12`, `nu12`.
  Internally converted to an orthotropic material.
  Optional overrides for `e3`, `nu13`, `nu23`, `g13`, `g23` are accepted; defaults
  are listed above.

---


## Orthotropic

`type="orthotropic"` or `type="engineering"`

```xml
<materials>
  ...
  <material name="orth1" type="orthotropic">
    <elastic>
      <e1>...</e1>
      <e2>...</e2>
      <e3>...</e3>
      <g12>...</g12>
      <g13>...</g13>
      <g23>...</g23>
      <nu12>...</nu12>
      <nu13>...</nu13>
      <nu23>...</nu23>
    </elastic>
  </material>
  ...
</materials>
```

**Specification**

- 9 constants: `e1`, `e2`, `e3`, `g12`, `g13`, `g23`, `nu12`, `nu13`, `nu23`.

---

## Anisotropic

`type="anisotropic"`

```xml
<materials>
  ...
  <material name="aniso1" type="anisotropic">
    <elastic>
      <c11>...</c11>
      <c12>...</c12>
      <c13>...</c13>
      <c14>...</c14>
      <c15>...</c15>
      <c16>...</c16>
      <c22>...</c22>
      <c23>...</c23>
      <c24>...</c24>
      <c25>...</c25>
      <c26>...</c26>
      <c33>...</c33>
      <c34>...</c34>
      <c35>...</c35>
      <c36>...</c36>
      <c44>...</c44>
      <c45>...</c45>
      <c46>...</c46>
      <c55>...</c55>
      <c56>...</c56>
      <c66>...</c66>
    </elastic>
  </material>
  ...
</materials>
```

**Specification**

- 21 constants: `c11`, `c12`, `c13`, `c14`, `c15`, `c16`, `c22`, `c23`, `c24`, `c25`, `c26`, `c33`, `c34`, `c35`, `c36`, `c44`, `c45`, `c46`, `c55`, `c56`, `c66`. These constants are defined as:

$$
\begin{Bmatrix}
  \sigma_{11} \\ \sigma_{12} \\ \sigma_{13} \\ \sigma_{22} \\ \sigma_{23} \\ \sigma_{33}
\end{Bmatrix} =
\begin{bmatrix}
  c_{11} & c_{12} & c_{13} & c_{14} & c_{15} & c_{16} \\
  c_{12} & c_{22} & c_{23} & c_{24} & c_{25} & c_{26} \\
  c_{13} & c_{23} & c_{33} & c_{34} & c_{35} & c_{36} \\
  c_{14} & c_{24} & c_{34} & c_{44} & c_{45} & c_{46} \\
  c_{15} & c_{25} & c_{35} & c_{45} & c_{55} & c_{56} \\
  c_{16} & c_{26} & c_{36} & c_{46} & c_{56} & c_{66}
\end{bmatrix}
\begin{Bmatrix}
  \epsilon_{11} \\ 2\epsilon_{12} \\ 2\epsilon_{13} \\ \epsilon_{22} \\ 2\epsilon_{23} \\ \epsilon_{33}
\end{Bmatrix}
$$

