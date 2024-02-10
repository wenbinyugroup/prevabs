## Proposed input format

````xml
<cross_section name="" type="airfoil" format="1">

  <analysis>...</analysis>
  <general>...</general>

  <include>
    <material>material_database</material>
    <airfoil>sc1095</airfoil>
  </include>

  <layups>...</layups>

  <topology type="box">

    <bound_le>0.2  90</bound_le>
    <bound_te>0.5  90</bound_te>

    <spar_top>layup_1</spar_top>
    <spar_bottom>layup_2</spar_bottom>

    <web_le>layup_3</web_le>
    <web_te>layup_4</web_te>

    <cap_top>layup_5</cap_top>
    <cap_bottom>layup_5</cap_bottom>

    <overwrap_top>layup_6</overwrap_top>
    <overwrap_bottom>layup_6</overwrap_bottom>

    <fill_le>material_1</fill_le>
    <fill_te>material_2</fill_te>

    <ns_mass>x2 x3 r material</ns_mass>

  </topology>

</cross_section>
````


## Components

Internally, create the following components:
- spar (box), including: 
  - spar_top
  - spar_bottom
  - web_le
  - web_te
- leading edge cap, including:
  - cap_top
  - cap_bottom
- overwrap, including:
  - overwrap_top
  - overwrap_bottom
- non-structural mass
- leading edge filling
- trailing edge filling


