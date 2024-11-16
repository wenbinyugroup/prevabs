# Change log

## Version 1.6

- 1.6.0 (2024)
  - Changed the gmsh linking to the shared library from the SDK.
  - Fixed the issue of unabling to run PreVABS without VABS dll.
  - Fixed the issue of not reading airfoil data properly on linux.
  - Fixed the issue of not skipping header rows of airfoil data.
  - Fixed the issue of not able to run the input without the "include" block.

## Version 1.5

- 1.5.2
  - Added a new output file after creating the cross-section to store the mapping between material IDs and names.

- 1.5.1 (2022/10/27)
  - Fixed the issue of reading result of failure analysis of the new format with an extra line of load case label.
  - Added a new tag name for the root XML element: "sg".
  - Added a new input for output model dimension (for SwiftComp only).
  - Updated the writing function for 3D solid model (for SwiftComp only).
  - Changed the Gmsh plot option for strength ratios.

- 1.5.0 (2022/06/05)
  - Added a new type of line "airfoil", which can accept the file name of a standard airfoil data file.
  - Added new arguments for points defined on an airfoil line.
  - Added a new function to remove very short edges, very close vertices.
  - Added a new input of length tolerance for edge removal.
  - Added a new input to for multiple loading cases for dehomogenization and failure analysis.
  - Added a new input to include a file of loading cases for dehomogenization and failure analysis.
  - Added a new method to define an arc using two end points and a radius.
  - Added a new method to define a line by joining multiple lines.
  - Set the default line type to "straight", which can be omitted now.
  - Fixed the issue of overlapping/duplicated mesh verticies.

## Version 1.4

- 1.4.3 (2022/01/11)
  - Fixed the issue of plotting element stress and strain results.
  - Fixed the issue of plotting dehomogenization results with quadratic elements.
  - Added groups for different stress and strain plots.
  - Added alternative tag names for material strength properties.
  - Added visualization for initial failure analysis results.
  - Added a new example for initial failure analysis (ex_uh60a_f).
  - Simplifed and unified material strength inputs for different types of materials.
  - Updated the section of material strength property input in the documentation.

- 1.4.2 (2021/11/05)
  - Fixed an issue related with layer generation.

- 1.4.1 (2021/09/01)
  - Fixed an issue in assigning theta1 and theta3 for filling components.

- 1.4.0 (2021/06/16)
  - Added a new command option to run integrated VABS.
  - Added new inputs for the rotor blade specific parameterization.
  - Added new inputs for strength property of 'lamina' type materials.
  - Added new inputs for assigning orientations for filling materials.
  - Added a new input option for recover analysis in VABS. The default is linear beam theory.
  - Added a new input option for setting the tolerance used in geometric computation.
  - Added the capability of failure analysis using VABS.
  - Updated the logging system.
  - Changed the default element type to 'quadratic'.
  - Unified the input syntax of recovery/dehomogenization of VABS/SwfitComp.
  - Fixed the issue of incorrectly parsing numbers with more than one spaces between numbers.
  - Fixed an issue in transforming an arc defined by center, start and radius.

## Version 1.3

- 1.3.0 (2021/02/25)
  - Added a new capability to create base points using normalized parametric locations on a base line.
  - Added a new capability to assign local mesh size for filling components.

## Version 1.2

- 1.2.0 (2020/11/10)
  - Added a new capability to create layups from sublayups.
  - Added a new capability to create segments using normalized parametric locations on a base line.

## Version 1.1

- 1.1.1 (2020/10/28)
  - Fixed the problem of unable to read recover analysis result files on Linux.
  - Fixed a problem when the segment is too short while the laminate is too thick.
- 1.1.0 (2020/10/15)
  - Added a new format for the input file. Now baselines and layups data are merged into the main input file, to reduce the number of input files needed.
  - Added a new material type 'lamina' accepting four numbers for elastic properties.
  - Added default small numbers for elastic properties for isotropic materials.
  - Updated the fill-type component for non-structural mass.

## Version 1.0 (2019/07/01)

- Added capability to create quadratic triangle elements. In the main input file, under the xml element `<general>`, create a sub element `<element_type>` and set it to `quadratic` or `2`. Default is `linear`.
- Added a new output file `*.txt` storing all running messages.
- Changed one of the layup methods tag name from 'explict list' ('el') to 'layer list' ('ll').
- Fixed the crashing issue caused by zero number of layers.
- Fixed the bug that sectional loads were read and written to the distributed loads when doing recovery using VABS.
- Fixed the bug that local lamina data does not overwrite global data.

## Version 0.6 (2018/07/01)

- Added xml elements in the main input file for various options of VABS/SwiftComp execution.
- Added xml elements in the main input file for failure analysis of SwiftComp.
- Added a `<basepoints>` element in the baseline file. Now for simple shapes, users do not need to use an extra basepoint file, which can still be included for long point list.
- Added a material database file along with the executable. Now PreVABS will look for materials in this file by default.
- Changed Gmsh library to dynamic/shared library.

## Version 0.5 (2018/01/31)

- Added the capability to create nose mass in an airfoil type cross section.
- Added the post-processing function to visualize the recovered strains and stresses in Gmsh.

## Version 0.4 (2017/12/04)

- Added the capability to read stacking sequence code.
- Added the parameter to set the number of straight lines to approximate an arc or circle.
- Changed the Gmsh input file name to `*.msh`, where `*` is the cross section name.

## Version 0.3 (2017/11/27)

- Updated the manual.
- Changed the default behaviour of the command with input file specified only to preparing VABS input (without running VABS).
- Changed the element tag `<origin>` to `<translate>`. Now the two numbers in this element moves base points and base lines, instead of the origin.
- Changed the element tag `<rotation>` to `<rotate>`.
- Changed the 'level' of a segment from an element to an attribute, with default 1.
- Changed the 'layup_direction' of a segment from an element to an attribute of the layup, with default 'right'.
- Changed the material type 'engineering constants' to 'orthotropic'.
- Set the default element type as 'quadratic'.
- Set the default mesh size to the smallest layer thickness.

