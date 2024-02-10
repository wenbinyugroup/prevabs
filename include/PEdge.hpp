#pragma once

#include "PDCELVertex.hpp"
#include "PDCELHalfEdge.hpp"

class PEdge {
private:
  PDCELVertex *_incident_vertex;
  PDCELHalfEdge *_incident_halfedge;
public:
  PEdge() {}
  
};

#endif
