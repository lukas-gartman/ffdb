// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)

#include "BTree.h"

template <Comparable T>
Node<T>::Node(int t, bool is_leaf) {
    this->t = t; // Minimum degree, the lower bound
    this->is_leaf = is_leaf;

    // CLRS B-tree properties:
    // 1. Every node has at most 2t-1 keys and at most 2t children
    // 2. Every node (except the root) has at least t-1 keys and at least t children
    keys.reserve(2*t - 1); // preallocate space for keys' upper bound
    if (!is_leaf) {
        children.reserve(2*t); // we can have at most 2t children
    }
}

template <Comparable T>
Node<T> *Node<T>::search(T key) {
    // Check if the current node contains the key
    int i = 0;
    while (i < keys.size() && key > keys[i]) {
        i++;
    }

    return i <= keys.size() && key == keys[i] ? this // If the key is found in the current node
       : is_leaf ? nullptr // Nothing further to search
       : children[i]->search(key); // Look further in the child node
}

template <Comparable T>
BTree<T>::BTree(int t) {
    this->t =  t;
    root = new Node<T>(t, true);
}
