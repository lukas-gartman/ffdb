// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)
#ifndef BTREE_FUNCTIONAL_H
#define BTREE_FUNCTIONAL_H

#include <algorithm>
#include "FFDB.h"
#include "FFDBKey.h"
#include "Typedefs.h"

template <PrimitiveVal T, typename V, int t>
class BTree;

template <PrimitiveVal T, typename V, int t>
class Node;

template<PrimitiveVal T, typename V, int t>
class BTreeFunctional;

#define REF_NULL Ref<Node<T, V, t>>::null()

template <PrimitiveVal T, typename V>
struct KeyValue {
    FFDBKey<T> key;
    V  value;
};

template <PrimitiveVal T, typename V, int t>
class Node {
private:
    KeyValue<T, V> key_values[2 * t - 1];
    Ref<Node> children[2 * t];
    Id txn_id;
    int n; // Number of keys in the node
    bool is_leaf;

    Node(Id txn_id, int n, bool is_leaf): txn_id(txn_id), n(n), is_leaf(is_leaf) {}

public:
    friend class BTreeFunctional<T, V, t>;
};

template <PrimitiveVal T, typename V, int t>
using NodeRef = Ref<Node<T, V, t>>;

template <PrimitiveVal T, typename V, int t>
class BTreeHeader {
private:
    NodeRef<T, V, t> root;
    int size;
    Id txn_id;

public:
    friend class BTreeFunctional<T, V, t>;
};

template <PrimitiveVal T, typename V, int t>
using BTreeRef = Ref<BTreeHeader<T, V, t>>;

template<PrimitiveVal T, typename V>
constexpr int find_t(Size bytes) {
    auto sizeof_node = [](int t) constexpr {
        auto align = [](Size bytes, Size alignment) constexpr {
            return (bytes + alignment - 1) & ~(alignment - 1);
        };

        constexpr int n = 5;
        Size sizes[n] = {
            sizeof(KeyValue<T, V>) * (2 * t - 1),
            sizeof(Ref<void>) * 2 * t,
            sizeof(Size),
            sizeof(int),
            sizeof(bool)
        };
        Size alignments[n] = {
            alignof(KeyValue<T, V>),
            alignof(Ref<void>),
            alignof(Size),
            alignof(int),
            alignof(bool)
        };

        Size max_align = std::max({
            alignof(KeyValue<T, V>),
            alignof(Ref<void>),
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
    };

    int best_t = 0;
    for (int t = 2; t < bytes; t++) {
        if (sizeof_node(t) > bytes) break;
        best_t = t;
    }

    return best_t;
}


template<PrimitiveVal T, typename V, int t>
class BTreeFunctional {
private:
    struct InsertRes {
        bool split;
        bool inserted;
        NodeRef<T, V, t> node; // Node that was split (or not)
        NodeRef<T, V, t> right_node; // Node created by split (or not)
        KeyValue<T, V> middle_key; // Key to propagate up.
    };

    static NodeRef<T, V, t> create_node(bool is_leaf) {

        auto node = malloc_node();
        new (&*node) Node<T, V, t>(current_db()->get_txn_id(), 0, is_leaf);

        for (int i = 0; i < 2 * t - 1; ++i) {
            node->key_values[i].key = T();
            node->children[i] = REF_NULL;
        }
        node->children[2 * t - 1] = REF_NULL;

        return node;
    }

#if FFDB_PAGE_BOUNDARY_AWARE
    static NodeRef<T, V, t> malloc_node() {
        return current_db()->malloc_bytes<Node<T, V, t>>(FFDB_LOGICAL_PAGE_SIZE);
    }

    static void free_node(NodeRef<T, V, t> node) {
        current_db()->free_bytes(node, FFDB_LOGICAL_PAGE_SIZE);
    }
#else
    static NodeRef<T, V, t> malloc_node() {
        return current_db()->malloc<Node<T, V, t>>();
    }

    static void free_node(NodeRef<T, V, t> node) {
        current_db()->free(node);
    }
#endif

    static NodeRef<T, V, t> upd_node(NodeRef<T, V, t> node) {
        if (node->txn_id == current_db()->get_txn_id())
            return node;

        auto new_node = malloc_node();
        new (&*new_node) Node<T, V, t>(*node);
        new_node->txn_id = current_db()->get_txn_id();

        free_node(node);
        return new_node;
    }

    static BTreeRef<T, V, t> upd_header(BTreeRef<T, V, t> header, NodeRef<T, V, t> root, int size) {
        if (header->txn_id == current_db()->get_txn_id()) {
            header->root = root;
            header->size = size;
            return header;
        }

        auto new_header =  current_db()->malloc<BTreeHeader<T, V, t>>();
        new_header->root = root;
        new_header->size = size;
        new_header->txn_id = current_db()->get_txn_id();

        current_db()->free(header);
        return new_header;
    }

    // Originally search
    template<ValidKeyArg<T> K>
    static NodeRef<T, V, t> find_node(NodeRef<T, V, t> x, const K &key) {
        // If REF_NULL is given as argument, return REF_NULL.
        if (x == REF_NULL) return x;

        // Find the index of the first key greater than or equal to key
        int i = 0;

        while (i < x->n && key > x->key_values[i].key) i++;

        return i < x->n && key == x->key_values[i].key
            ? x                               // Key found
            : x->is_leaf ? REF_NULL           // Key not found
            : find_node(x->children[i], key); // Search in the child
    }

    static NodeRef<T, V, t> split_child(NodeRef<T, V, t> x, int i) {
        NodeRef<T, V, t> y = upd_node(x->children[i]);
        // Ensure parent points to the updated (CoW) child
        x->children[i] = y;

        NodeRef<T, V, t> z = create_node(y->is_leaf);
        z->n = t - 1;

        // Copy the last t - 1 keys of y to z
        for (int j = 0; j < t - 1; j++)
            z->key_values[j] = y->key_values[j + t];

        // Copy the last t children of y to z
        if (!y->is_leaf) {
            for (int j = 0; j < t; j++)
                z->children[j] = y->children[j + t];
        }

        // Store the middle key before reducing y's size
        auto middle_kv = y->key_values[t - 1];

        // Reduce the number of keys in y
        y->n = t - 1;

        // Make room for the new child in x
        for (int j = x->n; j >= i + 1; --j)
            x->children[j + 1] = x->children[j];

        // Insert the new child into x
        x->children[i + 1] = z;

        // Make room for the new key in x
        for (int j = x->n - 1; j >= i; --j)
            x->key_values[j + 1] = x->key_values[j];

        // Insert the middle key of y into x
        x->key_values[i] = middle_kv;
        ++x->n;

        return x;
    }

    /*
    static InsertRes split_node(NodeRef<T, V, t> x) {
        auto y = create_node(x->is_leaf);
        y->n = t - 1;

        // Copy last t-1 keys into y
        for (int j = 0; j < t - 1; j++)
            y->key_values[j] = x->key_values[j + t];

        if (!y->is_leaf) {
            for (int j = 0; j < t; j++)
                y->children[j] = x->children[j + t];
        }

        KeyValue<T, V> middle_key = x->key_values[t - 1];

        x->n = t - 1;

        return {true, true, x, y, middle_key};
    } */

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V, t>, bool> insert_non_full(NodeRef<T, V, t> x, const K &key, V value) {
        // Find the position to insert the new key, starting from the right
        int i = x->n - 1;

        if (x->is_leaf) {
            // Check if the key already exists
            for (int j = 0; j < x->n; ++j) {
                if (x->key_values[j].key == key) return std::make_pair(x, false); // Key already exists, do not insert
            }

            x = upd_node(x);

            // Shift keys to the right to make room for the new key
            while (i >= 0 && key < x->key_values[i].key) {
                x->key_values[i + 1] = x->key_values[i];
                --i;
            }

            // Insert the new key
            x->key_values[i + 1].key = key;
            x->key_values[i + 1].value = value;
            x->n = x->n + 1;

            return std::make_pair(x, true);
        }

        // Find the child to insert the new key
        while (i >= 0 && key < x->key_values[i].key) --i;
        ++i; // Increment i to find the child

        // Split the child if it is full
        if (x->children[i]->n == 2 * t - 1) {
            x = split_child(upd_node(x), i);

            // Determine which of the two children to insert the new key into
            if (key > x->key_values[i].key) ++i;
        }

        // Insert the new key into the appropriate child
        auto [child, collision] = insert_non_full(x->children[i], key, value);
        x->children[i] = child;
        return std::make_pair(x, collision);
    }
    /*
    template<ValidKeyArg<T> K>
    static InsertRes insert_rec(NodeRef<T, V, t> x, const K &key, V value) {
        // Find the position to insert the new key, starting from the right
        int i = x->n - 1;

        for (int j = 0; j < x->n; ++j) {
            if (x->key_values[j].key == key) return {false, false, x};
        }

        if (x->is_leaf) {
            x = upd_node(x);

            // Shift keys to the right to make room for the new key
            while (i >= 0 && key < x->key_values[i].key) {
                x->key_values[i + 1] = x->key_values[i];
                --i;
            }

            // Insert the new key
            x->key_values[i + 1] = {key, value};
        }
        else {
            // Internal node
            while (i >= 0 && key < x->key_values[i].key) --i;
            ++i;

            auto ins_res = insert_rec(x->children[i], key, value);

            // Insertion failed, abort.
            if (!ins_res.inserted) return {false, false, x};

            // Insertion successful, CoW x.
            x = upd_node(x);
            x->children[i] = ins_res.node;

            if (!ins_res.split) {
                return {false, true, x};
            }

            for (int j = x->n; j > i; --j)
                x->children[j + 1] = x->children[j];
            x->children[i + 1] = ins_res.right_node;

            for (int j = x->n - 1; j >= i; --j)
                x->key_values[j + 1] = x->key_values[j];
            x->key_values[i] = ins_res.middle_key;
        }

        // Key successfully inserted this recursion
        x->n = x->n + 1;

        if (x->n < 2 * t - 1) return {false, true, x};

        return split_node(x);
    }*/

    static KeyValue<T, V> get_in_order_predecessor(NodeRef<T, V, t> node, int i) {
        // Find the rightmost child of the left child
        auto current = node->children[i];
        while (!current->is_leaf)
            current = current->children[current->n];

        return current->key_values[current->n - 1];
    }

    static KeyValue<T, V> get_in_order_successor(NodeRef<T, V, t> node, int i) {
        // Find the leftmost child of the right child
        auto current = node->children[i + 1];
        while (!current->is_leaf)
            current = current->children[0];

        return current->key_values[0];
    }

    // parent assumed to be copied
    static NodeRef<T, V, t> merge(NodeRef<T, V, t> parent, int i) {
        NodeRef<T, V, t> child = upd_node(parent->children[i]);
        parent->children[i] = child;

        NodeRef<T, V, t> right_sibling = parent->children[i + 1];

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

        return parent;
    }

    // Parent assumed to be copied
    static NodeRef<T, V, t> move_key_and_child_from_left_sibling(NodeRef<T, V, t> parent, int i) {
        NodeRef<T, V, t> child = upd_node(parent->children[i]);
        parent->children[i] = child;

        NodeRef<T, V, t> left_sibling = upd_node(parent->children[i - 1]);
        parent->children[i - 1] = left_sibling;

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

        return parent;
    }

    // Parent assumed to be copied
    static NodeRef<T, V, t> move_key_and_child_from_right_sibling(NodeRef<T, V, t> parent, int i) {
        NodeRef<T, V, t> child = upd_node(parent->children[i]);
        parent->children[i] = child;

        NodeRef<T, V, t> right_sibling = upd_node(parent->children[i + 1]);
        parent->children[i + 1] = right_sibling;

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

        return parent;
    }

    static NodeRef<T, V, t> rebalance_node(NodeRef<T, V, t> parent, int i) {
        NodeRef<T, V, t> left_sibling = i != 0 ? parent->children[i - 1] : REF_NULL;
        NodeRef<T, V, t> right_sibling = i != parent->n ? parent->children[i + 1] : REF_NULL;

        // Move a key from an immediate sibling if it has enough keys
        if (left_sibling && left_sibling->n >= t)
            // Left sibling has enough keys, move a key from it
            return move_key_and_child_from_left_sibling(upd_node(parent), i);

        if (right_sibling && right_sibling->n >= t)
            return move_key_and_child_from_right_sibling(upd_node(parent), i);

        // Neither sibling has enough keys, merge with the right sibling unless we're at the last child
        if (i != parent->n)
            return merge(upd_node(parent), i); // Right sibling

        return merge(upd_node(parent), i - 1); // Left sibling
    }

    // node not assumed to be copied because we don't want to copy if the key is not found.
    template<ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V, t>, bool> remove(NodeRef<T, V, t> node, const K &key) {
        // Find the first key greater than or equal to key
        int i = 0;
        while (i < node->n && key > node->key_values[i].key) ++i;

        // Key exists in the current node
        if (i < node->n && key == node->key_values[i].key) {
            if (node->is_leaf)
               return remove_from_leaf(upd_node(node), i);

            return remove_from_non_leaf(upd_node(node), i);
        }

        // Key not found in the current node and node is leaf, key does not exist.
        if (node->is_leaf) { return std::make_pair(node, false); } // Key not found

        // Before descending, ensure the child we descend into has at least t keys
        NodeRef<T, V, t> parent = node;
        int child_index = i;

        // Rebalance if the child has only t-1 keys
        if (parent->children[child_index]->n == t - 1) {
            parent = upd_node(parent);

            NodeRef<T, V, t> left_sibling = child_index > 0 ? parent->children[child_index - 1] : REF_NULL;
            NodeRef<T, V, t> right_sibling = child_index < parent->n ? parent->children[child_index + 1] : REF_NULL;

            // Move a key from an immediate sibling if it has enough keys
            if (left_sibling && left_sibling->n >= t) {
                // Left sibling has enough keys, move a key from it
                parent = move_key_and_child_from_left_sibling(parent, child_index);
            } else if (right_sibling && right_sibling->n >= t) {
                parent = move_key_and_child_from_right_sibling(parent, child_index);
            } else {
                // Neither sibling has enough keys, merge with the right sibling unless we're at the last child
                if (child_index < parent->n) {
                    parent = merge(parent, child_index);
                } else {
                    parent = merge(parent, child_index - 1);
                    child_index -= 1; // After merging with left sibling, our target child moved left.
                }
            }
        }

        // Descend into the child now guaranteed to contain at least t keys.
        auto [child, modified] = remove(parent->children[child_index], key);
        if (!modified) return std::make_pair(node, false);

        parent->children[child_index] = child;
        return std::make_pair(parent, true);
    }

    // parent assumed to be copied
    static std::pair<NodeRef<T, V, t>, bool> remove_from_non_leaf(NodeRef<T, V, t> parent, int i) {
        KeyValue<T, V> key_value = parent->key_values[i];

        if (parent->children[i]->n >= t) {
            // Left child has at least t keys so we replace the key with the largest key in
            // the left subtree and recursively remove the predecessor from the left child
            auto pred_kv = get_in_order_predecessor(parent, i);
            parent->key_values[i].key = pred_kv.key.deep_copy(); // Replace with deep copy to avoid aliasing with key being removed in child
            parent->key_values[i].value = pred_kv.value;

            // Remove predecessor from left child
            auto left_child = remove(parent->children[i], pred_kv.key).first;
            parent->children[i] = left_child;

            return std::make_pair(parent, true);
        }

        if (parent->children[i + 1]->n >= t) {
            // Right child has at least t keys so we replace the key with the smallest key in
            // the right subtree and recursively remove the successor from the right child
            auto succ_kv = get_in_order_successor(parent, i);
            parent->key_values[i].key = succ_kv.key.deep_copy(); // Replace with deep copy to avoid aliasing with key being removed in child
            parent->key_values[i].value = succ_kv.value;

            // Remove successor from right child
            auto right_child = remove(parent->children[i + 1], succ_kv.key).first;
            parent->children[i + 1] = right_child;

            return std::make_pair(parent, true);
        }

        // Both children have fewer than t keys so we need to merge the right child into
        // the left child before removing the key
        parent = merge(parent, i); // parent already copied
        auto child = remove(parent->children[i], key_value.key).first;
        parent->children[i] = child;

        return std::make_pair(parent, true);
    }

    // node assumed to be copied
    static std::pair<NodeRef<T, V, t>, bool> remove_from_leaf(NodeRef<T, V, t> node, int i) {
        // Free prion of key allocated elsewhere
        node->key_values[i].key.free_ref();

        // Overwrite key by shifting keys to the left
        for (int j = i + 1; j < node->n; ++j)
            node->key_values[j - 1] = node->key_values[j];

        --node->n; // Reduce the key count

        return std::make_pair(node, true);
    }

    static void debug_print(NodeRef<T, V, t> node, const std::string &path) {
        if (node == REF_NULL) return;

        std::cout << path << ": " << std::endl;
        for (int i = 0; i < node->n; i++) {
            std::cout << "    node" << i << ": " << node->key_values[i].key
                      << ": " << node->key_values[i].value << std::endl;
        }

        for (int i = 0; i <= node->n; i++) {
            if (!node->is_leaf) {
                debug_print(node->children[i], path + "->" + std::to_string(i));
            }
        }
    }

    template <ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V, t>, bool> update_value_in_node(NodeRef<T, V, t> x, const K &key, std::function<V(V)> f) {
        // If REF_NULL is given as argument, return REF_NULL.
        if (x == REF_NULL) return std::make_pair(x, false);

        // Find the index of the first key greater than or equal to key
        int i = 0;
        while (i < x->n && key > x->key_values[i].key) i++;

        if (i < x->n && key == x->key_values[i].key) {
            x = upd_node(x);
            x->key_values[i].value = f(x->key_values[i].value);
            return std::make_pair(x, true);
        }

        if (x->is_leaf) return std::make_pair(x, false);

        auto [new_child, updated] = update_value_in_node(x->children[i], key, f);
        if (updated) {
            x = upd_node(x);
            x->children[i] = new_child;
        }

        return std::make_pair(x, updated);
    }

public:
    BTreeFunctional() = delete;

    static BTreeRef<T, V, t> empty() {
        auto btree = current_db()->malloc<BTreeHeader<T, V, t>>();
        btree->size = 0;
        btree->root = REF_NULL;
        btree->txn_id = current_db()->get_txn_id();
        return btree;
    }

    template<ValidKeyArg<T> K>
    static BTreeRef<T, V, t> insert(BTreeRef<T, V, t> btree, const K &key, V value) {
        if (btree->root == REF_NULL) {
            btree = upd_header(btree, create_node(true), 1);
            btree->root->key_values[0].key = key;
            btree->root->key_values[0].value = value;
            btree->root->n = 1;

            return btree;
        }

        // To avoid unnecessary Cow triggered by split_child, check if the key exists manually.
        if (find_node(btree->root, key) != REF_NULL) return btree;

        // Split if node is full.
        if (btree->root->n == 2 * t - 1) {
            auto new_root = create_node(false);
            new_root->children[0] = btree->root; // Make the old root a child of the new root

            // Splits old root and moves its middle key up to the new root
            new_root = insert_non_full(split_child(upd_node(new_root), 0), key, value).first;

            return upd_header(btree, new_root, btree->size + 1);
        }

        auto [new_root, inserted] = insert_non_full(upd_node(btree->root), key, value);

        if (!inserted) return btree;

        return upd_header(btree, new_root, btree->size + 1);
    }

    /*
    template<ValidKeyArg<T> K>
    static BTreeRef<T, V, t> insert(BTreeRef<T, V, t> btree, const K &key, V value) {
        if (btree->root == REF_NULL) {
            btree = upd_header(btree, create_node(true), 1);
            btree->root->key_values[0].key = key;
            btree->root->key_values[0].value = value;
            btree->root->n = 1;

            return btree;
        }

        auto ins_res = insert_rec(btree->root, key, value);

        if (!ins_res.inserted) return btree;

        if (!ins_res.split) return upd_header(btree, ins_res.node, btree->size + 1);

        auto new_root = create_node(false);
        new_root->key_values[0] = ins_res.middle_key;
        new_root->children[0] = ins_res.node;
        new_root->children[1] = ins_res.right_node;
        new_root->n = 1;

        return upd_header(btree, new_root, btree->size + 1);
    }
    */

    template<ValidKeyArg<T> K>
    static std::pair<BTreeRef<T, V, t>, bool> remove(BTreeRef<T, V, t> btree, const K &key) {
        if (!btree->root) { return std::make_pair(btree, false); }

        auto [new_root, removed] = remove(btree->root, key);

        if (!removed) { return std::make_pair(btree, false); }

        btree = upd_header(btree, new_root, btree->size - 1);

        // Make sure to clean up the root if it has no keys left
        if (new_root->n == 0) {
            btree->root = new_root->is_leaf ? REF_NULL : new_root->children[0];
            free_node(new_root);
        }

        return std::make_pair(btree, removed);
    }

    static int size(BTreeRef<T, V, t> btree) {
        return btree->size;
    }

    template <ValidKeyArg<T> K>
    static std::optional<V> get_value(BTreeRef<T, V, t> btree, const K &key) {
        if (!btree) return std::nullopt;

        auto found_node = find_node(btree->root, key);
        if (found_node == REF_NULL) return std::nullopt;

        // Since we already know the key exist in found_node
        int i = 0;
        while (i < found_node->n && key > found_node->key_values[i].key) i++;

        return found_node->key_values[i].value;
    }

    template <ValidKeyArg<T> K>
    static bool key_exists(BTreeRef<T, V, t> btree, const K &key) {
        return find_node(btree->root, key) != REF_NULL;
    }

    template <ValidKeyArg<T> K>
    static BTreeRef<T, V, t> update_value(BTreeRef<T, V, t> btree, const K &key, std::function<V(V)> f) {
        if (!btree) return btree;

        auto [new_root, updated] = update_value_in_node(btree->root, key, f);
        if (!updated) return btree;

        return upd_header(btree, new_root, btree->size);
    }

    template <ValidKeyArg<T> K>
    static BTreeRef<T, V, t> replace_value(BTreeRef<T, V, t> btree, const K &key, V value) {
        return update_value(btree, key, [&](V v){ return value; });
    }

    static void debug_print(BTreeRef<T, V, t> btree) {
        if (!btree->root) return;

        debug_print(btree->root, std::string("Root"));
        std::cout << std::endl;
    }

    static void print_keys(NodeRef<T, V, t> node) {
        std::cout << "-------------------------------------------------------------" << std::endl;
        for (size_t i = 0; i < node->n; ++i) {
            std::cout << "(i=" << i << ", size=" << node->key_values[i].key.size << ", value=" << node->key_values[i].key << ", type_index=" << node->key_values[i].key.key_storage.index() <<  ")" << std::endl;
        }
    }
};

#endif //BTREE_FUNCTIONAL_H

