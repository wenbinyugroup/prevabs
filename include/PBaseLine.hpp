#pragma once

#include "declarations.hpp"
#include "CrossSection.hpp"
#include "globalConstants.hpp"
#include "Material.hpp"
#include "PDCELVertex.hpp"
#include "utilities.hpp"
#include "gmsh_mod/SVector3.h"

#include <list>
#include <string>
#include <vector>


class PDCELVertex;
class CrossSection;
class Message;


/** @ingroup cs
 * A cross-sectional line class.
 */
class Baseline {
private:
  std::string blname;
  std::string bltype;
  int blnumbasepoints;
  std::vector<PDCELVertex *> _pvertices;
  PDCELVertex *_ref_vertex = nullptr;

public:

  /**
   * A constructor.
   */
  Baseline() {}

  /**
   * A constructor.
   */
  Baseline(std::string name, std::string type)
      : blname(name), bltype(type) {}

  /**
   * A copy constructor.
   */
  Baseline(Baseline *);




  /**
   * Print out a summary.
   */
  void print(Message *);
  // void printBaseline(); // Print details









  /**
   * Get the name of the line.
   */
  std::string getName() const { return blname; }

  /**
   * Get the type of the line.
   */
  std::string getType() const { return bltype; }

  int getNumberOfBasepoints() const { return blnumbasepoints; }

  /**
   * Get the list of vertices of the line.
   */
  std::vector<PDCELVertex *> &vertices() { return _pvertices; }

  /**
   * Get the reference vertex of the line.
   */
  PDCELVertex *refVertex() { return _ref_vertex; }









  /**
   * Is the line closed or open.
   */
  bool isClosed() const { return _pvertices.front() == _pvertices.back(); }

  SVector3 getTangentVectorBegin();
  SVector3 getTangentVectorEnd();









  /**
   * Set the name of the line.
   */
  void setName(std::string name) { blname = name; }

  /**
   * Set the type of the line.
   */
  void setType(std::string type) { bltype = type; }

  int topology();
  void reverse();

  /**
   * Append new vertex to the line.
   */
  void addPVertex(PDCELVertex *);

  /**
   * Insert new vertex into the line.
   */
  void insertPVertex(const int &, PDCELVertex *);

  /**
   * Set the reference vertex.
   */
  void setRefVertex(PDCELVertex *rv) { _ref_vertex = rv; }

  /**
   * Join another curve to this one.
   * 
   * @param[in] curve The curve that will be joined to this one
   * @param[in] end The end of this curve that will be joined to (0: head, 1: tail)
   * @param[in] reverse If reverse the direction of the other curve before joinning
   */
  void join(Baseline *curve, int end, bool reverse);
};

