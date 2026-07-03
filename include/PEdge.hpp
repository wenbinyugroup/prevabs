#pragma once

#include "PDCELVertex.hpp"
#include "PDCELHalfEdge.hpp"

class PEdge {
private:
  dcel::PDCELVertex *_incident_vertex;
  dcel::PDCELHalfEdge *_incident_halfedge;
public:
  PEdge() {}
  
};

#endif
