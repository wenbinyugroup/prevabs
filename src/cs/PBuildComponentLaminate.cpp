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

namespace {

static JointStyle resolveJointStyle(
    Segment *seg, Segment *seg_p, JointStyle default_style,
    const std::vector<std::vector<std::string>> &joint_segments,
    const std::vector<JointStyle> &joint_styles)
{
  for (int jsi = 0; jsi < joint_segments.size(); ++jsi) {
    if ((seg->getName() == joint_segments[jsi][0] &&
         seg_p->getName() == joint_segments[jsi][1]) ||
        (seg->getName() == joint_segments[jsi][1] &&
         seg_p->getName() == joint_segments[jsi][0])) {
      return joint_styles[jsi];
    }
  }
  return default_style;
}

} // namespace

void PComponent::buildLaminate(const BuilderConfig &bcfg) {
  MESSAGE_SCOPE(g_msg);

  for (auto seg : _segments) {

    // seg->curveBase()->print(pmessage, 9);
    if (seg->curveOffset() == nullptr) {
      seg->offsetCurveBase();
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
      bcfg.dcel->addEdge(seg->curveBase()->vertices()[0],
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
          seg_p->offsetCurveBase();
        }

        if (seg_p != seg) {

          if (seg->headVertexOffset() == nullptr) {
            const JointStyle js = resolveJointStyle(
                seg, seg_p, _style, _joint_segments, _joint_styles);
            if (seg->getBeginVertex() == seg_p->getBeginVertex()) {
              // Head to head
              found_begin = true;
              joinSegments(seg, seg_p, 0, 0, seg->getBeginVertex(), js, bcfg);
              // break;
            } else if (seg->getBeginVertex() == seg_p->getEndVertex()) {
              // Head to tail
              found_begin = true;
              joinSegments(seg, seg_p, 0, 1, seg->getBeginVertex(), js, bcfg);
              // break;
            }
          }
          if (seg->tailVertexOffset() == nullptr) {
            const JointStyle js = resolveJointStyle(
                seg, seg_p, _style, _joint_segments, _joint_styles);
            if (seg->getEndVertex() == seg_p->getBeginVertex()) {
              // Tail to head
              found_end = true;
              joinSegments(seg, seg_p, 1, 0, seg->getEndVertex(), js, bcfg);
              // break;
            } else if (seg->getEndVertex() == seg_p->getEndVertex()) {
              // Tail to tail
              found_end = true;
              joinSegments(seg, seg_p, 1, 1, seg->getEndVertex(), js, bcfg);
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
        joinSegments(seg, 0, seg->getBeginVertex(), bcfg);
      }

      // seg->curveBase()->print(pmessage, 9);

      if (seg->tailVertexOffset() == nullptr && !found_end) {
        joinSegments(seg, 1, seg->getEndVertex(), bcfg);
      }

      // seg->curveBase()->print(pmessage, 9);
    }
    // seg->curveBase()->print(pmessage, 9);


  }



  // Build for each segment
  // Create half edge loops for each segment (outer boundary)
  for (auto seg : _segments) {
    if (seg->headVertexOffset() == nullptr) {
      PLOG(error) << "buildLaminate: missing head offset vertex"
                  << " for segment '" << seg->getName()
                  << "' in component '" << _name << "'";
      return;
    }

    if (seg->tailVertexOffset() == nullptr) {
      PLOG(error) << "buildLaminate: missing tail offset vertex"
                  << " for segment '" << seg->getName()
                  << "' in component '" << _name << "'";
      return;
    }

    seg->build(bcfg);

    if (bcfg.debug && bcfg.plotDebug) bcfg.plotDebug(g_msg);

  }


}
