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
#include "PModel.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>


void PComponent::buildLaminate(Message *pmessage) {

  // pmessage->increaseIndent();

  PLOG(info) << pmessage->message("building component: " + _name);

  for (auto seg : _segments) {

    PLOG(debug) << "building segment: " << seg->getName();

    if (seg->curveOffset() == nullptr) {
      PLOG(debug) << "segment has no offset curve";
      seg->offsetCurveBase(pmessage);
    }

    if (seg->closed()) {
      PLOG(debug) << "segment is closed";
      seg->setHeadVertexOffset(seg->curveOffset()->vertices().front());
      seg->setTailVertexOffset(seg->curveOffset()->vertices().back());
      seg->setPrevBoundVertices({
        seg->curveBase()->vertices().front(),
        seg->curveOffset()->vertices().front()
      });
      seg->setNextBoundVertices({
        seg->curveBase()->vertices().back(),
        seg->curveOffset()->vertices().back()
      });
      _pmodel->dcel()->addEdge(seg->curveBase()->vertices()[0],
                                seg->curveOffset()->vertices()[0]);
      continue;
    }

    bool found_begin = false;
    bool found_end = false;

    if (seg->headVertexOffset() == nullptr ||
        seg->tailVertexOffset() == nullptr) {
      PLOG(debug) << "handle the segment ends";
      // Check if the segment connects to any other segment or is a free end
      for (auto seg_p : _segments) {
        if (seg_p == seg) {
          continue;
        }

        if (seg_p->curveOffset() == nullptr) {
          seg_p->offsetCurveBase(pmessage);
        }

        // if (seg_p != seg) {
        PLOG(debug) << "checking if segment " << seg->getName() << " connects to segment " << seg_p->getName();

        if (seg->headVertexOffset() == nullptr) {
          PLOG(debug) << "segment " << seg->getName() << " has no offset vertex at the beginning";
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
          PLOG(debug) << "joint style: " << js;

          if (seg->getBeginVertex() == seg_p->getBeginVertex()) {
            // Head to head
            PLOG(debug) << "head of segment " << seg->getName() << " connects to head of segment " << seg_p->getName();
            found_begin = true;
            seg->setPrevSegment(seg_p);
            seg->setPrevEnd(0);
            seg_p->setPrevSegment(seg);
            seg_p->setPrevEnd(0);
            joinSegments(seg, seg_p, 0, 0, seg->getBeginVertex(), js, pmessage);
            // break;
          } else if (seg->getBeginVertex() == seg_p->getEndVertex()) {
            // Head to tail
            PLOG(debug) << "head of segment " << seg->getName() << " connects to tail of segment " << seg_p->getName();
            found_begin = true;
            seg->setPrevSegment(seg_p);
            seg->setPrevEnd(1);
            seg_p->setNextSegment(seg);
            seg_p->setNextEnd(0);
            joinSegments(seg, seg_p, 0, 1, seg->getBeginVertex(), js, pmessage);
            // break;
          }
        }

        if (seg->tailVertexOffset() == nullptr) {
          PLOG(debug) << "segment " << seg->getName() << " has no offset vertex at the tail";
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
          PLOG(debug) << "joint style: " << js;

          if (seg->getEndVertex() == seg_p->getBeginVertex()) {
            // Tail to head
            PLOG(debug) << "tail of segment " << seg->getName() << " connects to head of segment " << seg_p->getName();
            found_end = true;
            seg->setNextSegment(seg_p);
            seg->setNextEnd(0);
            seg_p->setPrevSegment(seg);
            seg_p->setPrevEnd(1);
            joinSegments(seg, seg_p, 1, 0, seg->getEndVertex(), js, pmessage);
            // break;
          } else if (seg->getEndVertex() == seg_p->getEndVertex()) {
            // Tail to tail
            PLOG(debug) << "tail of segment " << seg->getName() << " connects to tail of segment " << seg_p->getName();
            found_end = true;
            seg->setNextSegment(seg_p);
            seg->setNextEnd(1);
            seg_p->setNextSegment(seg);
            seg_p->setNextEnd(1);
            joinSegments(seg, seg_p, 1, 1, seg->getEndVertex(), js, pmessage);
            // break;
          }
        }
        // }

        if (seg->headVertexOffset() != nullptr &&
            seg->tailVertexOffset() != nullptr) {
          PLOG(debug) << "segment " << seg->getName() << " has both head and tail offset vertices";
          break;
        }
      }

      // seg->curveBase()->print(pmessage, 9);

      if (seg->headVertexOffset() == nullptr && !found_begin) {
        PLOG(debug) << "segment " << seg->getName() << " does not connect to any other segment at the head";
        PLOG(debug) << "creating a free end at the head";
        joinSegments(seg, 0, seg->getBeginVertex(), pmessage);
      }

      // seg->curveBase()->print(pmessage, 9);

      if (seg->tailVertexOffset() == nullptr && !found_end) {
        PLOG(debug) << "segment " << seg->getName() << " does not connect to any other segment at the tail";
        PLOG(debug) << "creating a free end at the tail";
        joinSegments(seg, 1, seg->getEndVertex(), pmessage);
      }

    }

  }


  // Build for each segment
  // Create half edge loops for each segment (outer boundary)
  PLOG(debug) << "building half edge loops for each segment";
  for (auto seg : _segments) {

    seg->build(pmessage);

    if (config.debug) _pmodel->plotGeoDebug(pmessage);

  }


  // Check DCEL for debug
  if (config.debug) {
    PLOG(debug) << "writing DCEL to file after building segments";
    _pmodel->dcel()->write_dcel_to_file(config.file_directory + config.file_base_name + "_dcel_done_building_segments_general_shape.txt");
  }

  // pmessage->decreaseIndent();


}
