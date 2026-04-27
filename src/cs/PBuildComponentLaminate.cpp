#include "PComponent.hpp"

#include "PDCEL.hpp"
#include "globalConstants.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include <vector>

namespace {

static bool areCoincidentVertices(PDCELVertex *v1, PDCELVertex *v2)
{
  return v1 != nullptr && v2 != nullptr
      && v1->point().distance(v2->point()) <= GEO_TOL;
}

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
  LaminateState &laminate = _laminate;
  Segment *first_segment =
      laminate.segments.empty() ? nullptr : laminate.segments.front();
  Segment *last_segment =
      laminate.segments.empty() ? nullptr : laminate.segments.back();

  if (laminate.cycle) {
    if (laminate.segments.size() == 1 && first_segment != nullptr &&
        !first_segment->closed()) {
      PLOG(error) << "buildLaminate: cyclic component '" << _name
                  << "' with a single open segment is not supported";
      return;
    }

    if (laminate.segments.size() > 1 && first_segment != nullptr &&
        last_segment != nullptr) {
      if (!areCoincidentVertices(
              first_segment->getBeginVertex(), last_segment->getEndVertex())) {
        PLOG(error) << "buildLaminate: cyclic component '" << _name
                    << "' does not close between first segment '"
                    << first_segment->getName() << "' and last segment '"
                    << last_segment->getName() << "'";
        return;
      }

      if (first_segment->getBeginVertex() != last_segment->getEndVertex()) {
        last_segment->curveBase()->vertices().back() =
            first_segment->getBeginVertex();
      }
    }
  }

  for (auto seg : laminate.segments) {

    // seg->curveBase()->print(pmessage, 9);
    seg->offsetCurveBase();

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
      for (auto seg_p : laminate.segments) {
        seg_p->offsetCurveBase();

        if (seg_p != seg) {

          if (seg->headVertexOffset() == nullptr) {
            const JointStyle js = resolveJointStyle(
                seg, seg_p, laminate.style, laminate.joint_segments,
                laminate.joint_styles);
            if (seg->getBeginVertex() == seg_p->getBeginVertex()) {
              // Head to head
              found_begin = true;
              joinSegments(seg, seg_p, 0, 0, seg->getBeginVertex(), js, bcfg);
            } else if (seg->getBeginVertex() == seg_p->getEndVertex()) {
              // Head to tail
              found_begin = true;
              joinSegments(seg, seg_p, 0, 1, seg->getBeginVertex(), js, bcfg);
            }
          }
          if (seg->tailVertexOffset() == nullptr) {
            const JointStyle js = resolveJointStyle(
                seg, seg_p, laminate.style, laminate.joint_segments,
                laminate.joint_styles);
            if (seg->getEndVertex() == seg_p->getBeginVertex()) {
              // Tail to head
              found_end = true;
              joinSegments(seg, seg_p, 1, 0, seg->getEndVertex(), js, bcfg);
            } else if (seg->getEndVertex() == seg_p->getEndVertex()) {
              // Tail to tail
              found_end = true;
              joinSegments(seg, seg_p, 1, 1, seg->getEndVertex(), js, bcfg);
            }
          }
        }

        if (seg->headVertexOffset() != nullptr &&
            seg->tailVertexOffset() != nullptr) {
          break;
        }
      }

      // seg->curveBase()->print(pmessage, 9);

      const bool is_cyclic_head =
          laminate.cycle &&
          seg == first_segment && first_segment != last_segment;
      if (seg->headVertexOffset() == nullptr && !found_begin &&
          !is_cyclic_head) {
        joinSegments(seg, 0, bcfg);
      }

      // seg->curveBase()->print(pmessage, 9);

      const bool is_cyclic_tail =
          laminate.cycle &&
          seg == last_segment && first_segment != last_segment;
      if (seg->tailVertexOffset() == nullptr && !found_end &&
          !is_cyclic_tail) {
        joinSegments(seg, 1, bcfg);
      }

      // seg->curveBase()->print(pmessage, 9);
    }
    // seg->curveBase()->print(pmessage, 9);


  }



  // Build for each segment
  // Create half edge loops for each segment (outer boundary)
  for (auto seg : laminate.segments) {
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

  }


}