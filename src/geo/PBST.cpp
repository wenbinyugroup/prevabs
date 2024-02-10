#include "PBST.hpp"
#include "PDCELVertex.hpp"

#include <algorithm>
#include <iostream>
#include <list>
#include <string>

template <class T>
std::ostream &operator<<(std::ostream &out, PBSTNode<T> *node) {
  out << node->data();
  return out;
}

template <class T> int PBSTNode<T>::balance() {
  int hl = _left == nullptr ? -1 : _left->height();
  int hr = _right == nullptr ? -1 : _right->height();
  return hr - hl;
}

template <class T> void PBSTNode<T>::setHeight() {
  int hl = _left == nullptr ? -1 : _left->height();
  int hr = _right == nullptr ? -1 : _right->height();
  _height = std::max(hl, hr) + 1;
}

std::ostream &operator<<(std::ostream &out, PBSTNodeVertex *node) {
  out << node->_vertex;
  return out;
}

int PBSTNodeVertex::balance() {
  int hl = _left == nullptr ? -1 : _left->height();
  int hr = _right == nullptr ? -1 : _right->height();
  return hr - hl;
}

void PBSTNodeVertex::setHeight() {
  int hl = _left == nullptr ? -1 : _left->height();
  int hr = _right == nullptr ? -1 : _right->height();
  _height = std::max(hl, hr) + 1;
}


void PAVLTreeVertex::printInOrder() {
  std::cout << "printing AVL tree nodes in-order" << std::endl;
  printRecur(_root, 1);

  // std::cout << std::endl;
  // std::list<PBSTNodeVertex *> nlist = toListInOrder();
  // for (auto n : nlist) {
  //   std::cout << n << std::endl;
  // }
}

PBSTNodeVertex *PAVLTreeVertex::search(PDCELVertex *vertex) {
  PBSTNodeVertex *node = _root;

  while (node != nullptr) {
    if (node->vertex() == vertex) {
      break;
    } else if (vertex->point() < node->vertex()->point()) {
      node = node->left();
      continue;
    } else if (node->vertex()->point() < vertex->point()) {
      node = node->right();
    }
  }

  return node;
}

void PAVLTreeVertex::insert(PDCELVertex *vertex) {
  // std::cout << "inserting vertex " << vertex << " into the tree" <<
  // std::endl;

  // ordinary BST insert
  PBSTNodeVertex *node;
  node = insertNode(vertex);

  // std::cout << "node " << node << std::endl;

  // Go up for each parent, check balance and rotate if necessary
  node = node->parent();
  while (node != nullptr) {
    rebalance(node);

    node = node->parent();
  }
}

void PAVLTreeVertex::remove(PDCELVertex *vertex) {
  PBSTNodeVertex *node_to_remove, *retrace_start = nullptr;
  node_to_remove = search(vertex);

  if (node_to_remove->left() == nullptr && node_to_remove->right() == nullptr) {
    // Leaf node
    if (node_to_remove == _root) {
      _root = nullptr;
    } else {
      if (node_to_remove == node_to_remove->parent()->left()) {
        node_to_remove->parent()->setLeft(nullptr);
      } else if (node_to_remove == node_to_remove->parent()->right()) {
        node_to_remove->parent()->setRight(nullptr);
      }
      retrace_start = node_to_remove->parent();
      delete node_to_remove;
    }
  } else if (node_to_remove->left() != nullptr &&
             node_to_remove->right() == nullptr) {
    // Left child only
    // Replace the data, instead of removing the node
    node_to_remove->setData(node_to_remove->left()->vertex());
    node_to_remove->setLeft(nullptr);
    retrace_start = node_to_remove;
  } else if (node_to_remove->left() == nullptr &&
             node_to_remove->right() != nullptr) {
    // Right child only
    node_to_remove->setData(node_to_remove->right()->vertex());
    node_to_remove->setRight(nullptr);
    retrace_start = node_to_remove;
  } else {
    PBSTNodeVertex *successor_inorder = node_to_remove;
    do {
      successor_inorder = node_to_remove->left();
    } while (successor_inorder->left() != nullptr);

    node_to_remove->setData(successor_inorder->vertex());
    if (successor_inorder->right() != nullptr) {
      successor_inorder->setData(successor_inorder->right()->vertex());
      successor_inorder->setRight(nullptr);
      retrace_start = successor_inorder;
    } else {
      successor_inorder->parent()->setLeft(nullptr);
      retrace_start = successor_inorder->parent();
    }
  }

  // Update the hight and rebalance the tree if necessary
  while (retrace_start != nullptr) {
    retrace_start->setHeight();

    // Rebalance if necessary
    rebalance(retrace_start);

    retrace_start = retrace_start->parent();
  }
}

std::list<PBSTNodeVertex *> PAVLTreeVertex::toListInOrder() {
  std::list<PBSTNodeVertex *> nlist, ntemp;

  PBSTNodeVertex *node = _root;
  while (!ntemp.empty() || node != nullptr) {
    if (node != nullptr) {
      ntemp.push_back(node);
      node = node->left();
    } else {
      node = ntemp.back();
      ntemp.pop_back();

      nlist.push_back(node);

      node = node->right();
    }
  }

  return nlist;
}

// ===================================================================
//
// Private helper functions
//
// ===================================================================

void PAVLTreeVertex::printRecur(PBSTNodeVertex *node, const int &order) {
  // 1: In-order
  // 2: Pre-order
  // 3: Post-order
  if (node == nullptr)
    return;

  if (order == 1) {
    printRecur(node->left(), order);
    std::cout << node << std::endl;
    printRecur(node->right(), order);
  }
}

PBSTNodeVertex *PAVLTreeVertex::insertNode(PDCELVertex *vertex) {
  PBSTNodeVertex *current = _root, *parent = nullptr;
  std::string side;

  while (current != nullptr) {
    parent = current;
    // if (&(vertex) < &(current->vertex())) {
    if (vertex->point() < current->vertex()->point()) {
      current = current->left();
      side = "left";
    } else if (current->vertex()->point() < vertex->point()) {
      current = current->right();
      side = "right";
    } else if (vertex->point() == current->vertex()->point()) {
      return current;
    }
  }

  current = new PBSTNodeVertex(vertex);
  current->setParent(parent);
  if (parent != nullptr) {
    if (side == "left") {
      parent->setLeft(current);
    } else if (side == "right") {
      parent->setRight(current);
    }
  }

  while (parent != nullptr) {
    parent->setHeight();
    parent = parent->parent();
  }

  if (_root == nullptr) {
    _root = current;
  }

  return current;
}

// PBSTNodeVertex *PAVLTreeVertex::insertSubTree(PBSTNodeVertex *sub_root,
// PDCELVertex *vertex, PBSTNodeVertex *parent) {
//   if (sub_root == nullptr) {
//     sub_root = new PBSTNodeVertex(vertex);
//     sub_root->setParent(parent);
//   } else if (&vertex < &(sub_root->vertex)) {
//     sub_root->setLeft(insertSubTree(sub_root->left(), vertex, sub_root));
//     sub_root->setHeight();
//   } else if (&vertex > &(sub_root->vertex)) {
//     sub_root->setRight(insertSubTree(sub_root->right(), vertex, sub_root));
//     sub_root->setHeight();
//   }

//   return sub_root;
// }

PBSTNodeVertex *PAVLTreeVertex::rotateLeft(PBSTNodeVertex *x) {
  PBSTNodeVertex *xp = x->parent();
  PBSTNodeVertex *xr = x->right();
  PBSTNodeVertex *xrl = xr->left();

  if (xp != nullptr) {
    if (x == xp->left()) {
      xp->setLeft(xr);
    } else if (x == xp->right()) {
      xp->setRight(xr);
    }
  } else {
    _root = xr;
  }

  xr->setLeft(x);
  x->setRight(xrl);

  if (xrl != nullptr) {
    xrl->setParent(x);
  }
  x->setParent(xr);
  xr->setParent(xp);

  x->setHeight();
  xr->setHeight();

  return xr;
}

PBSTNodeVertex *PAVLTreeVertex::rotateRight(PBSTNodeVertex *x) {
  PBSTNodeVertex *xp = x->parent();
  PBSTNodeVertex *xl = x->left();
  PBSTNodeVertex *xlr = xl->right();

  if (xp != nullptr) {
    if (x == xp->left()) {
      xp->setLeft(xl);
    } else if (x == xp->right()) {
      xp->setRight(xl);
    }
  } else {
    _root = xl;
  }

  xl->setRight(x);
  x->setLeft(xlr);

  if (xlr != nullptr) {
    xlr->setParent(x);
  }
  x->setParent(xl);
  xl->setParent(xp);

  x->setHeight();
  xl->setHeight();

  return xl;
}

void PAVLTreeVertex::rebalance(PBSTNodeVertex *node) {
  // std::cout << "balance = " << node->balance() << std::endl;
  // Four cases
  if (node->balance() < -1) {
    // Left heavy
    if (node->left()->balance() > 0) {
      // Case 1: Left-Right rotation
      rotateLeft(node->left());
      rotateRight(node);
    } else {
      // Case 2: Right rotation
      rotateRight(node);
    }
  } else if (node->balance() > 1) {
    // Right heavy
    if (node->right()->balance() < 0) {
      // Case 3: Right-Left rotation
      rotateRight(node->right());
      rotateLeft(node);
    } else {
      // Case 4: Left rotation
      rotateLeft(node);
    }
  }
}
