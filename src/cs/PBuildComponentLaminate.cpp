#include "PComponent.hpp"

#include "Material.hpp"
#include "PDCEL.hpp"
#include "PGeoClasses.hpp"
#include "PSegment.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "PBaseLine.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>


void PComponent::buildLaminate(Message *pmessage) {

  for (auto seg : _segments) {

    // seg->curveBase()->print(pmessage, 9);
    if (seg->curveOffset() == nullptr) {
      seg->offsetCurveBase(pmessage);
    }

    // std::cout << "base line: " <<  seg->curveBase()->vertices().front();
    // std::cout << " -> " <<  seg->curveBase()->vertices().back() << std::endl;
    // std::cout << "offset line: " <<  seg->curveOffset()->vertices().front();
    // std::cout << " -> " <<  seg->curveOffset()->vertices().back() << std::endl;

    // if (config.debug) {
    //   seg->curveOffset()->print(pmessage, 9);
    // }

    if (seg->closed()) {
      seg->setHeadVertexOffset(seg->curveOffset()->vertices().front());
      seg->setTailVertexOffset(seg->curveOffset()->vertices().back());
      _pmodel->dcel()->addEdge(seg->curveBase()->vertices()[0],
                                seg->curveOffset()->vertices()[0]);
      continue;
    }

    // seg->curveBase()->print(pmessage, 9);

    bool found_begin = false;
    bool found_end = false;

    if (seg->headVertexOffset() == nullptr ||
        seg->tailVertexOffset() == nullptr) {
      // Check if the segment connects to any other segment or is a free end
      for (auto seg_p : _segments) {
        if (seg_p->curveOffset() == nullptr) {
          seg_p->offsetCurveBase(pmessage);
        }

        if (seg_p != seg) {
          // std::cout << "[debug] first vertex of seg " << seg->getName() <<
          // ": "; seg->getBeginVertex()->printWithAddress(); std::cout <<
          // "[debug] last vertex of seg " << seg->getName() << ": ";
          // seg->getEndVertex()->printWithAddress();
          // std::cout << "[debug] first vertex of seg_p " << seg_p->getName()
          // << ": "; seg_p->getBeginVertex()->printWithAddress(); std::cout
          // << "[debug] last vertex of seg_p " << seg_p->getName() << ": ";
          // seg_p->getEndVertex()->printWithAddress();

          if (seg->headVertexOffset() == nullptr) {
            int js = _style;
            for (int jsi = 0; jsi < _joint_segments.size(); ++jsi) {
              if ((seg->getName() == _joint_segments[jsi][0] &&
                    seg_p->getName() == _joint_segments[jsi][1]) ||
                  (seg->getName() == _joint_segments[jsi][1] &&
                    seg_p->getName() == _joint_segments[jsi][0])) {
                js = _joint_styles[jsi];
                break;
              }
            }
            if (seg->getBeginVertex() == seg_p->getBeginVertex()) {
              // Head to head
              found_begin = true;
              joinSegments(seg, seg_p, 0, 0, seg->getBeginVertex(), js, pmessage);
              // break;
            } else if (seg->getBeginVertex() == seg_p->getEndVertex()) {
              // Head to tail
              found_begin = true;
              joinSegments(seg, seg_p, 0, 1, seg->getBeginVertex(), js, pmessage);
              // break;
            }
          }
          if (seg->tailVertexOffset() == nullptr) {
            int js = _style;
            for (int jsi = 0; jsi < _joint_segments.size(); ++jsi) {
              if ((seg->getName() == _joint_segments[jsi][0] &&
                    seg_p->getName() == _joint_segments[jsi][1]) ||
                  (seg->getName() == _joint_segments[jsi][1] &&
                    seg_p->getName() == _joint_segments[jsi][0])) {
                js = _joint_styles[jsi];
                break;
              }
            }
            if (seg->getEndVertex() == seg_p->getBeginVertex()) {
              // Tail to head
              found_end = true;
              joinSegments(seg, seg_p, 1, 0, seg->getEndVertex(), js, pmessage);
              // break;
            } else if (seg->getEndVertex() == seg_p->getEndVertex()) {
              // Tail to tail
              found_end = true;
              joinSegments(seg, seg_p, 1, 1, seg->getEndVertex(), js, pmessage);
              // break;
            }
          }
        }

        if (seg->headVertexOffset() != nullptr &&
            seg->tailVertexOffset() != nullptr) {
          break;
        }
      }

      // seg->curveBase()->print(pmessage, 9);

      if (seg->headVertexOffset() == nullptr && !found_begin) {
        joinSegments(seg, 0, seg->getBeginVertex(), pmessage);
      }

      // seg->curveBase()->print(pmessage, 9);

      if (seg->tailVertexOffset() == nullptr && !found_end) {
        joinSegments(seg, 1, seg->getEndVertex(), pmessage);
      }

      // seg->curveBase()->print(pmessage, 9);
    }
    // seg->curveBase()->print(pmessage, 9);


  }



  // Build for each segment
  // Create half edge loops for each segment (outer boundary)
  for (auto seg : _segments) {

    seg->build(pmessage);

    if (config.debug) _pmodel->plotGeoDebug(pmessage);

  }


}
