#include "PDCELValidator.hpp"
#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"
#include "plog.hpp"

#include <algorithm>
#include <set>

// Define the static member
std::string PDCELValidator::_lastError;

bool PDCELValidator::validateDCEL(const PDCEL* dcel) {
    if (!dcel) {
        _lastError = "DCEL is null";
        return false;
    }

    // Validate all vertices
    // for (const auto& vertex : dcel->vertices()) {
    //     if (!validateVertex(vertex)) {
    //         PLOG(error) << "Invalid vertex found: " << _lastError;
    //         return false;
    //     }
    // }

    // Validate all half-edges
    // for (const auto& edge : dcel->halfedges()) {
    //     if (!validateHalfEdge(edge)) {
    //         PLOG(error) << "Invalid half-edge found: " << _lastError;
    //         return false;
    //     }
    // }

    // Validate all loops
    for (const auto& loop : dcel->halfedgeloops()) {
        if (!validateLoop(loop)) {
            PLOG(error) << "Invalid loop found: " << _lastError;
            return false;
        }
    }

    // Validate all faces
    for (const auto& face : dcel->faces()) {
        if (!validateFace(face)) {
            PLOG(error) << "Invalid face found: " << _lastError;
            return false;
        }
    }

    return true;
}

bool PDCELValidator::validateFace(const PDCELFace* face) {
    if (!face) {
        _lastError = "Face is null";
        return false;
    }

    // Validate outer component
    if (!face->outer()) {
        _lastError = "Face has no outer component";
        return false;
    }

    // Validate outer loop
    if (!face->outer()->loop()) {
        _lastError = "Face's outer half-edge has no loop";
        return false;
    }

    // Validate loop-face linkage
    if (!validateLoopFaceLinkage(face->outer()->loop(), face)) {
        return false;
    }

    // Validate inner loops
    if (!validateInnerLoops(face)) {
        return false;
    }

    return true;
}

bool PDCELValidator::validateLoop(const PDCELHalfEdgeLoop* loop) {
    if (!loop) {
        _lastError = "Loop is null";
        return false;
    }

    // Validate incident edge
    if (!loop->incidentEdge()) {
        _lastError = "Loop has no incident edge";
        return false;
    }

    // Validate cycle
    if (!validateCycle(loop->incidentEdge())) {
        _lastError = "Loop does not form a valid cycle";
        return false;
    }

    // Validate face linkage
    if (loop->face() && !validateLoopFaceLinkage(loop, loop->face())) {
        return false;
    }

    return true;
}

bool PDCELValidator::validateHalfEdge(const PDCELHalfEdge* edge) {
    if (!edge) {
        _lastError = "Half-edge is null";
        return false;
    }

    // Validate source vertex
    if (!edge->source()) {
        _lastError = "Half-edge has no source vertex";
        return false;
    }

    // Validate twin
    if (!edge->twin()) {
        _lastError = "Half-edge has no twin";
        return false;
    }

    // Validate twin's source is this edge's target
    if (edge->twin()->source() != edge->target()) {
        _lastError = "Half-edge twin source does not match target";
        return false;
    }

    // Validate next/prev pointers
    if (edge->next() && edge->next()->prev() != edge) {
        _lastError = "Half-edge next->prev does not point back";
        return false;
    }

    if (edge->prev() && edge->prev()->next() != edge) {
        _lastError = "Half-edge prev->next does not point forward";
        return false;
    }

    // Validate loop linkage
    if (edge->loop() && edge->loop()->incidentEdge() != edge) {
        _lastError = "Half-edge loop linkage is invalid";
        return false;
    }

    // Validate face linkage
    if (edge->face() && edge->face()->outer() != edge) {
        bool found = false;
        for (const auto& inner : edge->face()->inners()) {
            if (inner == edge) {
                found = true;
                break;
            }
        }
        if (!found) {
            _lastError = "Half-edge not found in face's outer or inner components";
            return false;
        }
    }

    return true;
}

bool PDCELValidator::validateVertex(const PDCELVertex* vertex) {
    if (!vertex) {
        _lastError = "Vertex is null";
        return false;
    }

    // Validate incident edge
    if (!vertex->edge()) {
        _lastError = "Vertex has no incident edge";
        return false;
    }

    // Validate vertex cycle
    if (!validateVertexCycle(vertex)) {
        _lastError = "Vertex's incident edges do not form a valid cycle";
        return false;
    }

    return true;
}

bool PDCELValidator::validateCycle(const PDCELHalfEdge* start) {
    if (!start) {
        _lastError = "Cycle start is null";
        return false;
    }

    std::set<const PDCELHalfEdge*> visited;
    const PDCELHalfEdge* current = start;

    do {
        if (!current) {
            _lastError = "Cycle contains null half-edge";
            return false;
        }

        if (visited.find(current) != visited.end()) {
            _lastError = "Cycle contains duplicate half-edge";
            return false;
        }

        visited.insert(current);
        current = current->next();
    } while (current != start);

    return true;
}

bool PDCELValidator::validateLoopFaceLinkage(const PDCELHalfEdgeLoop* loop, const PDCELFace* face) {
    if (!loop || !face) {
        _lastError = "Loop or face is null";
        return false;
    }

    // Check if loop is properly linked to face
    if (loop->face() != face) {
        _lastError = "Loop not properly linked to face";
        return false;
    }

    // Check if all half-edges in loop point to the face
    const PDCELHalfEdge* start = loop->incidentEdge();
    const PDCELHalfEdge* current = start;

    do {
        if (current->face() != face) {
            _lastError = "Half-edge in loop not properly linked to face";
            return false;
        }
        current = current->next();
    } while (current != start);

    return true;
}

bool PDCELValidator::validateInnerLoops(const PDCELFace* face) {
    if (!face) {
        _lastError = "Face is null";
        return false;
    }

    // Validate each inner loop
    for (const auto& inner : face->inners()) {
        if (!inner) {
            _lastError = "Face has null inner component";
            return false;
        }

        if (!inner->loop()) {
            _lastError = "Inner component has no loop";
            return false;
        }

        if (!validateLoopFaceLinkage(inner->loop(), face)) {
            return false;
        }
    }

    return true;
}

bool PDCELValidator::validateVertexCycle(const PDCELVertex* vertex) {
    if (!vertex) {
        _lastError = "Vertex is null";
        return false;
    }

    std::set<const PDCELHalfEdge*> visited;
    const PDCELHalfEdge* start = vertex->edge();
    const PDCELHalfEdge* current = start;

    do {
        if (!current) {
            _lastError = "Vertex cycle contains null half-edge";
            return false;
        }

        if (visited.find(current) != visited.end()) {
            _lastError = "Vertex cycle contains duplicate half-edge";
            return false;
        }

        if (current->source() != vertex) {
            _lastError = "Half-edge in vertex cycle has wrong source";
            return false;
        }

        visited.insert(current);
        current = current->twin()->next();
    } while (current != start);

    return true;
}
