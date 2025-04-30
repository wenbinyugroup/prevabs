(section-recover)=
# Specifications for recovery

## VABS

Once the 1D beam analysis is finished, those results can be used to recover local 3D strains and stresses at every point of the cross section.
As stated in the VABS manual, the following data are required to carry out the recover analysis at a selected location along the beam:

- 1D beam displacements;
- rotations in the form of a direction cosine matrix;
- sectional forces and moments;
- distributed forces and moments, and their first three derivatives with
  respect to $x_1$.

> [!NOTE]
> The current version of PreVABS only performs recovery for Euler-Bernoulli model and Timoshenko model.
> If you want to perform recovery for other models such as the Vlasov model, please refer to the VABS manual to modify the input file yourself.


These data are included in a `<global>` element, added into the `<cross_section>` element in the main input file.
The nine entries in the direction cosine matrix are flattened into a single array in the
`<rotations>` element.
This matrix is defined in {eq}`eq_direction_cosine`.
Note that displacements and rotations are needed only for recovering 3D displacements.
Numbers in this template are defualt values.
Once this file is updated correctly, the VABS recover analysis can be carried out as shown in Section: {ref}`section-command-option`.

$$
\mathbf{B}_i = C_{i1} \mathbf{b}_1 + C_{i2} \mathbf{b}_2 + C_{i3} \mathbf{b}_3 \quad \text{with} \quad i = 1, 2, 3
$$
where $\mathbf{B}_1$, $\mathbf{B}_2$, and $\mathbf{B}_3$ are the base vectors of the deformed triad and $\mathbf{b}_1$, $\mathbf{b}_2$, and $\mathbf{b}_3$ are the base vectors of the undeformed triad.

A template for this part of data is:
```xml
<cross_section name="cs1">
  ...
  <dehomo> 1 </dehomo>
  <global>
    <displacements> 0 0 0 </displacements>
    <rotations> 1 0 0 0 1 0 0 0 1 </rotations>
    <loads> 0 0 0 0 0 0 </loads>
    <distributed>
      <forces> 0 0 0 </forces>
      <forces_d1> 0 0 0 </forces_d1>
      <forces_d2> 0 0 0 </forces_d2>
      <forces_d3> 0 0 0 </forces_d3>
      <moments> 0 0 0 </moments>
      <moments_d1> 0 0 0 </moments_d1>
      <moments_d2> 0 0 0 </moments_d2>
      <moments_d3> 0 0 0 </moments_d3>
    </distributed>
  </global>
</cross_section>
```

**Specification**

- `<global>` - Global analysis results. Required.
- `<displacements>` - Three components ($u_1$, $u_2$, $u_3$) of the global displacements. Optional. Default values are {0, 0, 0}.
- `<rotations>` - Nine components ($c_{11}$, $c_{12}$, $c_{13}$, $c_{21}$, $c_{22}$, $c_{23}$, $c_{31}$, $c_{32}$, $c_{33}$) of the global rotations (direction cosine matrix). Optional. Default values are {1, 0, 0, 0, 1, 0, 0, 0, 1}.
- `<loads>` - The sectional loading components. For the Euler-Bernoulli model, four numbers ($F_1$, $M_1$, $M_2$, $M_3$) are needed. For the Timoshenko model, six numbers ($F_1$, $F_2$, $F_3$, $M_1$, $M_2$, $M_3$) are needed. Optional. Default values are zero loads.
- `<distributed>` - Distributed sectional loads per unit span (Timoshenko model only). Optional. Default values are zero distributed loads.

  - `<forces>` - Distributed sectional forces ($f_1$, $f_2$, $f_3$).
  - `<forces_d1>` - First derivative of distributed sectional forces ($f'_1$, $f'_2$, $f'_3$).
  - `<forces_d2>` - Second derivative of distributed sectional forces ($f''_1$, $f''_2$, $f''_3$).
  - `<forces_d3>` - Third derivative of distributed sectional forces ($f'''_1$, $f'''_2$, $f'''_3$).
  - `<moments>` - Distributed sectional moments ($m_1$, $m_2$, $m_3$).
  - `<moments_d1>` - First derivative of distributed sectional moments ($m'_1$, $m'_2$, $m'_3$).
  - `<moments_d2>` - Second derivative of distributed sectional moments ($m''_1$, $m''_2$, $m''_3$).
  - `<moments_d3>` - Third derivative of distributed sectional moments ($m'''_1$, $m'''_2$, $m'''_3$).

- `<dehomo>` - Order of theory for dehomogenization, 1 for nonlinear beam theory and 2 for linear beam theory. Optional. Default is 2.

## SwiftComp

```xml
<global measure="stress">
  <displacements> 0 0 0 </displacements>
  <rotations> 1 0 0 0 1 0 0 0 1 </rotations>
  <loads> 0 0 0 0 0 0 </loads>
</global>
```

**Specification**

- `<global>` - Global analysis results. Required.

  - `measure` - Type of the sectional loads, `stress` for generalized stresses and `strain` for generalized strains. Required.

- `<displacements>` - Three components ($u_1$, $u_2$, $u_3$) of the global displacements. Optional. Default values are {0, 0, 0}.
- `<rotations>` - Nine components ($c_{11}$, $c_{12}$, $c_{13}$, $c_{21}$, $c_{22}$, $c_{23}$, $c_{31}$, $c_{32}$, $c_{33}$) of the global rotations (direction cosine matrix). Optional. Default values are {1, 0, 0, 0, 1, 0, 0, 0, 1}.
- `<loads>` - The sectional loading components. For the Euler-Bernoulli model, four numbers ($F_1$, $M_1$, $M_2$, $M_3$) are needed. For the Timoshenko model, six numbers ($F_1$, $F_2$, $F_3$, $M_1$, $M_2$, $M_3$) are needed. Optional. Default values are zero loads.



