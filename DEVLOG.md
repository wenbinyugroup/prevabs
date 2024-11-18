# Development Log

## Backlog

- [Bug] Issue of generating layups for a closed line if the closing point has joint angle larger than 180 degrees
- [Bug] Issue of creating a new point on airfoil using 'x2' if it coincides with an existing point in the airfoil data

## Done

2023/04/14

- [Bug] Fixed an issue of writing incompatible input for SwiftComp version 2.2.
- [New] Added a new command line option to configure VABS/SwiftComp version.

2023/02/19

- [New] Added thermo-related inputs (CTE and specific heat) for SwiftComp.
- [New] Added a new input option `physics` under `analysis` to indicate the analysis type for SwiftComp.

2023/02/09

- [New] Added the capability to identify interfaces and output the nodes on the interfaces, with three new input options under `general`: `track_interface`, `interface_theta1_diff_threshold`, and `interface_theta3_diff_threshold`.

2022/12/10

- [New] Added the input and capability to reverse the line with type "airfoil"

2022/11/24

- [New] Added the capability to set material orientation align the base line

2022/11/22

- [Bug] Fixed the issue of not reading the root 'sg' element for dehomogenization and failure analysis
- [Bug] Fixed the issue of not reading 'global' input without loads for failure strength analysis

2022/11/18

- [New] Added the capability to change the free end shape using a vector

2022/11/16

- [New] Added the input for omega (General volume of the SG in the periodic dimensions; SwiftComp only)
- [New] Added the input for material and lamina in the main input file.

2022/11/14

- [Bug] Fixed the issue of writing wrong flag for general anisotropic materials.

2022/07/23

- [Bug] Fixed the issue of reading result of failure analysis of the new format with an extra line of load case label.
- [Opt] Changed the Gmsh plot option for strength ratios.


## Changes in Gmsh

- Added a new member variable `tags` in the class `GEntity` (`GEntity.h`).
