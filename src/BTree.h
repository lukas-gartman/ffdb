// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)
#ifndef BTREE_H
#define BTREE_H

#include <concepts>
#include <algorithm>
#include "FFDB.h"
#include "FFDBKey.h"
#include "Typedefs.h"

namespace BTree {

template <PrimitiveVal T, typename V, int t>
class BTree;

template <PrimitiveVal T, typename V, int t>
class Node;

#define REF_NULL Ref<Node<T, V, t>>::null()

/**
* \struct KeyValue
* \brief Represents a key-value pair in the B-Tree.
* \details Stores a key of type `T` and a value of either type `V` or a reference to `V`.
*          A `Ref` is used to store the value in an overflow page if the value does not fit within the node.
*
* \tparam T The type of the key. Must satisfy the PrimitiveVal concept.
* \tparam V The type of the value. Can be a direct value or a `Ref` to the value.
*/

template <PrimitiveVal T, typename V>
struct KeyValue {
    FFDBKey<T> key;
    std::variant<V, Ref<V>> value;
};


/**
 * \class Node
 * \brief Represents a node in the B-Tree.
 * \details Each node contains an array of key-value pairs, an array of child node Refs,
 *          the number of keys, and whether the node is a leaf.
 *
 * \tparam T The type of keys stored in the node. Must satisfy the PrimitiveVal concept.
 * \tparam V The type of values stored in the node. Can be a direct value or a `Ref` to the value in an overflow page.
 * \tparam t The minimum degree of the B-Tree. It defines:
 *           - The minimum number of keys in a non-root node (`t - 1`).
 *           - The maximum number of keys in any node (`2 * t - 1`).
 *           - The minimum number of children in a non-leaf, non-root node (`t`).
 *           - The maximum number of children in any node (`2 * t`).
 */
template <PrimitiveVal T, typename V, int t>
class Node {
private:
    KeyValue<T, V> key_values[2 * t - 1];
    Ref<Node> children[2 * t];
    size_t txn_id;
    int n; // Number of keys in the node
    bool is_leaf;
public:
    friend class BTree<T, V, t>;
};

template<PrimitiveVal T, typename V>
constexpr Size sizeof_node(int t) {
    auto align = [](Size bytes, Size alignment) {
        return (bytes + alignment - 1) & ~(alignment - 1);
    };

    constexpr int n = 5;
    Size sizes[n] = {
            sizeof(KeyValue<T, V>) * (2 * t - 1),
            sizeof(Ref<void>) * 2 * t, // All types of Refs are the same size
            sizeof(Size),
            sizeof(int),
            sizeof(bool)
    };
    Size alignments[n] = {
            alignof(KeyValue<T, V>),
            alignof(Ref<void>), // All types of Refs are aligned the same
            alignof(Size),
            alignof(int),
            alignof(bool)
    };

    Size max_align = std::max({
        alignof(KeyValue<T, V>),
        alignof(Ref<void>), // All types of Refs are aligned the same
        alignof(Size),
        alignof(int),
        alignof(bool)
    });

    Size total_size = 0;
    for (int i = 0; i < n; i++) {
        total_size = align(total_size, alignments[i]);
        total_size += sizes[i];
    }

    return align(total_size, max_align);
}

template<PrimitiveVal T, typename V>
constexpr int find_t(Size bytes) {
    int best_t = 0;
    for (int t = 2; t < bytes; t++) {
        if (sizeof_node<T, V>(t) > bytes) break;
        best_t = t;
    }
    return best_t;
}

/**
 * \class BTree
 * \brief A B-Tree implementation based on CLRS's definition.
 * \details Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8).
 *
 * \tparam T The type of keys stored in the B-Tree. Must satisfy the PrimitiveVal concept.
 * \tparam V The type of values stored in the B-Tree. Can be a direct value or a `Ref` to the value in an overflow page.
 * \tparam t The minimum degree of the B-Tree. It defines:
 *           - The minimum number of keys in a non-root node (`t - 1`).
 *           - The maximum number of keys in any node (`2 * t - 1`).
 *           - The minimum number of children in a non-leaf, non-root node (`t`).
 *           - The maximum number of children in any node (`2 * t`).
 */
template <PrimitiveVal T, typename V, int t>
class BTree {
private:
    using NodeRef = Ref<Node<T, V, t>>;
    NodeRef root;
    int _size;

    /**
     * \brief Creates a new node in the B-Tree.
     * \details Allocates memory for a new node, initializes its properties, and sets all keys and children to default values.
     *
     * \param is_leaf A boolean indicating whether the node is a leaf node.
     * \return A Ref to the newly created node.
     */
    NodeRef create_node(bool is_leaf) {
        // TODO: Something is not updated when mmap is resized so this tree can't be found anymore.
        NodeRef node = current_db()->malloc<Node<T, V, t>>();
        new (&*node) Node<T, V, t>();
        node->is_leaf = is_leaf;
        node->txn_id = current_db()->get_txn_id();
        node->n = 0;
        for (int i = 0; i < 2*t-1; ++i) {
            node->key_values[i].key = T();
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
    void free_node(NodeRef node) {
        current_db()->free(node);
    }

    /**
     * \brief Searches for a key in the B-Tree.
     * \details This function recursively searches for a key in the B-Tree starting from the given node.
     *
     * \param x The node to start the search from.
     * \param key The key to search for in the B-Tree.
     * \return A Ref to the node containing the key, or REF_NULL if the key is not found.
     */
    template <ValidKeyArg<T> K>
    NodeRef search(NodeRef x, const K &key) {
        // Find the index of the first key greater than or equal to key
        int i = 0;
        while (i < x->n && key > x->key_values[i].key) {
            i++;
        }

        return i < x->n && key == x->key_values[i].key ? x // Key found
                                                       : x->is_leaf ? REF_NULL // Key not found
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
    void split_child(NodeRef x, int i) {
        NodeRef y = x->children[i];
        NodeRef z = create_node(y->is_leaf);
        z->n = t - 1;

        // Copy the last t-1 keys of y to z
        for (int j = 0; j < t - 1; j++) {
            z->key_values[j] = y->key_values[j + t];
        }

        // Copy the last t children of y to z
        if (!y->is_leaf) {
            for (int j = 0; j < t; j++) {
                z->children[j] = y->children[j + t];
            }
        }

        // Store the middle key before reducing y's size
        KeyValue<T, V> middle_key_value = y->key_values[t - 1];

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
            x->key_values[j + 1] = x->key_values[j];
        }

        // Insert the middle key of y into x
        x->key_values[i] = middle_key_value;
        ++x->n;
    }

    /**
     * \brief Creates a copy of a given node.
     * \details Allocates memory for a new node, copies the contents of the given node into the new node,
     *          and updates the transaction ID of the new node to the current transaction ID.
     *
     * \param node The Ref to the node to be copied.
     * \return A Ref to the newly created copy of the node.
     */
    NodeRef copy_node(NodeRef node) {
        NodeRef x = current_db()->malloc<Node<T, V, t>>();
        new (&*x) Node<T, V, t>(*node);
        x->txn_id = current_db()->get_txn_id();
        return x;
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
    template<ValidKeyArg<T> K>
    bool insert_non_full(NodeRef x, const K &key, V value) {
        // Find the position to insert the new key, starting from the right
        int i = x->n - 1;
        if (x->is_leaf) {
            // Check if the key already exists
            for (int j = 0; j < x->n; ++j) {
                if (x->key_values[j].key == key) { return false; } // Key already exists, do not insert
            }

            // Shift keys to the right to make room for the new key
            while (i >= 0 && key < x->key_values[i].key) {
                x->key_values[i + 1] = x->key_values[i];
                --i;
            }

            // Insert the new key
            x->key_values[i+1].key = FFDBKey<T>(key);
            x->key_values[i+1].value = value;
            x->n = x->n + 1;
            ++_size;

            return true;
        } else {
            // Find the child to insert the new key
            while (i >= 0 && key < x->key_values[i].key) {
                --i;
            }
            ++i; // Increment i to find the child

            // Copy-on-write
            if (current_db()->get_txn_id() != x->children[i]->txn_id) {
                x->children[i] = copy_node(x->children[i]);
            }

            // Split the child if it is full
            if (x->children[i]->n == 2*t - 1) {
                split_child(x, i);

                // Determine which of the two children to insert the new key into
                if (key > x->key_values[i].key) {
                    ++i;
                }
            }

            // Insert the new key into the appropriate child
            return insert_non_full(x->children[i], key, value);
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
    KeyValue<T, V> get_in_order_predecessor(NodeRef node, int i) {
        // Find the rightmost child of the left child
        NodeRef current = node->children[i];
        while (!current->is_leaf) {
            current = current->children[current->n];
        }
        return current->key_values[current->n - 1];
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
    KeyValue<T, V> get_in_order_successor(NodeRef node, int i) {
        // Find the leftmost child of the right child
        NodeRef current = node->children[i + 1];
        while (!current->is_leaf) {
            current = current->children[0];
        }
        return current->key_values[0];
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
    void rebalance_node(NodeRef parent, int i) {
        NodeRef left_sibling = i != 0 ? parent->children[i - 1] : REF_NULL;
        NodeRef right_sibling = i != parent->n ? parent->children[i + 1] : REF_NULL;

        // Move a key from an immediate sibling if it has enough keys
        if (left_sibling && left_sibling->n >= t) {
            // Left sibling has enough keys, move a key from it
            move_key_and_child_from_left_sibling(parent, i);
        } else if (right_sibling && right_sibling->n >= t) {
            move_key_and_child_from_right_sibling(parent, i);
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
    void move_key_and_child_from_left_sibling(NodeRef parent, int i) {
        NodeRef child = parent->children[i];
        NodeRef left_sibling = parent->children[i - 1];

        // Shift keys right to make room for the key that we will take from parent
        for (int j = child->n - 1; j >= 0; --j) {
            child->key_values[j + 1] = child->key_values[j];
        }

        // Shift children right to make room for the child that we will take from sibling
        if (!child->is_leaf) {
            for (int j = child->n; j >= 0; --j) {
                child->children[j + 1] = child->children[j];
            }
        }

        // Move the last child of the left sibling to child's first child
        if (!child->is_leaf) {
            child->children[0] = left_sibling->children[left_sibling->n];
        }

        // Bring down the key from parent
        child->key_values[0] = parent->key_values[i - 1];

        // Move up the key from the sibling to parent
        parent->key_values[i - 1] = left_sibling->key_values[left_sibling->n - 1];

        // Child gained a key and its left sibling lost one
        child->n += 1;
        left_sibling->n -= 1;
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
    void move_key_and_child_from_right_sibling(NodeRef parent, int i) {
        NodeRef child = parent->children[i];
        NodeRef right_sibling = parent->children[i + 1];

        // Bring down the key from parent
        child->key_values[child->n] = parent->key_values[i];

        // Move the first child of right sibling to child's last child
        if (!child->is_leaf) {
            child->children[child->n + 1] = right_sibling->children[0];
        }

        // Move up the first key from the right sibling to parent
        parent->key_values[i] = right_sibling->key_values[0];

        // Shift keys left to fill the gap in the right sibling
        for (int j = 1; j < right_sibling->n; ++j) {
            right_sibling->key_values[j - 1] = right_sibling->key_values[j];
        }

        // Shift children left to fill the gap in the right sibling
        if (!right_sibling->is_leaf) {
            for (int j = 1; j <= right_sibling->n; ++j) {
                right_sibling->children[j - 1] = right_sibling->children[j];
            }
        }

        child->n += 1;
        right_sibling->n -= 1;
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
    void merge(NodeRef parent, int i) {
        NodeRef child = parent->children[i];
        NodeRef right_sibling = parent->children[i + 1];

        // Bring down the key from parent
        child->key_values[t - 1] = parent->key_values[i];

        // Copy the keys from sibling to child
        for (int j = 0; j < right_sibling->n; ++j) {
            child->key_values[j + t] = right_sibling->key_values[j];
        }

        // Copy the children from sibling to child
        if (!child->is_leaf) {
            for (int j = 0; j <= right_sibling->n; ++j) {
                child->children[j + t] = right_sibling->children[j];
            }
        }

        // Fill the gap in parent that was created by moving down the key
        for (int j = i + 1; j < parent->n; ++j) {
            parent->key_values[j - 1] = parent->key_values[j];
        }

        // Fill the gap in parent that was created by moving down the child
        for (int j = i + 2; j <= parent->n; ++j) {
            parent->children[j - 1] = parent->children[j];
        }

        // Size is now child and sibling's size together + the key from parent
        child->n += right_sibling->n + 1;
        --parent->n;

        free_node(right_sibling);
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
    bool remove_from_non_leaf(NodeRef parent, int i) {
        KeyValue<T, V> key_value = parent->key_values[i];

        // Copy-on-write for both children
        if (current_db()->get_txn_id() != parent->children[i]->txn_id) {
            parent->children[i] = copy_node(parent->children[i]);
        }
        if (current_db()->get_txn_id() != parent->children[i + 1]->txn_id) {
            parent->children[i + 1] = copy_node(parent->children[i + 1]);
        }
        NodeRef left_child = parent->children[i];
        NodeRef right_child = parent->children[i + 1];

        if (left_child->n >= t) {
            // Left child has at least t keys so we replace the key with the largest key in
            // the left subtree and recursively remove the predecessor from the left child
            KeyValue<T, V> predecessor = get_in_order_predecessor(parent, i);
            parent->key_values[i] = predecessor;
            return remove(left_child, predecessor.key);
        } else if (right_child->n >= t) {
            // Right child has at least t keys so we replace the key with the smallest key in
            // the right subtree and recursively remove the successor from the right child
            KeyValue<T, V> successor = get_in_order_successor(parent, i);
            parent->key_values[i] = successor;
            return remove(right_child, successor.key);
        } else {
            // Both children have fewer than t keys so we need to merge the right child into
            // the left child before removing the key
            merge(parent, i);
            return remove(left_child, key_value.key);
        }
    }

    /**
     * \brief Removes a key from a leaf node.
     * \details This function handles the removal of a key from a leaf node by shifting
     *          the key to the left to overwrite the key at the specified index.
     *
     * \param node The leaf node from which the key is to be removed.
     * \param i The index of the key to be removed in the node's `key_values` array.
     */
    bool remove_from_leaf(NodeRef node, const int i) {
        // Overwrite key by shifting keys to the left
        for (int j = i + 1; j < node->n; ++j) {
            node->key_values[j - 1] = node->key_values[j];
        }

        --node->n; // Reduce the key count
        --_size; // Reduce the size of the B-Tree
        return true;
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
    template <ValidKeyArg<T> K>
    bool remove(NodeRef node, const K &key) {
        // Find the first key greater than or equal to key
        int i = 0;
        while (i < node->n && node->key_values[i].key < key) {
            ++i;
        }

        // Ref<Node<T, V, t>> child = node->children[i];
        NodeRef child;

        // Key exists in the current node
        if (i < node->n && node->key_values[i].key == key) {
            if (node->is_leaf) {
                return remove_from_leaf(node, i);
            } else {
                return remove_from_non_leaf(node, i);
            }
        } else { // Key not found in the current node, search in the child
            if (node->is_leaf) { return false; } // Key not found

            // Copy-on-write for the child node
            if (current_db()->get_txn_id() != node->children[i]->txn_id) {
                node->children[i] = copy_node(node->children[i]);
            }
            child = node->children[i];

            // Flag which later indicates if we need to process the last child
            bool is_last_child = i == node->n;

            // Rebalance nodes along the traversal if necessary, ensuring they have >= t keys
            if (child->n < t) {
                rebalance_node(node, i);
            }

            // If child was the last child and the number of keys in the node decreased
            // after re-balancing, search for the key in the previous child instead
            if (is_last_child && i > node->n) {
                NodeRef prev_child = node->children[i - 1];
                return remove(prev_child, key);
            } else { // Search in the child
                return remove(child, key);
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
    void delete_node(NodeRef node) {
        if (node == REF_NULL) return;

        if (!node->is_leaf) {
            for (int i = 0; i <= node->n; i++) {
                delete_node(node->children[i]);
            }
        }
        free_node(node);
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
    void traverse_node(NodeRef node, Func& callback) {
        if (node == REF_NULL) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            if (!node->is_leaf) {
                traverse_node(node->children[i], callback);
            }

            std::visit([&](auto&& val) {
                callback(node->key_values[i].key, val);
            }, node->key_values[i].value);
        }

        if (!node->is_leaf) {
            traverse_node(node->children[i], callback);
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
    void print_node(NodeRef node, const std::string &path) {
        if (node == REF_NULL) { return; }

        int i;
        for (i = 0; i < node->n; i++) {
            std::visit([node, i](auto&& val) {
                std::cout << node->key_values[i].key << ": " << val << std::endl;
            }, node->key_values[i].value);
        }

        for (i = 0; i <= node->n; i++) {
            if (!node->is_leaf) {
                print_node(node->children[i], path + "->" + std::to_string(i));
            }
        }
    }

    void print_node_deluxe(NodeRef node, const std::string &path) {
        if (node == REF_NULL) { return; }

        int i;
        std::cout << path << ": " << std::endl;
        for (i = 0; i < node->n; i++) {
            std::visit([node, i](auto&& val) {
                std::cout << "    node" << i << ": " << node->key_values[i].key << ": " << val << std::endl;
            }, node->key_values[i].value);
        }

        for (i = 0; i <= node->n; i++) {
            if (!node->is_leaf) {
                print_node_deluxe(node->children[i], path + "->" + std::to_string(i));
            }
        }
    }

public:
    /**
     * \brief Constructs a BTree with a given allocator.
     * \details Initializes the by creating an empty root node.
     */
    BTree() : root(REF_NULL), _size(0) {
        root = create_node(true);
    }
    // ~BTree() { delete_node(root); }

    /**
    * \brief Searches for a key in the B-Tree.
    * \details This function searches for a key starting from the root node of the B-Tree.
    *          If the key is found, it returns a Ref to the node containing the key.
    *          If the key is not found, it returns a null Ref.
    *
    * \param key The key to search for in the B-Tree.
    * \return A Ref to the node containing the key, or REF_NULL if the key is not found.
    */
    template <ValidKeyArg<T> K>
    NodeRef search(const K &key) {
        return root == REF_NULL ? REF_NULL : search(root, key);
    }

    template <ValidKeyArg<T> K>
    std::optional<V> get_value(const K &key) {
        auto node = search(key);

        if (node == REF_NULL) return std::nullopt;

        // Find the key in the node
        int i = 0;
        while (i < node->n && node->key_values[i].key != key) ++i;

        auto val = node->key_values[i].value;

        if (std::holds_alternative<V>(val)) return std::get<V>(val);
        if (std::holds_alternative<Ref<V>>(val)) return *std::get<Ref<V>>(val);
    }

    /**
    * \brief Inserts a key-value pair into the B-Tree.
    * \details This function inserts a key-value pair into the B-Tree, ensuring that the tree maintains
    *          its properties. If the root node is full, it is split, and a new root is created.
    *          The key-value pair is then inserted into the appropriate position in the tree.
    *          The function also implements copy-on-write and returns the updated root of the B-Tree.
    *
    * \param key The key to be inserted into the B-Tree.
    * \param value The value associated with the key to be inserted.
    * \return The updated root of the B-Tree.
    */
    template <ValidKeyArg<T> K>
    BTree insert(const K &key, V value) {
        if (root == REF_NULL) {
            root = create_node(true);
            root->key_values[0].key = key;
            root->key_values[0].value = value;
            root->n = 1;
            return *this;
        }

        // Check if the root is full and needs to be split
        if (root->n == 2*t - 1) {
            // To avoid unnecessary Cow triggered by split_child, check if the key exists manually.
            NodeRef s = create_node(false);
            s->children[0] = root; // Make the old root a child of the new root
            root = s;
            split_child(s, 0); // Splits old root and moves its middle key up to the new root
            bool inserted = insert_non_full(s, key, value);
            if (!inserted) {
                NodeRef old_root = s->children[0];
                free_node(s);
                root = old_root;
            }
        } else {
            NodeRef old_root = root;

            // Copy-on-write
            if (current_db()->get_txn_id() != root->txn_id) {
                root = copy_node(root);
            }

            bool inserted = insert_non_full(root, key, value);
            if (!inserted && root->txn_id == current_db()->get_txn_id()) {
                free_node(root);
                root = copy_node(old_root);
            }
        }

        return *this;
    }

    /**
    * \brief Removes a key from the B-Tree.
    * \details This function removes a key from the B-Tree, ensuring that the tree maintains its properties.
    *          If the root node becomes empty after the removal, it is cleaned up and replaced with its first child
    *          (if it exists) or set to null if the tree becomes empty.
    *
    * \param key The key to be removed from the B-Tree.
    */
    template <ValidKeyArg<T> K>
    bool remove(const K &key) {
        if (!root) { return false; }

        NodeRef old_root = root;

        // Copy-on-write
        if (current_db()->get_txn_id() != root->txn_id) {
            root = copy_node(root);
        }

        bool removed = remove(root, key);

        // Make sure to clean up the root if it has no keys left
        if (root->n == 0) {
            NodeRef tmp = root;
            root = root->is_leaf ? REF_NULL : root->children[0];

            // If root changed to a child, we need to ensure copy-on-write for it too
            if (root != REF_NULL && current_db()->get_txn_id() != root->txn_id) {
                root = copy_node(root);
            }

            free_node(tmp);
        }

        // If no actual removal happened and we copied the root, restore the old root
        if (!removed && root->txn_id == current_db()->get_txn_id()) {
            free_node(root);
            root = old_root;
        }

        return removed;
    }

    /**
     * \brief Updates the value associated with a given key in the B-Tree.
     * \details Searches for the specified key in the B-Tree. If the key exists,
     *          updates its value to the new value provided.
     *
     * \param key The key whose value is to be updated.
     * \param new_value The new value to associate with the key.
     * \return True if the key was found and updated; false if the key does not exist in the B-Tree.
     */
    template <ValidKeyArg<T> K>
    bool update(const K &key, V new_value) {
        NodeRef node = search(key);
        if (node == REF_NULL) {
            return false;
        }

        // Find the key in the node
        int i = 0;
        while (i < node->n && node->key_values[i].key != key) {
            ++i;
        }

        // Copy-on-write
        if (current_db()->get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }

        // Update the value
        node->key_values[i].value = new_value;
        return true;
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
            traverse_node(root, callback);
        }
    }

    /**
     * \brief Prints the structure of the B-Tree.
     * \details This function prints the key-value pairs and structure of the B-Tree starting from the root node.
     */
    void print() {
        if (root) {
            print_node(root, std::string("Root"));
            std::cout << std::endl;
        }
    }

    void print_tree() {
        if (root) {
            print_node_deluxe(root, std::string("Root"));
            std::cout << std::endl;
        }
    }
};
}

#endif //BTREE_H