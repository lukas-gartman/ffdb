// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)

#ifndef BTREE_H
#define BTREE_H

#include <concepts>
#include "Allocator.h"

namespace BTree {

/**
 * \brief Ensures that the key type is comparable to other keys.
 * \details This concept checks that the type `T` supports comparison operators
 *          such as `<`, `<=`, `>`, `>=`, `==`, and `!=`, all of which must return
 *          a value convertible to `bool`.
 */
template <typename T>
concept Comparable = requires(T a, T b) {
    { a < b }  -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b }  -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

template <Comparable T, typename V, int t>
class BTree;

template <Comparable T, typename V, int t>
class Node;

#define REF_NULL Ref<Node<T, V, t>>::null()

/**
* \struct KeyValue
* \brief Represents a key-value pair in the B-Tree.
* \details Stores a key of type `T` and a value of either type `V` or a reference to `V`.
*          A `Ref` is used to store the value in an overflow page if the value does not fit within the node.
*
* \tparam T The type of the key. Must satisfy the Comparable concept.
* \tparam V The type of the value. Can be a direct value or a `Ref` to the value.
*/
template <Comparable T, typename V>
struct KeyValue {
    T key;
    std::variant<V, Ref<V>> value;
};

/**
 * \class Node
 * \brief Represents a node in the B-Tree.
 * \details Each node contains an array of key-value pairs, an array of child node Refs,
 *          the number of keys, and whether the node is a leaf.
 *
 * \tparam T The type of keys stored in the node. Must satisfy the Comparable concept.
 * \tparam V The type of values stored in the node. Can be a direct value or a `Ref` to the value in an overflow page.
 * \tparam t The minimum degree of the B-Tree. It defines:
 *           - The minimum number of keys in a non-root node (`t - 1`).
 *           - The maximum number of keys in any node (`2 * t - 1`).
 *           - The minimum number of children in a non-leaf, non-root node (`t`).
 *           - The maximum number of children in any node (`2 * t`).
 */
    template <Comparable T, typename V, int t>
    class Node {
    private:
        KeyValue<T, V> keyValues[2*t-1];
        Ref<Node> children[2*t];
        int n; // Number of keys in the node
        bool isLeaf;
    public:
        friend class BTree<T, V, t>;
    };

/**
 * \class BTree
 * \brief A B-Tree implementation based on CLRS's definition.
 * \details Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8).
 *
 * \tparam T The type of keys stored in the B-Tree. Must satisfy the Comparable concept.
 * \tparam V The type of values stored in the B-Tree. Can be a direct value or a `Ref` to the value in an overflow page.
 * \tparam t The minimum degree of the B-Tree. It defines:
 *           - The minimum number of keys in a non-root node (`t - 1`).
 *           - The maximum number of keys in any node (`2 * t - 1`).
 *           - The minimum number of children in a non-leaf, non-root node (`t`).
 *           - The maximum number of children in any node (`2 * t`).
 */
template <Comparable T, typename V, int t>
class BTree {
private:
    Ref<Node<T, V, t>> root;
    Allocator *allocator;
    int _size;

    /**
     * \brief Creates a new node in the B-Tree.
     * \details Allocates memory for a new node, initializes its properties, and sets all keys and children to default values.
     *
     * \param isLeaf A boolean indicating whether the node is a leaf node.
     * \return A Ref to the newly created node.
     */
    Ref<Node<T, V, t>> createNode(bool isLeaf) {
        Ref<Node<T, V, t>> node = allocator->malloc<Node<T, V, t>>();
        node->isLeaf = isLeaf;
        node->n = 0;
        for (int i = 0; i < 2*t-1; ++i) {
            node->keyValues[i].key = T();
            node->children[i] = REF_NULL;
        }
        node->children[2*t-1] = REF_NULL;

        return node;
    }

    /**
     * \brief Frees a node in the B-Tree.
     * \details This function deallocates the memory associated with a given node using the allocator.
     *
     * \param node The Ref to the node to be freed.
     */
    void freeNode(Ref<Node<T, V, t>> node) {
        allocator->free(node);
    }

    /**
     * \brief Searches for a key in the B-Tree.
     * \details This function recursively searches for a key in the B-Tree starting from the given node.
     *
     * \param x The node to start the search from.
     * \param key The key to search for in the B-Tree.
     * \return A Ref to the node containing the key, or REF_NULL if the key is not found.
     */
    Ref<Node<T, V, t>> search(Ref<Node<T, V, t>> x, T key) {
        // Find the index of the first key greater than or equal to key
        int i = 0;
        while (i < x->n && key > x->keyValues[i].key) {
            i++;
        }

        return i < x->n && key == x->keyValues[i].key ? x // Key found
                                                      : x->isLeaf ? REF_NULL // Key not found
                                                      : search(x->children[i], key); // Search in the child
    }

    /**
     * \brief Splits a full child node into two nodes.
     * \details This function splits the child node `y` of the node `x` at index `i` into two nodes.
     *          The middle key of `y` is moved up to `x`, and the right half of `y` is moved into a new node `z`.
     *
     * \param x The parent node containing the child to be split.
     * \param i The index of the child node in `x` to be split.
     */
    void splitChild(Ref<Node<T, V, t>> x, int i) {
        Ref<Node<T, V, t>> y = x->children[i];
        Ref<Node<T, V, t>> z = createNode(y->isLeaf);
        z->n = t - 1;

        // Copy the last t-1 keys of y to z
        for (int j = 0; j < t - 1; j++) {
            z->keyValues[j] = y->keyValues[j + t];
        }

        // Copy the last t children of y to z
        if (!y->isLeaf) {
            for (int j = 0; j < t; j++) {
                z->children[j] = y->children[j + t];
            }
        }

        // Store the middle key before reducing y's size
        KeyValue<T, V> middleKeyValue = y->keyValues[t - 1];

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
            x->keyValues[j + 1] = x->keyValues[j];
        }

        // Insert the middle key of y into x
        x->keyValues[i] = middleKeyValue;
        ++x->n;
    }

    /**
     * \brief Inserts a key-value pair into a non-full node.
     * \details This function handles the insertion of a key-value pair into a node that is not full.
     *          If the node is a leaf, the pair is directly inserted into the correct position.
     *          If the node is an internal node, the function determines the appropriate child
     *          to insert the pair into, splitting the child if necessary.
     *
     * \param x The node into which the key-value pair is to be inserted.
     * \param key The key to be inserted.
     * \param value The value associated with the key.
     */
    void insertNonFull(Ref<Node<T, V, t>> x, T key, V value) {
        // Find the position to insert the new key, starting from the right
        int i = x->n - 1;
        if (x->isLeaf) {
            // Shift keys to the right to make room for the new key
            while (i >= 0 && key < x->keyValues[i].key) {
                x->keyValues[i + 1] = x->keyValues[i];
                --i;
            }

            if (x->keyValues[i].key == key) { return; } // Key already exists, do not insert

            // Insert the new key
            x->keyValues[i+1].key = key;
            x->keyValues[i+1].value = value;
            x->n = x->n + 1;
            ++_size;
        } else {
            // Find the child to insert the new key
            while (i >= 0 && key < x->keyValues[i].key) {
                --i;
            }

            ++i; // Increment i to find the child

            // Split the child if it is full
            if (x->children[i]->n == 2*t - 1) {
                splitChild(x, i);

                // Determine which of the two children to insert the new key into
                if (key > x->keyValues[i].key) {
                    ++i;
                }
            }

            // Insert the new key into the appropriate child
            insertNonFull(x->children[i], key, value);
        }
    }

    /**
     * \brief Retrieves the in-order predecessor of a key.
     * \details This function finds the largest key-value pair in the left subtree of the node at the given index.
     *          It traverses to the rightmost child of the left child to locate the predecessor.
     *
     * \param node The node containing the key-value pair whose predecessor is to be found.
     * \param i The index of the key-value pair in the node.
     * \return The in-order predecessor key-value pair.
     */
    KeyValue<T, V> getInOrderPredecessor(Ref<Node<T, V, t>> node, int i) {
        // Find the rightmost child of the left child
        Ref<Node<T, V, t>> current = node->children[i];
        while (!current->isLeaf) {
            current = current->children[current->n];
        }
        return current->keyValues[current->n - 1];
    }

    /**
     * \brief Retrieves the in-order successor of a key-value pair.
     * \details This function finds the smallest key-value pair in the right subtree of the node at the given index.
     *          It traverses to the leftmost child of the right child to locate the successor.
     *
     * \param node The node containing the key-value pair whose successor is to be found.
     * \param i The index of the key-value pair in the node.
     * \return The in-order successor key-value pair.
     */
    KeyValue<T, V> getInOrderSuccessor(Ref<Node<T, V, t>> node, int i) {
        // Find the leftmost child of the right child
        Ref<Node<T, V, t>> current = node->children[i + 1];
        while (!current->isLeaf) {
            current = current->children[0];
        }
        return current->keyValues[0];
    }

    /**
     * \brief Rebalances a node.
     * \details This function ensures that a node satisfies the minimum degree property.
     *          If a child node has fewer than the required number of keys, it attempts to rebalance
     *          the node by borrowing a key from an immediate sibling or merging with a sibling.
     *
     * \param parent The parent node containing the child to be rebalanced.
     * \param i The index of the child node in the parent node's children array.
     */
    void rebalanceNode(Ref<Node<T, V, t>> parent, int i) {
        Ref<Node<T, V, t>> leftSibling = i != 0 ? parent->children[i - 1] : REF_NULL;
        Ref<Node<T, V, t>> rightSibling = i != parent->n ? parent->children[i + 1] : REF_NULL;

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

    /**
     * \brief Moves a key-value pair and child from the left sibling to the current child.
     * \details This function rebalances a node by moving a key-value pair from the left sibling
     *          to the parent and a key-value pair from the parent to the current child. It also
     *          moves the last child of the left sibling to the current child.
     *
     * \param parent The parent node containing the child and its left sibling.
     * \param i The index of the current child in the parent's children array.
     */
    void moveKeyAndChildFromLeftSibling(Ref<Node<T, V, t>> parent, int i) {
        Ref<Node<T, V, t>> child = parent->children[i];
        Ref<Node<T, V, t>> leftSibling = parent->children[i - 1];

        // Shift keys right to make room for the key that we will take from parent
        for (int j = child->n - 1; j >= 0; --j) {
            child->keyValues[j + 1] = child->keyValues[j];
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
        child->keyValues[0] = parent->keyValues[i - 1];

        // Move up the key from the sibling to parent
        parent->keyValues[i - 1] = leftSibling->keyValues[leftSibling->n - 1];

        // Child gained a key and its left sibling lost one
        child->n += 1;
        leftSibling->n -= 1;
    }

    /**
     * \brief Moves a key-value pair and child from the right sibling to the current child.
     * \details This function rebalances a node by moving a key-value pair from the right sibling
     *          to the parent and a key-value pair from the parent to the current child. It also
     *          moves the first child of the right sibling to the current child.
     *
     * \param parent The parent node containing the child and its right sibling.
     * \param i The index of the current child in the parent's children array.
     */
    void moveKeyAndChildFromRightSibling(Ref<Node<T, V, t>> parent, int i) {
        Ref<Node<T, V, t>> child = parent->children[i];
        Ref<Node<T, V, t>> rightSibling = parent->children[i + 1];

        // Bring down the key from parent
        child->keyValues[child->n] = parent->keyValues[i];

        // Move the first child of right sibling to child's last child
        if (!child->isLeaf) {
            child->children[child->n + 1] = rightSibling->children[0];
        }

        // Move up the first key from the right sibling to parent
        parent->keyValues[i] = rightSibling->keyValues[0];

        // Shift keys left to fill the gap in the right sibling
        for (int j = 1; j < rightSibling->n; ++j) {
            rightSibling->keyValues[j - 1] = rightSibling->keyValues[j];
        }

        // Shift children left to fill the gap in the right sibling
        if (!rightSibling->isLeaf) {
            for (int j = 1; j <= rightSibling->n; ++j) {
                rightSibling->children[j - 1] = rightSibling->children[j];
            }
        }

        child->n += 1;
        rightSibling->n -= 1;
    }

    /**
     * \brief Merges a child node with its right sibling.
     * \details This function merges the child node at index `i` with its right sibling in the parent node.
     *          The key-value pair from the parent node that separates the two children is moved down into the merged node.
     *          The right sibling is then deallocated, and the parent node is updated to reflect the merge.
     *
     * \param parent The parent node containing the child and its right sibling.
     * \param i The index of the child node in the parent's children array.
     */
    void merge(Ref<Node<T, V, t>> parent, int i) {
        Ref<Node<T, V, t>> child = parent->children[i];
        Ref<Node<T, V, t>> rightSibling = parent->children[i + 1];

        // Bring down the key from parent
        child->keyValues[t - 1] = parent->keyValues[i];

        // Copy the keys from sibling to child
        for (int j = 0; j < rightSibling->n; ++j) {
            child->keyValues[j + t] = rightSibling->keyValues[j];
        }

        // Copy the children from sibling to child
        if (!child->isLeaf) {
            for (int j = 0; j <= rightSibling->n; ++j) {
                child->children[j + t] = rightSibling->children[j];
            }
        }

        // Fill the gap in parent that was created by moving down the key
        for (int j = i + 1; j < parent->n; ++j) {
            parent->keyValues[j - 1] = parent->keyValues[j];
        }

        // Fill the gap in parent that was created by moving down the child
        for (int j = i + 2; j <= parent->n; ++j) {
            parent->children[j - 1] = parent->children[j];
        }

        // Size is now child and sibling's size together + the key from parent
        child->n += rightSibling->n + 1;
        --parent->n;

        freeNode(rightSibling);
    }

    /**
     * \brief Removes a key from a non-leaf node.
     * \details This function handles the removal of a key from a non-leaf node. Depending on
     *          the state of the child nodes, it either replaces the key with its in-order
     *          predecessor or successor, or merges the child nodes if necessary.
     *
     * \param parent The parent node containing the key to be removed.
     * \param i The index of the key in the parent node's keys array.
     */
    void removeFromNonLeaf(Ref<Node<T, V, t>> parent, int i) {
        KeyValue<T, V> keyValue = parent->keyValues[i];
        Ref<Node<T, V, t>> leftChild = parent->children[i];
        Ref<Node<T, V, t>> rightChild = parent->children[i + 1];

        if (leftChild->n >= t) {
            // Left child has at least t keys so we replace the key with the largest key in
            // the left subtree and recursively remove the predecessor from the left child
            KeyValue<T, V> predecessor = getInOrderPredecessor(parent, i);
            parent->keyValues[i] = predecessor;
            remove(leftChild, predecessor);
        } else if (rightChild->n >= t) {
            // Right child has at least t keys so we replace the key with the smallest key in
            // the right subtree and recursively remove the successor from the right child
            KeyValue<T, V> successor = getInOrderSuccessor(parent, i);
            parent->keyValues[i] = successor;
            remove(rightChild, successor);
        } else {
            // Both children have fewer than t keys so we need to merge the right child into
            // the left child before removing the key
            merge(parent, i);
            remove(leftChild, keyValue);
        }
    }

    /**
     * \brief Removes a key from a leaf node.
     * \details This function handles the removal of a key from a leaf node by shifting
     *          the key to the left to overwrite the key at the specified index.
     *
     * \param node The leaf node from which the key is to be removed.
     * \param i The index of the key to be removed in the node's `keyValues` array.
     */
    void removeFromLeaf(Ref<Node<T, V, t>> node, const int i) {
        // Overwrite key by shifting keys to the left
        for (int j = i + 1; j < node->n; ++j) {
            node->keyValues[j - 1] = node->keyValues[j];
        }

        --node->n; // Reduce the key count
        --_size; // Reduce the size of the B-Tree
    }

    /**
     * \brief Removes a key from the B-Tree.
     * \details This function removes a key from the B-Tree, ensuring that the tree maintains its properties.
     *          If the key-value pair exists in the current node, it is removed directly. Otherwise, the function searches
     *          for the key-value pair in the appropriate child node, rebalancing the tree if necessary.
     *
     * \param node The node from which the key is to be removed.
     * \param key The key to be removed from the B-Tree.
     */
    void remove(Ref<Node<T, V, t>> node, T key) {
        // Find the first key greater than or equal to key
        int i = 0;
        while (i < node->n && node->keyValues[i].key < key) {
            ++i;
        }

        Ref<Node<T, V, t>> child = node->children[i];

        // Key exists in the current node
        if (i < node->n && node->keyValues[i].key == key) {
            if (node->isLeaf) {
                removeFromLeaf(node, i);
            } else {
                removeFromNonLeaf(node, i);
            }
        } else { // Key not found in the current node, search in the child
            if (node->isLeaf) { return; } // Key not found

            // Flag which later indicates if we need to process the last child
            bool isLastChild = i == node->n;

            // Rebalance nodes along the traversal if necessary, ensuring they have >= t keys
            if (child->n < t) {
                rebalanceNode(node, i);
            }

            // If child was the last child and the number of keys in the node decreased
            // after re-balancing, search for the key in the previous child instead
            if (isLastChild && i > node->n) {
                Ref<Node<T, V, t>> prevChild = node->children[i - 1];
                remove(prevChild, key);
            } else { // Search in the child
                remove(child, key);
            }
        }
    }

    /**
     * \brief Recursively deletes a node and its children from the B-Tree.
     * \details This function deallocates the memory associated with a given node and all its child nodes.
     *          It ensures that all nodes in the subtree rooted at the given node are properly freed.
     *
     * \param node The Ref to the node to be deleted. If the node is null, the function does nothing.
     */
    void deleteNode(Ref<Node<T, V, t>> node) {
        if (node == REF_NULL) return;

        if (!node->isLeaf) {
            for (int i = 0; i <= node->n; i++) {
                deleteNode(node->children[i]);
            }
        }
        freeNode(node);
    }

    /**
    * \brief Traverses the nodes of the B-Tree in-order and applies a callback function to each key-value pair.
    * \details This function performs an in-order traversal of the B-Tree, starting from the given node.
    *          For each key-value pair encountered during the traversal, the provided callback function is invoked.
    *          If the node is not a leaf, the function recursively traverses its children.
    *
    * \tparam Func The type of the callback function. It must be callable with two arguments: the key of type `T` and the value of type `V`.
    * \param node The Ref to the node to start the traversal from.
    * \param callback The callback function to apply to each key-value pair during the traversal.
    */
    template <typename Func>
    void traverseNode(Ref<Node<T, V, t>> node, Func& callback) {
        if (node == REF_NULL) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            if (!node->isLeaf) {
                traverseNode(node->children[i], callback);
            }

            std::visit([&](auto&& val) {
                callback(node->keyValues[i].key, val);
            }, node->keyValues[i].value);
        }

        if (!node->isLeaf) {
            traverseNode(node->children[i], callback);
        }
    }

    /**
     * \brief Recursively prints the keys and values of the B-Tree nodes.
     * \details This function performs a recursive traversal of the B-Tree, starting from the given node.
     *          It prints the keys and values of each node and their respective paths in the tree.
     *
     * \param node The Ref to the node to start printing from.
     * \param path A string representing the path to the current node in the tree.
     */
    void printNode(Ref<Node<T, V, t>> node, const std::string &path) {
        if (node == REF_NULL) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            std::visit([node, i](auto&& val) {
                std::cout << node->keyValues[i].key << ": " << val << std::endl;
            }, node->keyValues[i].value);
        }

        for (i = 0; i <= node->n; i++) {
            if (!node->isLeaf) {
                printNode(node->children[i], path + "->" + std::to_string(i));
            }
        }
    }

public:
    /**
     * \brief Constructs a BTree with a given allocator.
     * \details Initializes the BTree with the provided memory allocator and creates an empty root node.
     *
     * \param alloc A pointer to the memory allocator used for node allocation.
     */
    explicit BTree(Allocator *alloc) : root(REF_NULL), _size(0) {
        allocator = alloc;
        root = createNode(true);
    }
    // ~BTree() { deleteNode(root); }

    /**
     * \brief Searches for a key in the B-Tree.
     * \details This function searches for a key starting from the root node of the B-Tree.
     *          If the key is found, it returns a Ref to the node containing the key.
     *          If the key is not found, it returns a null Ref.
     *
     * \param key The key to search for in the B-Tree.
     * \return A Ref to the node containing the key, or REF_NULL if the key is not found.
     */
    Ref<Node<T, V, t>> search(T key) {
        return root == REF_NULL ? REF_NULL : search(root, key);
    }

    /**
     * \brief Inserts a key-value pair into the B-Tree.
     * \details This function inserts a key-value pair into the B-Tree, ensuring that the tree maintains
     *          its properties. If the root node is full, it is split, and a new root is created.
     *          The key-value pair is then inserted into the appropriate position in the tree.
     *
     * \param key The key to be inserted into the B-Tree.
     * \param value The value associated with the key to be inserted.
     */
    void insert(T key, V value) {
        Ref<Node<T, V, t>> r = root;
        if (r == REF_NULL) {
            root = createNode(true);
            root->keyValues[0].key = key;
            root->keyValues[0].value = value;
            root->n = 1;
            return;
        }

        // Check if the root is full and needs to be split
        if (r->n == 2*t - 1) {
            Ref<Node<T, V, t>> s = createNode(false);
            s->children[0] = r; // Make the old root a child of the new root
            root = s;
            splitChild(s, 0); // Splits old root and moves its middle key up to the new root
            insertNonFull(s, key, value);
        } else {
            insertNonFull(r, key, value);
        }
    }

    /**
     * \brief Removes a key from the B-Tree.
     * \details This function removes a key from the B-Tree, ensuring that the tree maintains its properties.
     *          If the root node becomes empty after the removal, it is cleaned up and replaced with its first child
     *          (if it exists) or set to null if the tree becomes empty.
     *
     * \param key The key to be removed from the B-Tree.
     */
    void remove(T key) {
        if (!root) { return; }

        remove(root, key);

        // Make sure to clean up the root if it has no keys left
        if (root->n == 0) {
            Ref<Node<T, V, t>> tmp = root;
            root = root->isLeaf ? REF_NULL : root->children[0];
            freeNode(tmp);
        }
    }

    /**
     * \brief Gets the number of nodes in the B-Tree.
     * \details This function returns the current counter keeping track of the number of keys in the B-Tree.
     * \return The number of keys in the B-Tree.
     */
    [[nodiscard]]
    int size() const {
        return _size;
    }

    /**
     * \brief Traverses the B-Tree and applies a callback function to each key-value pair.
     * \details This function performs an in-order traversal of the B-Tree, starting from the root node.
     *          For each key-value pair encountered during the traversal, the provided callback function is invoked.
     *          If the tree is empty, the function does nothing.
     *
     * \tparam Func The type of the callback function. It must be callable with two arguments: the key of type `T` and the value of type `V`.
     * \param callback The callback function to apply to each key-value pair during the traversal.
     *
     * \note The callback function should not modify the structure of the B-Tree.
     *
     * \code{.cpp}
     * // Example usage: Print all keys and values in the B-Tree
     * tree.traverse([](int key, std::string value) {
     *     std::cout << key << ": " << value << std::endl;
     * });
     * \endcode
     */
    template <typename Func>
    void traverse(Func callback) {
        if (root) {
            traverseNode(root, callback);
        }
    }

    /**
     * \brief Prints the structure of the B-Tree.
     * \details This function prints the key-value pairs and structure of the B-Tree starting from the root node.
     */
    void print() {
        if (root) {
            printNode(root, std::string("Root"));
            std::cout << std::endl;
        }
    }
};
}

#endif //BTREE_H