#include <iostream>
#include "homog2d.hpp"

void intersect(h2d::Segment& seg1, h2d::Segment& seg2) {
    std::cout << "Segments: " << seg1 << " and " << seg2;
    auto res = seg1.intersects(seg2);
    if ( res() ) {
        std::cout << " number of intersections: " << res.size();
        auto pts = res.get();
        std::cout << " intersection points: " << pts << std::endl;
    } else {
        std::cout << " do not intersect." << std::endl;
    }
}

int main() {

    h2d::Segment seg1(0, 0, 10, 10);
    h2d::Segment seg2(0, 10, 10, 0);
    h2d::Segment seg3(10.0, 10.0, 20.0, 20.1);
    h2d::Segment seg4(10, 10, 20, 0);

    intersect(seg1, seg2);
    intersect(seg1, seg3);
    intersect(seg2, seg3);
    intersect(seg1, seg4);

    return 0;
}
