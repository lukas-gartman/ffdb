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

// Helper function to find the index of the first key greater than or equal to key
template <Comparable T>
int findKeyIndex(T key, auto &keys) {
    int idx = 0;
    while (idx < keys.size() && keys[idx] < key) {
        ++idx;
    }
    return idx;
}

template <Comparable T>
Node<T> *Node<T>::search(T key) {
    int i = findKeyIndex(key, keys);
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
Node<T> *BTree<T>::search(T key) {
    return root == nullptr ? nullptr : root->search(key);
}

template <Comparable T>
void BTree<T>::insert(T key) {
    auto r = root; // Start at the root
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

template <Comparable T>
T findPredecessor(Node<T> *node) {
    auto k_prime = node;
    while (!k_prime->is_leaf) {
        k_prime = k_prime->children[k_prime->keys.size()];
    }
    return k_prime->keys[k_prime->keys.size() - 1];
}

template <Comparable T>
T findSuccessor(Node<T> *node) {
    auto k_prime = node;
    while (!k_prime->is_leaf) {
        k_prime = k_prime->children[0];
    }
    return k_prime->keys[0];
}

template <Comparable T>
void mergeNodes(Node<T> *y, T k, Node<T> *z) {
    y->keys.push_back(k);
    y->keys.insert(y->keys.end(), z->keys.begin(), z->keys.end());
    if (!y->is_leaf) {
        y->children.insert(y->children.end(), z->children.begin(), z->children.end());
    }
}

template <Comparable T>
void moveKeyAndChildFromSibling(Node<T> *x, int i, bool isLeftSibling) {
    Node<T> *child = x->children[i];
    Node<T> *sibling = isLeftSibling ? x->children[i-1] : x->children[i+1];

    if (isLeftSibling) {
        // Shift all keys in the child node to the right
        for (int j = child->keys.size() - 1; j >= 0; --j) {
            child->keys[j+1] = child->keys[j];
        }
        if (!child->is_leaf) {
            // Shift all children in the child node to the right
            for (int j = child->children.size() - 1; j >= 0; --j) {
                child->children[j+1] = child->children[j];
            }
        }

        // Move the key from the current node down into the child node
        child->keys[0] = x->keys[i-1];
        if (!child->is_leaf) {
            // Move the child pointer from the left sibling up into the current node
            child->children[0] = sibling->children[sibling->children.size() - 1];
        }

        // Move the key from the left sibling up into the current node
        x->keys[i-1] = sibling->keys[sibling->keys.size() - 1];

        // Remove the key and child pointer from the left sibling
        sibling->keys.resize(sibling->keys.size() - 1);
        if (!sibling->is_leaf) {
            sibling->children.resize(sibling->children.size() - 1);
        }
    } else {
        // Move the key from the current node down into the child node
        child->keys[child->keys.size()] = x->keys[i];
        if (!child->is_leaf) {
            // Move the child pointer from the right sibling up into the current node
            child->children[child->children.size()] = sibling->children[0];
        }

        // Move the key from the right sibling up into the current node
        x->keys[i] = sibling->keys[0];

        // Shift all keys in the right sibling to the left
        for (int j = 1; j < sibling->keys.size(); ++j) {
            sibling->keys[j-1] = sibling->keys[j];
        }
        if (!sibling->is_leaf) {
            // Shift all children in the right sibling to the left
            for (int j = 1; j < sibling->children.size(); ++j) {
                sibling->children[j-1] = sibling->children[j];
            }
        }

        // Remove the key and child pointer from the right sibling
        sibling->keys.resize(sibling->keys.size() - 1);
        if (!sibling->is_leaf) {
            sibling->children.resize(sibling->children.size() - 1);
        }
    }

    // Update the size of the child node
    child->keys.resize(child->keys.size() + 1);
    if (!child->is_leaf) {
        child->children.resize(child->children.size() + 1);
    }
}

template <Comparable T>
void mergeChildWithSibling(Node<T> *x, int i, bool isLeftSibling) {
    Node<T> *child = x->children[i];
    Node<T> *sibling = isLeftSibling ? x->children[i-1] : x->children[i+1];

    if (isLeftSibling) {
        // Move the key from the current node down into the child node
        for (int j = child->keys.size() - 1; j >= 0; --j) {
            child->keys[j+1] = child->keys[j];
        }
        child->keys[0] = x->keys[i-1];

        // Move all keys from the left sibling into the child node
        for (int j = sibling->keys.size() - 1; j >= 0; --j) {
            child->keys[j+1] = sibling->keys[j];
        }
        if (!child->is_leaf) {
            // Move all children from the left sibling into the child node
            for (int j = sibling->children.size() - 1; j >= 0; --j) {
                child->children[j+1] = sibling->children[j];
            }
        }

        // Remove the key and child pointer from the current node
        for (int j = i; j < x->keys.size(); ++j) {
            x->keys[j-1] = x->keys[j];
        }
        for (int j = i + 1; j < x->children.size(); ++j) {
            x->children[j-1] = x->children[j];
        }

        // Delete the left sibling
        delete sibling;
    } else {
        // Move the key from the current node down into the child node
        child->keys[child->keys.size()] = x->keys[i];

        // Move all keys from the right sibling into the child node
        for (int j = 0; j < sibling->keys.size(); ++j) {
            child->keys[child->keys.size() + 1 + j] = sibling->keys[j];
        }
        if (!child->is_leaf) {
            // Move all children from the right sibling into the child node
            for (int j = 0; j < sibling->children.size(); ++j) {
                child->children[child->children.size() + 1 + j] = sibling->children[j];
            }
        }

        // Remove the key and child pointer from the current node
        for (int j = i + 1; j < x->keys.size(); ++j) {
            x->keys[j-1] = x->keys[j];
        }
        for (int j = i + 2; j < x->children.size(); ++j) {
            x->children[j-1] = x->children[j];
        }

        // Delete the right sibling
        delete sibling;
    }

    // Update the size of the current node
    x->keys.resize(x->keys.size() - 1);
    x->children.resize(x->children.size() - 1);

    // Update the size of the child node
    child->keys.resize(child->keys.size() + sibling->keys.size() + 1);
    if (!child->is_leaf) {
        child->children.resize(child->children.size() + sibling->children.size() + 1);
    }
}

template <Comparable T>
void BTree<T>::remove(T key) {
    int i = findKeyIndex(key, root->keys);

    auto k = root->keys[i];
    auto x = root;
    auto y = root->children[i];
    auto z = root->children[i+1];

    // Check if key is in the current node
    if (i < x->keys.size() && key == k) {
        if (x->is_leaf) {
            x->keys.erase(x->keys.begin() + i);
        } else {
            if (y->keys.size() >= t) {
                T predecessor = findPredecessor(y);
                x->remove(predecessor);
                k = predecessor;
            } else if (y->keys.size() < t) {
                if (z->keys.size() >= t) {
                    T successor = findSuccessor(z);
                    x->remove(successor);
                    k = successor;
                } else {
                    mergeNodes(y, k, z); // Merge y, k, and z into y
                    x->keys.erase(x->keys.begin() + i);
                    x->children.erase(x->children.begin() + i + 1);
                    delete z;
                    y->remove(key);
                }
            }
        }
    // The key is not in the current node, search in the node closest to the key
    } else {
        // If the key exists, it must be in the subtree rooted at the last child
        bool isLastChild = i == x->keys.size();

        // Check if child has minimum allowed (t-1) keys
        if (x->children[i]->keys.size() < t) {
            // Check if child node has an immediate sibling with at least t keys
            if (i != 0 && x->children[i-1]->keys.size() >= t) { // Left
                moveKeyAndChildFromSibling(x, i, true);
            } else if (i != x->keys.size() && x->children[i+1]->keys.size() >= t) { // Right
                moveKeyAndChildFromSibling(x, i, false);
            // If child node and both immediate siblings have t-1 keys
            } else {
                // Merge child node with one sibling
                if (i != x->keys.size()) {
                    mergeChildWithSibling(x, i, false);
                } else {
                    mergeChildWithSibling(x, i, true);
                }
            }
        }

        // Recurse on the appropriate child of the current node
        if (isLastChild && i > x->keys.size()) {
            x->children[i-1]->remove(key);
        } else {
            x->children[i]->remove(key);
        }
    }
}
