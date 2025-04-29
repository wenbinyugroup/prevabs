#pragma once

#include "PDCEL.hpp"
#include "PDCELFace.hpp"
#include "PDCELHalfEdge.hpp"
#include "PDCELHalfEdgeLoop.hpp"
#include "PDCELVertex.hpp"

#include <string>
#include <vector>

/**
 * @class PDCELValidator
 * @brief A class for validating the integrity of a PDCEL structure
 */
class PDCELValidator {
public:
    /**
     * @brief Validate the entire DCEL structure
     * @param dcel The DCEL to validate
     * @return true if the DCEL is valid, false otherwise
     */
    static bool validateDCEL(const PDCEL* dcel);

    /**
     * @brief Validate a face and its components
     * @param face The face to validate
     * @return true if the face is valid, false otherwise
     */
    static bool validateFace(const PDCELFace* face);

    /**
     * @brief Validate a half-edge loop and its components
     * @param loop The loop to validate
     * @return true if the loop is valid, false otherwise
     */
    static bool validateLoop(const PDCELHalfEdgeLoop* loop);

    /**
     * @brief Validate a half-edge and its relationships
     * @param edge The half-edge to validate
     * @return true if the half-edge is valid, false otherwise
     */
    static bool validateHalfEdge(const PDCELHalfEdge* edge);

    /**
     * @brief Validate a vertex and its incident edges
     * @param vertex The vertex to validate
     * @return true if the vertex is valid, false otherwise
     */
    static bool validateVertex(const PDCELVertex* vertex);

    /**
     * @brief Get the last validation error message
     * @return The error message
     */
    static std::string getLastError() { return _lastError; }

private:
    static std::string _lastError;

    /**
     * @brief Check if a half-edge forms a valid cycle
     * @param start The starting half-edge
     * @return true if the cycle is valid, false otherwise
     */
    static bool validateCycle(const PDCELHalfEdge* start);

    /**
     * @brief Check if a half-edge loop is properly linked to its face
     * @param loop The loop to check
     * @param face The face to check against
     * @return true if the linkage is valid, false otherwise
     */
    static bool validateLoopFaceLinkage(const PDCELHalfEdgeLoop* loop, const PDCELFace* face);

    /**
     * @brief Check if a face's inner loops are properly contained
     * @param face The face to check
     * @return true if the inner loops are valid, false otherwise
     */
    static bool validateInnerLoops(const PDCELFace* face);

    /**
     * @brief Check if a vertex's incident edges form a valid cycle
     * @param vertex The vertex to check
     * @return true if the cycle is valid, false otherwise
     */
    static bool validateVertexCycle(const PDCELVertex* vertex);
}; 