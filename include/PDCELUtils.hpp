#pragma once
// Safe DCEL half-edge loop traversal with a hard iteration cap.
// Prevents infinite loops when a half-edge cycle is broken and next()
// never returns to the start half-edge.

#include "PDCELHalfEdge.hpp"
#include "plog.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

// Hard cap for DCEL half-edge loop traversals.  Large enough for any normal
// cross-section mesh; tight enough to terminate quickly on a broken cycle.
static const int kDCELLoopHardCap = 65536;

static const int kDCELWarnLoopSteps = 128;

std::string formatLoopWalkHalfEdge(PDCELHalfEdge *he);
std::string formatLoopWalkFace(PDCELHalfEdge *he);

// Traverse do { op(he); he = he->next(); } while (he != start) safely.
// Throws std::runtime_error containing "DCEL loop walk exceeded" if more
// than max_iter steps are taken.
template <typename Op>
void walkLoopWithLimit(PDCELHalfEdge *start, Op op,
                       int max_iter = kDCELLoopHardCap) {
  if (start == nullptr) return;
  PDCELHalfEdge *he = start;
  int iter = 0;
  do {
    if (iter >= max_iter) {
      std::ostringstream oss;
      oss << "DCEL loop walk exceeded " << max_iter
          << " iterations"
          << " | start=" << formatLoopWalkHalfEdge(start)
          << " | current=" << formatLoopWalkHalfEdge(he)
          << " | face=" << formatLoopWalkFace(he);
      throw std::runtime_error(oss.str());
    }
    if (iter == 0) {
      PLOG_DEBUG_AT(geo) << "walkLoopWithLimit: start"
                         << " | step=0"
                         << " | start=" << formatLoopWalkHalfEdge(start)
                         << " | current=" << formatLoopWalkHalfEdge(he)
                         << " | face=" << formatLoopWalkFace(he);
    }
    if (iter == kDCELWarnLoopSteps) {
      PLOG(warning) << "walkLoopWithLimit: unusually long loop walk"
                    << " (>= " << kDCELWarnLoopSteps << " steps)"
                    << " | start=" << formatLoopWalkHalfEdge(start)
                    << " | current=" << formatLoopWalkHalfEdge(he)
                    << " | face=" << formatLoopWalkFace(he);
    }
    ++iter;
    op(he);
    he = he->next();
  } while (he != start);
}
