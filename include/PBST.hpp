#pragma once

#include "declarations.hpp"
#include "PDCELVertex.hpp"

#include <iostream>
#include <list>
#include <string>

namespace dcel { class PDCELVertex; }

// template <class T>
// class PBSTNode;

// template <class T>
// std::ostream &operator<< <>(std::ostream &, PBSTNode<T> *);

template <class T> class PBSTNode {
private:
  T *_data;
  PBSTNode<T> *_left;
  PBSTNode<T> *_right;
  PBSTNode<T> *_parent;
  int _height;

public:
  PBSTNode(T *data)
      : _data(data), _left(nullptr), _right(nullptr), _parent(nullptr),
        _height(0) {}

  // friend std::ostream &operator<< <>(std::ostream &, PBSTNode<T> *);

  T *data() { return _data; }
  PBSTNode *left() { return _left; }
  PBSTNode *right() { return _right; }
  PBSTNode *parent() { return _parent; }
  int height() { return _height; }
  int balance();

  void setData(T *data) { _data = data; }
  void setLeft(PBSTNode *node) { _left = node; }
  void setRight(PBSTNode *node) { _right = node; }
  void setParent(PBSTNode *node) { _parent = node; }
  void setHeight();
};




template <class T> std::ostream &operator<<(std::ostream &, PBSTNode<T> *);









/** @ingroup geo
 * An implementation of BST node for vertex.
 */
class PBSTNodeVertex {
private:
  dcel::PDCELVertex *_vertex;
  PBSTNodeVertex *_left;
  PBSTNodeVertex *_right;
  PBSTNodeVertex *_parent;
  int _height;

public:
  PBSTNodeVertex(dcel::PDCELVertex *vertex)
      : _vertex(vertex), _left(nullptr), _right(nullptr), _parent(nullptr),
        _height(0) {}

  friend std::ostream &operator<<(std::ostream &, PBSTNodeVertex *);

  dcel::PDCELVertex *vertex() { return _vertex; }
  PBSTNodeVertex *parent() const { return _parent; }
  PBSTNodeVertex *left() const { return _left; }
  PBSTNodeVertex *right() const { return _right; }
  int height() { return _height; }
  int balance();

  void setData(dcel::PDCELVertex *vertex) { _vertex = vertex; }
  void setParent(PBSTNodeVertex *parent) { _parent = parent; }
  void setLeft(PBSTNodeVertex *left) { _left = left; }
  void setRight(PBSTNodeVertex *right) { _right = right; }
  void setHeight();
};









/** @ingroup geo
 * An implementation of AVL tree speciallized for vertices.
 */
class PAVLTreeVertex {
private:
  PBSTNodeVertex *_root;

  // helper functions
  void printRecur(PBSTNodeVertex *, const int &);

  PBSTNodeVertex *insertNode(dcel::PDCELVertex *);
  PBSTNodeVertex *insertSubTree(PBSTNodeVertex *sub_root, dcel::PDCELVertex *vertex,
                                PBSTNodeVertex *parent = nullptr);

  PBSTNodeVertex *rotateLeft(PBSTNodeVertex *);
  PBSTNodeVertex *rotateRight(PBSTNodeVertex *);

  void rebalance(PBSTNodeVertex *);

public:
  PAVLTreeVertex() : _root(nullptr) {}

  PBSTNodeVertex *root() { return _root; }

  void printInOrder();
  PBSTNodeVertex *search(dcel::PDCELVertex *);
  void insert(dcel::PDCELVertex *);
  void remove(dcel::PDCELVertex *);

  /// Convert the tree into a list (in-order)
  std::list<PBSTNodeVertex *> toListInOrder();
};
