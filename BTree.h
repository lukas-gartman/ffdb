// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)

#ifndef BTREE_H
#define BTREE_H

#include <concepts>

// Ensure the key type is comparable to other keys
template <typename T>
concept Comparable = requires(T a, T b) {
    { a < b }  -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b }  -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

template <Comparable T, int t>
class BTree;

template <Comparable T, int t>
class Node {
private:
    T keys[2*t-1];
    Node *children[2*t];
    int n; // Number of keys in the node
    bool isLeaf;

public:
    explicit Node(const bool isLeaf) : n(0), isLeaf(isLeaf) {
        for (int i = 0; i < 2*t-1; ++i) {
            keys[i] = T();
        }
        for (int i = 0; i < 2*t; ++i) {
            children[i] = nullptr;
        }
    }

    friend class BTree<T, t>;
};

template <Comparable T, int t>
class BTree {
private:
    Node<T, t> *root;

    Node<T, t> *search(Node<T, t> *x, T key) {
        // Find the index of the first key greater than or equal to key
        int i = 0;
        while (i < x->n && key > x->keys[i]) {
            i++;
        }

        return i < x->n && key == x->keys[i] ? x // Key found
            : x->isLeaf ? nullptr // Key not found
            : search(x->children[i], key); // Search in the child
    }

    void splitChild(Node<T, t> *x, int i) {
        assert(x != nullptr);
        assert(i >= 0 && i <= x->n);
        assert(x->children[i] != nullptr);
        assert(x->children[i]->n == 2 * t - 1);

        auto *y = x->children[i];
        auto *z = new Node<T, t>(y->isLeaf);
        z->n = t - 1;

        // Copy the last t-1 keys of y to z
        for (int j = 0; j < t - 1; j++) {
            z->keys[j] = y->keys[j + t];
        }

        // Copy the last t children of y to z
        if (!y->isLeaf) {
            for (int j = 0; j < t; j++) {
                z->children[j] = y->children[j + t];
            }
        }

        // Store the middle key before reducing y's size
        T middleKey = y->keys[t - 1];

        // Reduce the number of keys in y
        y->n = t - 1;

        // Make room for the new child in x
        for (int j = x->n; j >= i + 1; --j) {
            x->children[j + 1] = x->children[j];
        }

        // Insert the new child into x
        x->children[i + 1] = z;

        // Make room for the new key in x
        for (int j = x->n - 1; j >= i; --j) {
            x->keys[j + 1] = x->keys[j];
        }

        // Insert the middle key of y into x
        x->keys[i] = middleKey;
        ++x->n;

        assert(x->n <= 2 * t - 1);
        assert(y->n == t - 1);
        assert(z->n == t - 1);
    }

    void insertNonFull(Node<T, t> *x, T key) {
        assert(x != nullptr);
        assert(x->n < 2 * t - 1);
        assert(x->isLeaf || x->children[0] != nullptr);

        // Find the position to insert the new key, starting from the right
        int i = x->n - 1;
        if (x->isLeaf) {
            // Shift keys to the right to make room for the new key
            while (i >= 0 && key < x->keys[i]) {
                x->keys[i + 1] = x->keys[i];
                --i;
            }

            // if (x->keys[i] == key) { return; } // Key already exists, do not insert

            // Insert the new key
            x->keys[i + 1] = key;
            x->n = x->n + 1;
        } else {
            // Find the child to insert the new key
            while (i >= 0 && key < x->keys[i]) {
                --i;
            }

            ++i; // Increment i to find the child
            assert(x->children[i] != nullptr);

            // Split the child if it is full
            if (x->children[i]->n == 2*t - 1) {
                splitChild(x, i);

                // Determine which of the two children to insert the new key into
                if (key > x->keys[i]) {
                    ++i;
                }
            }

            // Insert the new key into the appropriate child
            insertNonFull(x->children[i], key);
        }
    }

    T getInOrderPredecessor(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);
        assert(node->children[i] != nullptr);

        // Find the rightmost child of the left child
        auto *current = node->children[i];
        while (!current->isLeaf) {
            current = current->children[current->n];
        }
        return current->keys[current->n - 1];
    }

    T getInOrderSuccessor(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);
        assert(node->children[i + 1] != nullptr);

        // Find the leftmost child of the right child
        auto *current = node->children[i + 1];
        while (!current->isLeaf) {
            current = current->children[0];
        }
        return current->keys[0];
    }

    void rebalanceNode(Node<T, t> *parent, int i) {
        assert(parent != nullptr);
        assert(i >= 0 && i <= parent->n);

        auto *leftSibling = i != 0 ? parent->children[i - 1] : nullptr;
        auto *rightSibling = i != parent->n ? parent->children[i + 1] : nullptr;

        // Move a key from an immediate sibling if it has enough keys
        if (leftSibling && leftSibling->n >= t) {
            // Left sibling has enough keys, move a key from it
            moveKeyAndChildFromLeftSibling(parent, i);
        } else if (rightSibling && rightSibling->n >= t) {
            moveKeyAndChildFromRightSibling(parent, i);
        } else {
            // Neither sibling has enough keys, merge with the right sibling unless we're at the last child
            if (i != parent->n) {
                merge(parent, i); // Right sibling
            } else {
                merge(parent, i - 1); // Left sibling
            }
        }
    }

    void moveKeyAndChildFromLeftSibling(Node<T, t> *parent, int i) {
        assert(parent != nullptr);
        assert(i > 0 && i <= parent->n);

        auto *child = parent->children[i];
        auto *leftSibling = parent->children[i - 1];

        assert(child != nullptr);
        assert(leftSibling != nullptr);
        assert(leftSibling->n >= t);

        // Shift keys right to make room for the key that we will take from parent
        for (int j = child->n - 1; j >= 0; --j) {
            child->keys[j + 1] = child->keys[j];
        }

        // Shift children right to make room for the child that we will take from sibling
        if (!child->isLeaf) {
            for (int j = child->n; j >= 0; --j) {
                child->children[j + 1] = child->children[j];
            }
        }

        // Move the last child of the left sibling to child's first child
        if (!child->isLeaf) {
            child->children[0] = leftSibling->children[leftSibling->n];
        }

        // Bring down the key from parent
        child->keys[0] = parent->keys[i - 1];

        // Move up the key from the sibling to parent
        parent->keys[i - 1] = leftSibling->keys[leftSibling->n - 1];

        // Child gained a key and its left sibling lost one
        child->n += 1;
        leftSibling->n -= 1;

        assert(child->n <= 2 * t - 1);
        assert(leftSibling->n >= 0);
    }

    void moveKeyAndChildFromRightSibling(Node<T, t> *parent, int i) {
        assert(parent != nullptr);
        assert(i >= 0 && i < parent->n);

        auto *child = parent->children[i];
        auto *rightSibling = parent->children[i + 1];

        assert(child != nullptr);
        assert(rightSibling != nullptr);
        assert(rightSibling->n >= t);

        // Bring down the key from parent
        child->keys[child->n] = parent->keys[i];

        // Move the first child of right sibling to child's last child
        if (!child->isLeaf) {
            child->children[child->n + 1] = rightSibling->children[0];
        }

        // Move up the first key from the right sibling to parent
        parent->keys[i] = rightSibling->keys[0];

        // Shift keys left to fill the gap in the right sibling
        for (int j = 1; j < rightSibling->n; ++j) {
            rightSibling->keys[j - 1] = rightSibling->keys[j];
        }

        // Shift children left to fill the gap in the right sibling
        if (!rightSibling->isLeaf) {
            for (int j = 1; j <= rightSibling->n; ++j) {
                rightSibling->children[j - 1] = rightSibling->children[j];
            }
        }

        child->n += 1;
        rightSibling->n -= 1;

        assert(child->n <= 2 * t - 1);
        assert(rightSibling->n >= 0);
    }

    void merge(Node<T, t> *parent, int i) {
        assert(parent != nullptr);
        assert(i >= 0 && i < parent->n);
        assert(parent->children[i] != nullptr);
        assert(parent->children[i + 1] != nullptr);

        auto *child = parent->children[i];
        auto *rightSibling = parent->children[i + 1];

        assert(child->n < t);
        assert(rightSibling->n < t);

        // Bring down the key from parent
        child->keys[t - 1] = parent->keys[i];

        // Copy the keys from sibling to child
        for (int j = 0; j < rightSibling->n; ++j) {
            child->keys[j + t] = rightSibling->keys[j];
        }

        // Copy the children from sibling to child
        if (!child->isLeaf) {
            for (int j = 0; j <= rightSibling->n; ++j) {
                child->children[j + t] = rightSibling->children[j];
            }
        }

        // Fill the gap in parent that was created by moving down the key
        for (int j = i + 1; j < parent->n; ++j) {
            parent->keys[j - 1] = parent->keys[j];
        }

        // Fill the gap in parent that was created by moving down the child
        for (int j = i + 2; j <= parent->n; ++j) {
            parent->children[j - 1] = parent->children[j];
        }

        // Size is now child and sibling's size together + the key from parent
        child->n += rightSibling->n + 1;
        --parent->n;

        assert(child->n <= 2 * t - 1);
        assert(parent->n >= 0);

        delete rightSibling;
    }

    void removeFromNonLeaf(Node<T, t> *parent, int i) {
        assert(parent != nullptr);
        assert(parent->n >= t - 1 || root == parent);
        assert(parent->n > 0 && parent->n <= 2 * t - 1);

        auto key = parent->keys[i];
        auto *leftChild = parent->children[i];
        auto *rightChild = parent->children[i + 1];

        if (leftChild->n >= t) {
            // Left child has at least t keys so we replace the key with the largest key in
            // the left subtree and recursively remove the predecessor from the left child
            T predecessor = getInOrderPredecessor(parent, i);
            parent->keys[i] = predecessor;
            remove(leftChild, predecessor);
            assert(leftChild->n >= t - 1);
        } else if (rightChild->n >= t) {
            // Right child has at least t keys so we replace the key with the smallest key in
            // the right subtree and recursively remove the successor from the right child
            T successor = getInOrderSuccessor(parent, i);
            parent->keys[i] = successor;
            remove(rightChild, successor);
            assert(rightChild->n >= t - 1);
        } else {
            // Both children have fewer than t keys so we need to merge the right child into
            // the left child before removing the key
            merge(parent, i);
            assert(leftChild->n >= t - 1);
            remove(leftChild, key);
            assert(leftChild->n >= t - 1);
        }

        assert(parent->n >= t - 1 || root == parent);
    }

    void removeFromLeaf(Node<T, t> *node, const int i) {
        // Overwrite key by shifting keys to the left
        for (int j = i + 1; j < node->n; ++j) {
            node->keys[j - 1] = node->keys[j];
        }

        --node->n; // Reduce the key count

        assert(node->n >= 0 && node->n < 2*t - 1);
    }

    void remove(Node<T, t> *node, T key) {
        assert(node != nullptr);
        assert(node->n > 0 && node->n < 2 * t);

        // Find the first key greater than or equal to key
        int i = 0;
        while (i < node->n && node->keys[i] < key) {
            ++i;
        }

        auto *child = node->children[i];

        // Key exists in the current node
        if (i < node->n && node->keys[i] == key) {
            if (node->isLeaf) {
                removeFromLeaf(node, i);
            } else {
                removeFromNonLeaf(node, i);
            }
        } else { // Key not found in the current node, search in the child
            if (node->isLeaf) { return; } // Key not found

            // Flag which later indicates if we need to process the last child
            bool isLastChild = i == node->n;

            assert(child != nullptr);
            if (child->n < t) {
                rebalanceNode(node, i);
            }

            // If child was the last child and the number of keys in the node decreased
            // after re-balancing, search for the key in the previous child instead
            if (isLastChild && i > node->n) {
                auto *prevChild = node->children[i - 1];
                assert(prevChild != nullptr);
                remove(prevChild, key);
            } else { // Search in the child
                assert(child != nullptr);
                remove(child, key);
            }
        }
    }

    void deleteNode(Node<T, t>* node) {
        if (node == nullptr) return;

        if (!node->isLeaf) {
            for (int i = 0; i <= node->n; i++) {
                deleteNode(node->children[i]);
            }
        }
        delete node;
    }

    template <typename Func>
    void traverseNode(Node<T, t>* node, Func& callback) {
        if (node == nullptr) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            if (!node->isLeaf) {
                traverseNode(node->children[i], callback);
            }

            callback(node->keys[i]);
        }

        if (!node->isLeaf) {
            traverseNode(node->children[i], callback);
        }
    }

    void printNode(Node<T, t>* node, const std::string &path) {
        if (node == nullptr) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            std::cout << node->keys[i] << std::endl;
        }

        for (i = 0; i <= node->n; i++) {
            if (!node->isLeaf) {
                printNode(node->children[i], path + "->" + std::to_string(i));
            }
        }
    }

public:
    BTree() { root = new Node<T, t>(true); }
    ~BTree() { deleteNode(root); }

    Node<T, t> *search(T key) {
        return root == nullptr ? nullptr : search(root, key);
    }

    void insert(T key) {
        auto r = root;
        if (r == nullptr) {
            root = new Node<T, t>(true);
            root->keys[0] = key;
            root->n = 1;
            return;
        }

        // Check if the root is full and needs to be split
        if (r->n == 2*t - 1) {
            auto *s = new Node<T, t>(false);
            s->children[0] = r; // Make the old root a child of the new root
            root = s;
            splitChild(s, 0); // Splits old root and moves its middle key up to the new root
            insertNonFull(s, key);
        } else {
            insertNonFull(r, key);
        }
    }

    void remove(T key) {
        if (!root) { return; }

        remove(root, key);

        // Make sure to clean up the root if it has no keys left
        if (root->n == 0) {
            auto *tmp = root;
            root = root->isLeaf ? nullptr : root->children[0];
            delete tmp;
        }
    }

    template <typename Func>
    void traverse(Func callback) {
        if (root) {
            traverseNode(root, callback);
        }
    }

    void print() {
        if (root) {
            printNode(root, std::string("Root"));
            std::cout << std::endl;
        }
    }
};

#endif //BTREE_H
