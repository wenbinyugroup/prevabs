#include <vector>
#include <cmath>
#include <limits>
#include <iostream>

#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"

// struct Point {
//     double x;
//     double y;
// };

// Helper function to compute the squared Euclidean distance between two points.
// double squaredDistance(const Point &a, const Point &b) {
//     double dx = a.x - b.x;
//     double dy = a.y - b.y;
//     return dx * dx + dy * dy;
// }

// Function to create pairs between points on the baseCurve and the pairedCurve.
// For each point on the baseCurve, the search on pairedCurve starts from the last paired index.
// The loop stops early if the current distance becomes larger than the previous one.
// std::vector<std::vector<int>> createPairs(const std::vector<Point>& baseCurve,
//                                             const std::vector<Point>& pairedCurve) {
//     std::vector<std::vector<int>> pairs;
//     if (baseCurve.empty() || pairedCurve.empty()) return pairs;

//     // Start from the beginning of the pairedCurve
//     int lastPairedIndex = 0;
    
//     // Iterate over each point on the base curve
//     for (size_t i = 0; i < baseCurve.size(); ++i) {
//         int bestIndex = lastPairedIndex;
//         double bestDist = squaredDistance(baseCurve[i], pairedCurve[lastPairedIndex]);
//         double prevDist = bestDist;
        
//         // Iterate over the pairedCurve starting from the last paired index + 1
//         for (size_t j = lastPairedIndex + 1; j < pairedCurve.size(); ++j) {
//             double curDist = squaredDistance(baseCurve[i], pairedCurve[j]);
//             // If the current point is closer, update the bestIndex and distance.
//             if (curDist < prevDist) {
//                 bestIndex = static_cast<int>(j);
//                 bestDist = curDist;
//                 prevDist = curDist;
//             } else {
//                 // If the current distance is larger than the previous one, stop the search.
//                 break;
//             }
//         }
        
//         // Update lastPairedIndex for the next iteration over baseCurve.
//         lastPairedIndex = bestIndex;
//         pairs.push_back({static_cast<int>(i), bestIndex});
//     }
    
//     return pairs;
// }

int i_indent = 0;

bool debug = false;
bool scientific_format = false;
PConfig config;


// Test function for createPairs.
int main() {
    // Test Case 1: Simple curves with clear closest points.
    std::vector<std::vector<double>> baseCurve = { {0, 0}, {1, 0}, {2, 1}, {2, 1.1}, {1, 2} };
    std::vector<std::vector<double>> pairedCurve = { {0, 0.2}, {0.9, 0.2}, {1.7, 1.05}, {1, 1.8} };

    std::vector<PDCELVertex *> base_curve;
    std::vector<PDCELVertex *> paired_curve;

    for (auto &p : baseCurve) {
        base_curve.push_back(new PDCELVertex(0, p[0], p[1]));
    }
    for (auto &p : pairedCurve) {
        paired_curve.push_back(new PDCELVertex(0, p[0], p[1]));
    }

    // auto pairs = createPairs(baseCurve, pairedCurve);
    auto pairs = create_polyline_vertex_pairs(base_curve, paired_curve);

    std::cout << "Test Case 1 Results:\n";
    for (const auto &pair : pairs) {
        std::cout << "Base Curve Index: " << pair[0]
                  << " -> Paired Curve Index: " << pair[1] << "\n";
    }
    
    return 0;
}
