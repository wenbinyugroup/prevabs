#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <iostream>
#include <fstream>


PDCELHalfEdgeLoop::PDCELHalfEdgeLoop() {
  PLOG(debug) << "new half edge loop created";
}


// // Overload the == operator
// bool PDCELHalfEdgeLoop::operator==(PDCELHalfEdgeLoop *other) const {

//   if (_direction != other->direction()) {
//     PLOG(debug) << "direction mismatch";
//     return false;
//   }

//   if (_bottom_left_vertex != other->bottomLeftVertex()) {
//     PLOG(debug) << "bottom left vertex mismatch";
//     return false;
//   }

//   if (_incident_edge != other->incidentEdge()) {
//     PLOG(debug) << "incident edge mismatch";
//     return false;
//   }

//   // if (_adjacent_loop != other->adjacentLoop()) {
//   //   return false;
//   // }

//   // if (_keep != other->keep()) {
//   //   return false;
//   // }

//   // if (_face != other->face()) {
//   //   return false;
//   // }

//   return true;
// // }


// bool PDCELHalfEdgeLoop::operator!=(PDCELHalfEdgeLoop *other) const {
//   return !(*this == other);
// }


void PDCELHalfEdgeLoop::log() {

  PLOG(trace) << "direction: " << ((direction() == 1) ? "outer" : "inner");

  PLOG(trace) << "keep: " << (_keep ? "yes" : "no");

  PLOG(trace) << "face: " << (_face ? _face->name() : "nullptr");


  PDCELHalfEdge *he = _incident_edge;

  PLOG(trace) << "half edges:";
  do {
    // he->print2();
    PLOG(trace) << he->printString();
    he = he->next();
  } while (he != _incident_edge);


  PLOG(trace) << "adjacent loop:" << (_adjacent_loop ? _adjacent_loop->incidentEdge()->printBrief() : "nullptr");

}


void PDCELHalfEdgeLoop::print() {
  std::cout << "direction: ";
  if (direction() == 1) {
    std::cout << "outer" << std::endl;
  } else if (direction() == -1) {
    std::cout << "inner" << std::endl;
  }

  std::cout << "keep: ";
  if (_keep) {
    std::cout << "yes" << std::endl;
  } else {
    std::cout << "no" << std::endl;
  }

  std::cout << "face: ";
  if (_face != nullptr) {
    std::cout << _face->name() << std::endl;
  } else {
    std::cout << "nullptr" << std::endl;
  }

  PDCELHalfEdge *he = _incident_edge;
  std::cout << "half edges:" << std::endl;
  do {
    he->print2();
    he = he->next();
  } while (he != _incident_edge);
  // std::cout << std::endl;

  std::cout << "adjacent loop:";
  if (_adjacent_loop == nullptr) {
    std::cout << " nullptr" << std::endl;
  }
  else {
    _adjacent_loop->incidentEdge()->print();
  }

  std::cout << std::endl;
}


int PDCELHalfEdgeLoop::direction() {
  if (_direction == 0) {
    SVector3 sv1, sv2, sv0;
    sv1 = _incident_edge->prev()->toVector();
    sv2 = _incident_edge->toVector();

    sv0 = crossprod(sv1, sv2);
    _direction = sv0.x() > 0 ? 1 : -1;

    // return sv0.x() > 0 ? 1 : -1;
  }
  return _direction;
}


void PDCELHalfEdgeLoop::setFace(PDCELFace *f) {
  _face = f;
  _incident_edge->setIncidentFace(f);
}


void PDCELHalfEdgeLoop::updateVertexEdge(PDCELHalfEdge *he) {
  PLOG(trace) << "updateVertexEdge: " << he;

  if (_incident_edge == nullptr) {
    PLOG(trace) << "  setting incident edge for the first time";
    _incident_edge = he;
    _bottom_left_vertex = he->source();
  } else if (he->source()->point() < _bottom_left_vertex->point()) {
    PLOG(trace) << "  setting incident edge to a smaller one";
    _incident_edge = he;
    _bottom_left_vertex = he->source();
  }

  PLOG(trace) << "done";
}


std::vector<PDCELVertex *> PDCELHalfEdgeLoop::vertices() {
  std::vector<PDCELVertex *> vertices;
  PDCELHalfEdge *he = _incident_edge;
  vertices.push_back(he->source()); 
  do {
    vertices.push_back(he->target());
    he = he->next();
  } while (he != _incident_edge);
  return vertices;
}


void PDCELHalfEdgeLoop::write_to_file(std::ofstream& file) {
  PLOG(debug) << "Starting to write half edge loop to file";

  try {
    PLOG(debug) << "Writing direction information";
    file << "direction: ";
    if (direction() == 1) {
      file << "outer" << std::endl;
    } else if (direction() == -1) {
      file << "inner" << std::endl;
    } else {
      PLOG(warning) << "Unexpected direction value: " << direction();
      file << "unknown" << std::endl;
    }

    PLOG(debug) << "Writing keep status";
    file << "keep: ";
    if (_keep) {
      file << "yes" << std::endl;
    } else {
      file << "no" << std::endl;
    }

    PLOG(debug) << "Writing face information";
    file << "face: ";
    if (_face != nullptr) {
      file << _face->name() << std::endl;
    } else {
      PLOG(debug) << "Face is nullptr";
      file << "nullptr" << std::endl;
    }

    PLOG(debug) << "Starting to write half edges";
    PDCELHalfEdge *he = _incident_edge;
    if (he == nullptr) {
      PLOG(error) << "Incident edge is nullptr";
      throw std::runtime_error("Incident edge is nullptr");
    }

    file << "half edges:" << std::endl;
    int edge_count = 0;
    do {
      PLOG(debug) << "Writing half edge " << edge_count;
      he->write_to_file(file);
      he = he->next();
      edge_count++;
      
      if (edge_count > 1000) { // Prevent infinite loops
        PLOG(error) << "Too many edges in loop, possible circular reference";
        throw std::runtime_error("Too many edges in loop");
      }
    } while (he != _incident_edge);
    PLOG(debug) << "Wrote " << edge_count << " half edges";

    PLOG(debug) << "Writing adjacent loop information";
    file << "adjacent loop:";
    if (_adjacent_loop == nullptr) {
      PLOG(debug) << "Adjacent loop is nullptr";
      file << " nullptr" << std::endl;
    }
    else {
      if (_adjacent_loop->incidentEdge() == nullptr) {
        PLOG(error) << "Adjacent loop's incident edge is nullptr";
        throw std::runtime_error("Adjacent loop's incident edge is nullptr");
      }
      _adjacent_loop->incidentEdge()->write_to_file(file);
    }

    file << std::endl;
    PLOG(debug) << "Successfully completed writing half edge loop to file";
  } catch (const std::exception& e) {
    PLOG(error) << "Error while writing half edge loop to file: " << e.what();
    throw; // Re-throw to maintain original error handling
  } catch (...) {
    PLOG(error) << "Unknown error while writing half edge loop to file";
    throw;
  }
}
