Instructions to AI to write an offset function


Write c++ code to offset a polyline by a specified distance.

Details:
- Consider the 2D Cartesian coordinates (x, y) only.
- The polyline is represented using a vector of vertices.
- The polyline has orientation, which is represented by the order of vertices in the vector.
- The function handles offset to one side only.
- Definition of the offset side: For each line segment, there is a tangent direction defined by the beginning and ending vertices. There is also a direction normal to the 2D plane and pointing outward, which is the binormal direction. The cross-product of the binormal and tangent using the right-hand-rule will give the normal direction. This is the "left" side of the polyline. Then the opposite direction is the "right" side.
- Sould be able to handle both open and closed polylines, and ensures that the resulting offset vertices maintain the correct orientation.

Inputs:
- Base polyline: A vector of vertices to be offset.
- Side: The side on which to offset the vertices (e.g., left or right).
- Distance: The distance by which to offset the vertices.

Outputs:
- Offset polyline: The output vector of offset vertices.

