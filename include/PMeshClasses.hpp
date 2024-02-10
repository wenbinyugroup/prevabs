#pragma once

#include "declarations.hpp"
#include "PGeoClasses.hpp"
#include "utilities.hpp"

#include <stdio.h>
#include <vector>


class PNode {
private:
  int _id;
  PGeoPoint3 *_coords;

public:
  PNode() = default;
  PNode(int id, double x, double y, double z): _id(id) {
    _coords = new PGeoPoint3(x, y, z);
  }

  int getId() const { return _id; }
  PGeoPoint3 *getCoords() { return _coords; }
};




class PElement {
private:
  int _id;
  std::vector<int> _nodes;
  int _type_id;  // element type
  int _prop_id;  // material/section/layer type

public:
  PElement() = default;
  PElement(int id): _id(id) {}
  PElement(int id, std::vector<int> nids): _id(id), _nodes(nids) {}

  int getId() const { return _id; }
  std::vector<int> getNodes() const { return _nodes; }
  int getTypeId() const { return _type_id; }
  int getPropId() const { return _prop_id; }

  void setNodes(std::vector<int> nodes) { _nodes = nodes; }
  void setTypeId(int id) { _type_id = id; }
  void setPropId(int id) { _prop_id = id; }

  void addNode(int nid) { _nodes.push_back(nid); }
};




class PMesh {
private:
  std::vector<PNode *> _nodes;
  std::vector<PElement *> _elements;

public:
  PMesh() = default;

  std::vector<PNode *> getNodes() { return _nodes; }
  std::vector<PElement *> getElements() { return _elements; }

  void addNode(PNode *n) { _nodes.push_back(n); }
  void addElement(PElement *e) { _elements.push_back(e); }

  void writeGmshMsh(FILE *, Message *);
};
