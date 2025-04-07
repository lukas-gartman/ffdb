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
        int i = 0;
        while (i < x->n && key > x->keys[i]) {
            i++;
        }

        // if (i < x->n && key == x->keys[i]) {
        //     std::cout << "------------------------------" << std::endl;
        //     std::cout << "Found key: " << key << " at i=" << i << std::endl;
        //     std::cout << (x->isLeaf ? "Node is a leaf" : "Node is not a leaf") << std::endl;
        //     std::cout << "Node has " << x->n << " keys out of " << 2*t-1 << std::endl;
        //     std::cout << "Node has " << (x->isLeaf ? 0 : x->n + 1) << " children out of " << 2*t << std::endl;
        //     std::cout << "------------------------------" << std::endl;
        // }

        return i < x->n && key == x->keys[i] ? x
            : x->isLeaf ? nullptr
            : search(x->children[i], key);
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

        int i = x->n - 1;

        if (x->isLeaf) {
            while (i >= 0 && key < x->keys[i]) {
                x->keys[i + 1] = x->keys[i];
                --i;
            }

            x->keys[i + 1] = key;
            x->n = x->n + 1;
        } else {
            while (i >= 0 && key < x->keys[i]) {
                --i;
            }

            ++i;
            assert(x->children[i] != nullptr);
            if (x->children[i]->n == 2*t - 1) {
                splitChild(x, i);

                if (key > x->keys[i]) {
                    ++i;
                }
            }
            insertNonFull(x->children[i], key);
        }
    }

    T getPredecessor(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);
        assert(node->children[i] != nullptr);

        auto *current = node->children[i];
        while (!current->isLeaf) {
            current = current->children[current->n];
        }
        return current->keys[current->n - 1];
    }

    T getSuccessor(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);
        assert(node->children[i + 1] != nullptr);

        auto *current = node->children[i + 1];
        while (!current->isLeaf) {
            current = current->children[current->n];
        }
        return current->keys[0];
    }

    void fill(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i <= node->n);

        auto *leftSibling = (i != 0) ? node->children[i - 1] : nullptr;
        auto *rightSibling = (i != node->n) ? node->children[i + 1] : nullptr;

        if (leftSibling && leftSibling->n >= t) {
            borrowFromPrev(node, i);
        } else if (rightSibling && rightSibling->n >= t) {
            borrowFromNext(node, i);
        } else {
            if (i != node->n) {
                merge(node, i);
            } else {
                merge(node, i - 1);
            }
        }
    }

    void borrowFromPrev(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i > 0 && i <= node->n);

        auto *child = node->children[i];
        auto *sibling = node->children[i - 1];

        assert(child != nullptr);
        assert(sibling != nullptr);
        assert(sibling->n >= t);

        for (int j = child->n - 1; j >= 0; --j) {
            child->keys[j + 1] = child->keys[j];
        }

        if (!child->isLeaf) {
            for (int j = child->n; j >= 0; --j) {
                child->children[j + 1] = child->children[j];
            }
        }

        child->keys[0] = node->keys[i - 1];

        if (!child->isLeaf) {
            child->children[0] = sibling->children[sibling->n];
        }

        node->keys[i - 1] = sibling->keys[sibling->n - 1];

        child->n += 1;
        sibling->n -= 1;

        assert(child->n <= 2 * t - 1);
        assert(sibling->n >= 0);
    }

    void borrowFromNext(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);

        auto *child = node->children[i];
        auto *sibling = node->children[i + 1];

        assert(child != nullptr);
        assert(sibling != nullptr);
        assert(sibling->n >= t);

        child->keys[child->n] = node->keys[i];

        if (!child->isLeaf) {
            child->children[child->n + 1] = sibling->children[0];
        }

        node->keys[i] = sibling->keys[0];

        for (int j = 1; j < sibling->n; ++j) {
            sibling->keys[j - 1] = sibling->keys[j];
        }

        if (!sibling->isLeaf) {
            for (int j = 1; j <= sibling->n; ++j) {
                sibling->children[j - 1] = sibling->children[j];
            }
        }

        child->n += 1;
        sibling->n -= 1;

        assert(child->n <= 2 * t - 1);
        assert(sibling->n >= 0);
    }

    void merge(Node<T, t> *node, int i) {
        assert(node != nullptr);
        assert(i >= 0 && i < node->n);
        assert(node->children[i] != nullptr);
        assert(node->children[i + 1] != nullptr);

        auto *child = node->children[i];
        auto *sibling = node->children[i + 1];

        assert(child->n < 2 * t - 1);
        assert(sibling->n < 2 * t - 1);

        child->keys[t - 1] = node->keys[i];

        for (int j = 0; j < sibling->n; ++j) {
            child->keys[j + t] = sibling->keys[j];
        }

        if (!child->isLeaf) {
            for (int j = 0; j <= sibling->n; ++j) {
                child->children[j + t] = sibling->children[j];
            }
        }

        for (int j = i + 1; j < node->n; ++j) {
            node->keys[j - 1] = node->keys[j];
        }

        for (int j = i + 2; j <= node->n; ++j) {
            node->children[j - 1] = node->children[j];
        }

        child->n += sibling->n + 1;
        --node->n;

        // Clean up sibling - make sure it doesn't delete its children
        // sibling->n = 0;  // Prevent recursive deletion
        // for (int j = 0; j <= 2*t; j++) {
        //     sibling->children[j] = nullptr;
        // }

        assert(child->n <= 2 * t - 1);
        assert(node->n >= 0);

        delete sibling;
    }

    void removeFromNonLeaf(Node<T, t> *node, int i) {
        assert(node != nullptr);
        std::cout << node->n << ", i=" << i << std::endl;
        assert(node->n >= t - 1);

        auto key = node->keys[i];

        if (node->children[i]->n >= t) {
            T predecessor = getPredecessor(node, i);
            node->keys[i] = predecessor;
            remove(node->children[i], predecessor);
            assert(node->children[i]->n >= t - 1);
        } else if (node->children[i + 1]->n >= t) {
            T successor = getSuccessor(node, i);
            node->keys[i] = successor;
            remove(node->children[i + 1], successor);
            assert(node->children[i + 1]->n >= t - 1);
        } else {
            merge(node, i);
            remove(node->children[i], key);
            assert(node->children[i]->n >= t - 1);
        }

        assert(node->n >= t - 1);
    }

    void removeFromLeaf(Node<T, t> *node, const int i) {
        for (int j = i + 1; j < node->n; ++j) {
            node->keys[j - 1] = node->keys[j];
        }

        --node->n;

        assert(node->n >= 0 && node->n < 2*t - 1);
    }

    void remove(Node<T, t> *node, T key) {
        assert(node != nullptr);
        assert(node->n >= 0 && node->n < 2 * t);

        int i = 0;
        while (i < node->n && node->keys[i] < key) {
            ++i;
        }

        if (i < node->n && node->keys[i] == key) {
            if (node->isLeaf) {
                removeFromLeaf(node, i);
            } else {
                removeFromNonLeaf(node, i);
            }
        } else {
            if (node->isLeaf) { return; }

            bool isLastChild = i == node->n;

            assert(node->children[i] != nullptr);
            if (node->children[i]->n < t) {
                fill(node, i);
            }

            if (isLastChild && i > node->n) {
                assert(node->children[i - 1] != nullptr);
                remove(node->children[i - 1], key);
            } else {
                assert(node->children[i] != nullptr);
                remove(node->children[i], key);
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

        if (r->n == 2*t - 1) {
            auto *s = new Node<T, t>(false);
            s->children[0] = r;
            root = s;
            splitChild(s, 0);
            insertNonFull(s, key);
        } else {
            insertNonFull(r, key);
        }
    }

    void remove(T key) {
        if (!root) { return; }

        remove(root, key);

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
};

#endif //BTREE_H
