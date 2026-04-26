#pragma once
// Safe DCEL half-edge loop traversal with a hard iteration cap.
// Prevents infinite loops when a half-edge cycle is broken and next()
// never returns to the start half-edge.

#include "PDCELHalfEdge.hpp"
#include <stdexcept>
#include <string>

// Hard cap for DCEL half-edge loop traversals.  Large enough for any normal
// cross-section mesh; tight enough to terminate quickly on a broken cycle.
static const int kDCELLoopHardCap = 65536;

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
      throw std::runtime_error(
          std::string("DCEL loop walk exceeded ") + std::to_string(max_iter) +
          " iterations starting at " + start->printString());
    }
    ++iter;
    op(he);
    he = he->next();
  } while (he != start);
}
