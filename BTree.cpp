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

    return i < keys.size() && key == keys[i] ? this // If the key is found in the current node
       : is_leaf ? nullptr // Nothing further to search
       : children[i]->search(key); // Look further in the child node
}

template <Comparable T>
void Node<T>::insertNonFull(T key) {
    int i = keys.size() - 1;
    if (is_leaf) { // If the current node is a leaf, insert the key
        // Find the key that is less than the key to be inserted
        while (i >= 0 && key < keys[i]) {
            keys[i+1] = keys[i]; // Shift the key to the right
            --i;
        }

        keys[i+1] = key; // Insert the key next to the key that is less than it
    } else { // Insert the key into the appropriate child node
        // Find the child node that the key should be inserted into
        while (i >= 0 && key < keys[i]) {
            --i;
        }

        ++i; // Increment i to get the child node index

        // Check if the child node is full
        if (children[i]->keys.size() == 2*t - 1) {
            splitChild(i, children[i]);
            // Determine which of the two children to insert the key into
            if (key > keys[i]) {
                ++i;
            }
        }

        children[i]->insertNonFull(key);
    }
}

template <Comparable T>
void Node<T>::splitChild(int i, Node *y) {
    Node *z = new Node(y->t, y->is_leaf);
    z->keys.resize(t - 1); // // z will have t-1 keys

    // Copy the upper half of y's keys to z's lower half
    for (int j = 0; j < t - 1; ++j) {
        z->keys[j] = y->keys[j + t];
    }

    if (!y->is_leaf) {
        // Copy the upper half of y's children to z's lower half
        for (int j = 0; j < t; ++j) {
            z->children[j] = y->children[j + t];
        }
    }

    // Remove the upper half of y's keys that were copied to z
    y->keys.resize(t - 1);

    // Move the children to the right to make space for z
    for (int j = keys.size(); j >= i+1; --j) {
        children[j+1] = children[j];
    }
    // Insert z as a child to the free space we just made
    children[i+1] = z;

    // Move the keys to the right to make space for the middle key of y
    for (int j = keys.size() - 1; j >= i; --j) {
        keys[j+1] = keys[j];
    }
    // Insert the middle key of y into the current node
    keys[i] = y->keys[t - 1];
    // Increase the number of keys in the current node
    keys.resize(keys.size() + 1);
}

template <Comparable T>
BTree<T>::BTree(int t) {
    this->t =  t;
    root = new Node<T>(t, true);
}

template <Comparable T>
void BTree<T>::insert(T key) {
    Node<T> *r = root; // Start at the root
    // If the root is full, split it
    if (r->keys.size() == 2*t - 1) {
        // Create a new root
        auto *s = new Node<T>(t, false);
        root = s;

        s->children[0] = r; // Make the old root the child of the new root
        s->splitChild(0, r); // Split the old root to ensure it is no longer full
        s->insertNonFull(key);
    } else {
        r->insertNonFull(key);
    }
}

