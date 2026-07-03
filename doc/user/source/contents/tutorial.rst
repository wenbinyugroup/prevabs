.. _tutorial:

Tutorial
========

This tutorial demonstrates how to prepare a PreVABS input file from a
cross-section design.

Cross-section design
--------------------

.. figure:: /figures/tutorial_box_0.png
  :name: fig_tutorial_box
  :width: 6in
  :align: center

  A box-beam cross section.

This tutorial uses the box-beam cross section shown in
:numref:`Fig. %s <fig_tutorial_box>`. Its four walls and two webs are
composite laminates. The webs are symmetric about the vertical centerline
and incline toward one another at the top. An isotropic material fills the
region enclosed by the top and bottom walls and the two webs.

Four parameters define the overall shape:

- Width: :math:`w = 4` m
- Height: :math:`h = 2` m
- Distance: :math:`d = 1` m
- Web angle: :math:`a = 100^\circ`

The material properties are listed in
:numref:`Table %s <tutorial_materials_iso>` and
:numref:`Table %s <tutorial_materials_ortho>`. The laminate layups are
listed in
:numref:`Table %s <tutorial_layups>`.

.. csv-table:: Isotropic material properties
  :name: tutorial_materials_iso
  :header-rows: 2
  :align: center

  "Name", "Density", :math:`E`, :math:`\nu`
   , |den_si|, |mod_si_k|,
  "m0", 1.00, 25.00, 0.30

.. tabularcolumns:: |l|r|r|r|r|r|r|r|r|r|r|

.. csv-table:: Orthotropic material properties
  :name: tutorial_materials_ortho
  :header-rows: 2
  :align: center

  "Name", "Density", |e1|, |e2|, |e3|, |g12|, |g13|, |g23|, |nu12|, |nu13|, |nu23|
   , |den_si_k|, |mod_si_g|, |mod_si_g|, |mod_si_g|, |mod_si_g|, |mod_si_g|, |mod_si_g|, , ,
  "m1", 1.86, 37.00, 9.00, 9.00, 4.00, 4.00, 4.00, 0.28, 0.28, 0.28
  "m2", 1.83, 10.30, 10.30, 10.30, 8.00, 8.00, 8.00, 0.30, 0.30, 0.30

.. csv-table:: Layups
  :name: tutorial_layups
  :header-rows: 2
  :align: center

  "Component", "Name", "Material", "Ply thickness", "Orientation", "Number of plies"
  , , , "m", "degree",
  "Walls", "layup1", "m1", 0.02,  45, 1
         ,          , "m1", 0.02, -45, 1
         ,          , "m2", 0.05,   0, 1
         ,          , "m1", 0.02,   0, 2
  "Webs", "layup2", "m2", 0.05, 0, 1
        ,          , "m1", 0.02, 0, 3
        ,          , "m2", 0.05, 0, 1



Preparing the input file
------------------------

In PreVABS, a cross section consists of laminate and fill components. A
laminate component contains one or more segments, each with an assigned
layup. A cross section can often be decomposed into components in several
ways, and the chosen decomposition determines the information required to
build the model.

This example contains four components: one component for the walls, one
for each of the two webs, and one for the fill. Their creation order follows
their dependencies. The walls define the boundaries that trim the webs, and
the walls and webs together enclose the fill region. PreVABS must therefore
create the walls first, the webs second, and the fill last, as shown in
:numref:`Fig. %s <fig_tutorial_box_order>`.

.. figure:: /figures/tutorial_box_order.png
  :name: fig_tutorial_box_order
  :width: 6in
  :align: center

  Order of component creation.

The design is defined in a self-contained XML file named ``box.xml``. Its
``<cross_section>`` element contains the analysis and meshing settings,
points and baselines, materials and laminae, layups, and components. Keeping
this small example in one file makes the relationships among these
definitions explicit. Larger models can store definitions in separate files;
see :ref:`input-other-settings`.

Define the geometry
^^^^^^^^^^^^^^^^^^^

As shown in :numref:`Fig. %s <fig_tutorial_box_points>`, seven points
define the cross-section geometry. Points p1 through p4 define the outer
walls, p5 and p6 locate the webs, and p0 identifies the region to fill. The
coordinate-system origin is at the center of the rectangle. The coordinates,
derived from :math:`w`, :math:`h`, and :math:`d`, are listed in
:numref:`Table %s <table_tutorial_box_points>`.

.. figure:: /figures/tutorial_box_points.png
  :name: fig_tutorial_box_points
  :width: 6in
  :align: center

  Key points defining the shape of the cross section.

.. csv-table:: Key points
  :name: table_tutorial_box_points
  :header-rows: 1
  :align: center

  "Name", "Coordinate"
  "p0", "(0, 0)"
  "p1", "(2, 1)"
  "p2", "(-2, 1)"
  "p3", "(-2, -1)"
  "p4", "(2, -1)"
  "p5", "(1, 0)"
  "p6", "(-1, 0)"

The points define the three baselines shown in
:numref:`Fig. %s <fig_tutorial_box_lines>`. Line 1 forms a closed path through
p1 -> p2 -> p3 -> p4 -> p1. Line 2 passes through p5 at an angle of 100
degrees, while line 3 passes through p6 at an angle of 80 degrees.

Baseline direction is significant: it determines the side on which each
laminate is created and contributes to the definition of each element's local
coordinate system.

The completed geometry section of ``box.xml`` is shown in
:numref:`Listing %s <code_tutorial_box_baselines>`.

.. figure:: /figures/tutorial_box_lines.png
  :name: fig_tutorial_box_lines
  :width: 6in
  :align: center

  Baselines defining the shape of the cross section.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_baselines
  :caption: Geometric elements in ``box.xml``.

  <baselines>
    <point name="p0">0 0</point>
    <point name="p1">2 1</point>
    <point name="p2">-2 1</point>
    <point name="p3">-2 -1</point>
    <point name="p4">2 -1</point>
    <point name="p5">1 0</point>
    <point name="p6">-1 0</point>

    <line name="line1">
      <points>p1,p2,p3,p4,p1</points>
    </line>
    <line name="line2">
      <point>p5</point>
      <angle>100</angle>
    </line>
    <line name="line3">
      <point>p6</point>
      <angle>80</angle>
    </line>
  </baselines>

Define the materials and layups
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``<materials>`` section of ``box.xml`` contains both material and lamina
definitions, as shown in
:numref:`Listing %s <code_tutorial_box_materials>`. The contents of each
``<elastic>`` element depend on whether the material is isotropic,
orthotropic, or anisotropic.

A lamina combines a material with a ply thickness. The layups in
:numref:`Table %s <tutorial_layups>` require two laminae: ``la_m1_002`` uses
material m1 with a thickness of 0.02 m, and ``la_m2_005`` uses material m2
with a thickness of 0.05 m. Layup layers reference these laminae rather than
the materials directly.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_materials
  :caption: Materials and laminae in ``box.xml``.

  <materials>
    <material name="m0" type="isotropic">
      <density>1.0</density>
      <elastic>
        <e>25.0e3</e>
        <nu>0.3</nu>
      </elastic>
    </material>
    <material name="m1" type="orthotropic">
      <density>1.86E+03</density>
      <elastic>
        <e1>3.70E+10</e1>
        <e2>9.00E+09</e2>
        <e3>9.00E+09</e3>
        <g12>4.00E+09</g12>
        <g13>4.00E+09</g13>
        <g23>4.00E+09</g23>
        <nu12>0.28</nu12>
        <nu13>0.28</nu13>
        <nu23>0.28</nu23>
      </elastic>
    </material>
    <lamina name="la_m1_002">
      <material>m1</material>
      <thickness>0.02</thickness>
    </lamina>
    <material name="m2" type="orthotropic">
      <density>1.83E+03</density>
      <elastic>
        <e1>1.03E+10</e1>
        <e2>1.03E+10</e2>
        <e3>1.03E+10</e3>
        <g12>8.00E+09</g12>
        <g13>8.00E+09</g13>
        <g23>8.00E+09</g23>
        <nu12>0.30</nu12>
        <nu13>0.30</nu13>
        <nu23>0.30</nu23>
      </elastic>
    </material>
    <lamina name="la_m2_005">
      <material>m2</material>
      <thickness>0.05</thickness>
    </lamina>
  </materials>

The ``<layups>`` section follows the material definitions, as shown in
:numref:`Listing %s <code_tutorial_box_layups>`. Layer order defines the
stacking sequence outward from the baseline. The text inside each ``<layer>``
element specifies the ply angle in degrees. A value after a colon specifies
the number of plies; for example, ``0:3`` represents three 0-degree plies.
An empty element represents one 0-degree ply.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_layups
  :caption: Layups in ``box.xml``.

  <layups>
    <layup name="layup1">
      <layer lamina="la_m1_002">45</layer>
      <layer lamina="la_m1_002">-45</layer>
      <layer lamina="la_m2_005">0</layer>
      <layer lamina="la_m1_002">0:2</layer>
    </layup>
    <layup name="layup2">
      <layer lamina="la_m2_005"></layer>
      <layer lamina="la_m1_002">0:3</layer>
      <layer lamina="la_m2_005"></layer>
    </layup>
  </layups>

Define the components
^^^^^^^^^^^^^^^^^^^^^

PreVABS supports laminate and fill components. This example has three
laminate components (the walls and two webs) and one fill component.
Dependencies specify which components must exist before another component
can be created.

Each laminate component contains one or more segments. A segment combines a
baseline with a layup. The ``direction`` attribute places the layup on the
``left`` or ``right`` side of the baseline, as viewed while moving in the
baseline direction. The default is ``left``. This convention is illustrated
in :numref:`Fig. %s <fig_tutorial_box_segments>`.

The walls must be created first because their inner boundary trims both web
baselines. Each web therefore depends on the wall component, as shown in
:numref:`Listing %s <code_tutorial_box_laminate>`.

.. figure:: /figures/tutorial_box_segments.png
  :name: fig_tutorial_box_segments
  :width: 6in
  :align: center

  Layup directions for each segment.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_laminate
  :caption: Input elements for the laminate-type components

  <component name="walls">
    <segment>
      <baseline>line1</baseline>
      <layup>layup1</layup>
    </segment>
  </component>
  <component name="web1" depend="walls">
    <segment>
      <baseline>line2</baseline>
      <layup>layup2</layup>
    </segment>
  </component>
  <component name="web2" depend="walls">
    <segment>
      <baseline>line3</baseline>
      <layup direction="right">layup2</layup>
    </segment>
  </component>

The fill component is defined by a point and a material. Point p0 identifies
the enclosed central region, and material m0 fills that region. Because the
walls and both webs bound the region, the fill depends on all three laminate
components, as shown in
:numref:`Listing %s <code_tutorial_box_fill>`.

.. figure:: /figures/tutorial_box_fill.png
  :name: fig_tutorial_box_fill
  :width: 6in
  :align: center

  The fill-type component.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_fill
  :caption: Input elements for the fill-type component

  <component name="fill" type="fill" depend="walls,web1,web2">
    <location>p0</location>
    <material>m0</material>
  </component>

Set the analysis and meshing options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In addition to the geometry, materials, layups, and components, the input
file contains analysis and meshing settings.

The optional ``<analysis>`` section configures the VABS cross-sectional
analysis. In this example, ``<model>1</model>`` selects the Timoshenko beam
model and produces a 6-by-6 stiffness matrix.

The optional ``<general>`` section configures the geometry and mesh. This
example uses a target mesh size of 0.04 and linear quadrilateral elements.

The complete cross-section input file is shown in
:numref:`Listing %s <code_tutorial_box_main>`.

.. code-block:: xml
  :linenos:
  :name: code_tutorial_box_main
  :caption: Input file for the cross section

  <cross_section name="box">

    <analysis>
      <model>1</model>
    </analysis>

    <general>
      <mesh_size>0.04</mesh_size>
      <element_shape>quad</element_shape>
      <element_type>linear</element_type>
    </general>

    <baselines>
      <point name="p0">0 0</point>
      <point name="p1">2 1</point>
      <point name="p2">-2 1</point>
      <point name="p3">-2 -1</point>
      <point name="p4">2 -1</point>
      <point name="p5">1 0</point>
      <point name="p6">-1 0</point>

      <line name="line1">
        <points>p1,p2,p3,p4,p1</points>
      </line>
      <line name="line2">
        <point>p5</point>
        <angle>100</angle>
      </line>
      <line name="line3">
        <point>p6</point>
        <angle>80</angle>
      </line>
    </baselines>

    <materials>
      <material name="m0" type="isotropic">
        <density>1.0</density>
        <elastic>
          <e>25.0e3</e>
          <nu>0.3</nu>
        </elastic>
      </material>
      <material name="m1" type="orthotropic">
        <density>1.86E+03</density>
        <elastic>
          <e1>3.70E+10</e1>
          <e2>9.00E+09</e2>
          <e3>9.00E+09</e3>
          <g12>4.00E+09</g12>
          <g13>4.00E+09</g13>
          <g23>4.00E+09</g23>
          <nu12>0.28</nu12>
          <nu13>0.28</nu13>
          <nu23>0.28</nu23>
        </elastic>
      </material>
      <lamina name="la_m1_002">
        <material>m1</material>
        <thickness>0.02</thickness>
      </lamina>
      <material name="m2" type="orthotropic">
        <density>1.83E+03</density>
        <elastic>
          <e1>1.03E+10</e1>
          <e2>1.03E+10</e2>
          <e3>1.03E+10</e3>
          <g12>8.00E+09</g12>
          <g13>8.00E+09</g13>
          <g23>8.00E+09</g23>
          <nu12>0.30</nu12>
          <nu13>0.30</nu13>
          <nu23>0.30</nu23>
        </elastic>
      </material>
      <lamina name="la_m2_005">
        <material>m2</material>
        <thickness>0.05</thickness>
      </lamina>
    </materials>

    <layups>
      <layup name="layup1">
        <layer lamina="la_m1_002">45</layer>
        <layer lamina="la_m1_002">-45</layer>
        <layer lamina="la_m2_005">0</layer>
        <layer lamina="la_m1_002">0:2</layer>
      </layup>
      <layup name="layup2">
        <layer lamina="la_m2_005"></layer>
        <layer lamina="la_m1_002">0:3</layer>
        <layer lamina="la_m2_005"></layer>
      </layup>
    </layups>

    <component name="walls">
      <segment>
        <baseline>line1</baseline>
        <layup>layup1</layup>
      </segment>
    </component>
    <component name="web1" depend="walls">
      <segment>
        <baseline>line2</baseline>
        <layup>layup2</layup>
      </segment>
    </component>
    <component name="web2" depend="walls">
      <segment>
        <baseline>line3</baseline>
        <layup direction="right">layup2</layup>
      </segment>
    </component>
    <component name="fill" type="fill" depend="walls,web1,web2">
      <location>p0</location>
      <material>m0</material>
    </component>

  </cross_section>


Run PreVABS and review the results
----------------------------------

Run the following command to build and homogenize the cross section:

::

  prevabs -i box.xml --hm -v -e

PreVABS calls Gmsh to generate and display the cross-section mesh, then calls
VABS to perform the homogenization analysis. The resulting cross section is
shown in :numref:`Fig. %s <fig_tutorial_box_plot>`; element edges are hidden
for clarity. VABS writes the effective beam properties to ``box.sg.K``. The
effective Timoshenko stiffness matrix from that file is reproduced in
:numref:`Table %s <tutorial_stiffness>`.


.. figure:: /figures/tutorial_box.png
  :name: fig_tutorial_box_plot
  :width: 6.5in
  :align: center

  The cross section created by PreVABS and plotted by Gmsh.

.. csv-table:: Timoshenko stiffness matrix in the file *box.sg.K*
  :name: tutorial_stiffness
  :delim: space
  :align: center

   3.991E+10   -1.563E+05    1.321E+00    4.222E+07   -5.830E+03    4.373E+01
  -1.563E+05    6.747E+09   -1.726E+02   -2.919E+08   -1.586E+07    4.747E+01
   1.321E+00   -1.726E+02    6.172E+09   -1.159E+03   -1.404E+01   -8.872E+06
   4.222E+07   -2.919E+08   -1.159E+03    1.973E+10    2.742E+05   -3.599E+01
  -5.830E+03   -1.586E+07   -1.404E+01    2.742E+05    2.173E+10    6.369E+01
   4.373E+01    4.747E+01   -8.872E+06   -3.599E+01    6.369E+01    6.728E+10
