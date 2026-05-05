(section-recover)=
# Specifications for recovery

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

These data are included in a `<global>` element added to the `<cross_section>` (or `<sg>`) element in the main input file.
The same syntax is used for both VABS and SwiftComp recovery; switching solvers is done via the `--vabs` / `--sc` command-line flags.

The nine entries in the direction cosine matrix are flattened into a single array in the `<rotations>` element.
This matrix is defined in {eq}`eq_direction_cosine`.
Note that displacements and rotations are needed only for recovering 3D displacements.
Numbers in the template below are default values.
Once this file is updated correctly, the recover analysis can be carried out via the command-line options described in [How to Run PreVABS](../start/run.md).

$$
\mathbf{B}_i = C_{i1} \mathbf{b}_1 + C_{i2} \mathbf{b}_2 + C_{i3} \mathbf{b}_3 \quad \text{with} \quad i = 1, 2, 3
$$

where $\mathbf{B}_1$, $\mathbf{B}_2$, and $\mathbf{B}_3$ are the base vectors of the deformed triad and $\mathbf{b}_1$, $\mathbf{b}_2$, and $\mathbf{b}_3$ are the base vectors of the undeformed triad.

A template for this part of data is:

```xml
<cross_section name="cs1">
  ...
  <global measure="stress">
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
  - Attributes
    - `measure` - Type of the sectional loads, `stress` (or `0`, default)
      for generalized stresses and `strain` (or `1`) for generalized
      strains.
- `<displacements>` - Three components ($u_1$, $u_2$, $u_3$) of the global displacements. Optional. Default values are {0, 0, 0}.
- `<rotations>` - Nine components ($c_{11}$, $c_{12}$, $c_{13}$, $c_{21}$, $c_{22}$, $c_{23}$, $c_{31}$, $c_{32}$, $c_{33}$) of the global rotations (direction cosine matrix). Optional. Default values are {1, 0, 0, 0, 1, 0, 0, 0, 1}.
- `<loads>` - The sectional loading components. For the Euler-Bernoulli model (`<model>0</model>`), four numbers ($F_1$, $M_1$, $M_2$, $M_3$) are needed. For the Timoshenko model (`<model>1</model>`), six numbers ($F_1$, $F_2$, $F_3$, $M_1$, $M_2$, $M_3$) are needed.
- `<distributed>` - Distributed sectional loads per unit span (Timoshenko model only, VABS). Optional. Default values are zero distributed loads.

  - `<forces>` - Distributed sectional forces ($f_1$, $f_2$, $f_3$).
  - `<forces_d1>` - First derivative of distributed sectional forces ($f'_1$, $f'_2$, $f'_3$).
  - `<forces_d2>` - Second derivative of distributed sectional forces ($f''_1$, $f''_2$, $f''_3$).
  - `<forces_d3>` - Third derivative of distributed sectional forces ($f'''_1$, $f'''_2$, $f'''_3$).
  - `<moments>` - Distributed sectional moments ($m_1$, $m_2$, $m_3$).
  - `<moments_d1>` - First derivative of distributed sectional moments ($m'_1$, $m'_2$, $m'_3$).
  - `<moments_d2>` - Second derivative of distributed sectional moments ($m''_1$, $m''_2$, $m''_3$).
  - `<moments_d3>` - Third derivative of distributed sectional moments ($m'''_1$, $m'''_2$, $m'''_3$).


## Multiple load cases

Multiple load cases can be analyzed in a single run by replacing `<loads>` with one or more `<case>` elements (or by referencing an external CSV file via `<include>`):

```xml
<global measure="stress">
  <displacements> 0 0 0 </displacements>
  <rotations> 1 0 0 0 1 0 0 0 1 </rotations>

  <case>
    <loads>1 0 0 0 0 0</loads>
  </case>
  <case>
    <loads>0 1 0 0 0 0</loads>
  </case>

  <!-- Or load cases listed row-by-row in a CSV file -->
  <include format="csv" header="1">load_cases.csv</include>
</global>
```

Each `<case>` accepts the same children as the legacy single-load-case form (`<displacements>`, `<rotations>`, `<loads>`, and, for VABS Timoshenko, `<distributed>`).
The `<include>` form expects either 4 or 6 numeric columns per row depending on the beam model.


## Failure-envelope load cases

For SwiftComp failure-envelope analysis (`--fe`), each `<case>` must list the two stress / strain axes that span the envelope plane and (optionally) the number of divisions:

```xml
<global measure="stress">
  <case>
    <axis1>f1</axis1>
    <axis2>m1</axis2>
    <divisions>20</divisions>
  </case>
</global>
```

Allowed axis labels are `f1`, `m1`, `m2`, `m3` for the classical model with stress measure (and `e11`, `k11`, `k12`, `k13` for the strain measure); the Timoshenko model adds `f2`, `f3` (or `g12`, `g13`).
