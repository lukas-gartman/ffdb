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
    static NodeRef<T, V, t> create_node(bool is_leaf) {

        auto node = current_db()->malloc<Node<T, V, t>>();
        new (&*node) Node<T, V, t>(current_db()->get_txn_id(), 0, is_leaf);

        for (int i = 0; i < 2 * t - 1; ++i) {
            node->key_values[i].key = T();
            node->children[i] = REF_NULL;
        }
        node->children[2 * t - 1] = REF_NULL;

        return node;
    }

    static void free_node(NodeRef<T, V, t> node) {
        current_db()->free(node);
    }

    static NodeRef<T, V, t> upd_node(NodeRef<T, V, t> node) {
        if (node->txn_id == current_db()->get_txn_id())
            return node;

        auto new_node = current_db()->malloc<Node<T, V, t>>();
        new (&*new_node) Node<T, V, t>(*node);
        new_node->txn_id = current_db()->get_txn_id();

        free_node(node);
        return new_node;
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
        while (i < node->n && node->key_values[i].key < key) ++i;

        // Key exists in the current node
        if (i < node->n && node->key_values[i].key == key) {
            if (node->is_leaf)
               return remove_from_leaf(upd_node(node), i);

            return remove_from_non_leaf(upd_node(node), i);
        }

        // Key not found in the current node and node is leaf, key does not exist.
        if (node->is_leaf) { return std::make_pair(node, false); } // Key not found

        // Key not found in the current node, search in child.
        auto [child, modified] = remove(node->children[i], key);

        // Key not found in child
        if (!modified) return std::make_pair(node, false);

        // Key was removed so we need to cow
        node = upd_node(node);
        node->children[i] = child;

        // Rebalance nodes along the traversal if necessary, ensuring they have >= t keys
        if (child->n < t) node = rebalance_node(node, i);

        return std::make_pair(node, modified);
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
            auto [left_child, _] = remove(parent->children[i], pred_kv.key);
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
            auto [right_child, _] = remove(parent->children[i + 1], succ_kv.key);
            parent->children[i + 1] = right_child;

            return std::make_pair(parent, true);
        }

        // Both children have fewer than t keys so we need to merge the right child into
        // the left child before removing the key
        parent = merge(parent, i); // parent already copied
        auto [child, _] = remove(parent->children[i], key_value.key);
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
    static std::pair<NodeRef<T, V, t>, bool> update_value(NodeRef<T, V, t> x, const K &key, std::function<V(V)> f) {
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

        auto [new_child, updated] = update_value(x->children[i], key, f);
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
        return btree;
    }

    template<ValidKeyArg<T> K>
    static BTreeRef<T, V, t> insert(BTreeRef<T, V, t> btree, const K &key, V value) {
        if (btree->root == REF_NULL) {
            auto new_btree = current_db()->malloc<BTreeHeader<T, V, t>>();

            new_btree->root = create_node(true);
            new_btree->root->key_values[0].key = key;
            new_btree->root->key_values[0].value = value;
            new_btree->root->n = 1;

            new_btree->size = 1;

            current_db()->free(btree);

            return new_btree;
        }

        // To avoid unnecessary Cow triggered by split_child, check if the key exists manually.
        if (find_node(btree->root, key) != REF_NULL) return btree;

        // Check if the root is full and needs to be split
        if (btree->root->n == 2 * t - 1) {
            auto new_root = create_node(false);
            new_root->children[0] = btree->root; // Make the old root a child of the new root

            //btree->root = new_root;
            // Splits old root and moves its middle key up to the new root
            new_root = insert_non_full(split_child(upd_node(new_root), 0), key, value).first;

            auto new_btree = current_db()->malloc<BTreeHeader<T, V, t>>();

            new_btree->root = new_root;
            new_btree->size = btree->size + 1;

            current_db()->free(btree);

            return new_btree;
        }

        auto [new_root, inserted] = insert_non_full(upd_node(btree->root), key, value);

        if (!inserted) return btree;

        auto new_btree = current_db()->malloc<BTreeHeader<T, V, t>>();

        new_btree->root = new_root;
        new_btree->size = btree->size + 1;

        current_db()->free(btree);

        return new_btree;
    }

    template<ValidKeyArg<T> K>
    static std::pair<BTreeRef<T, V, t>, bool> remove(BTreeRef<T, V, t> btree, const K &key) {
        if (!btree->root) { return std::make_pair(btree, false); }

        auto [new_root, removed] = remove(btree->root, key);

        if (!removed) { return std::make_pair(btree, false); }

        auto new_btree = current_db()->malloc<BTreeHeader<T, V, t>>();
        new_btree->root = new_root;
        new_btree->size = btree->size - 1;
        current_db()->free(btree); // Will be freed immediately since it was allocated this txn

        // Make sure to clean up the root if it has no keys left
        if (new_root->n == 0) {
            new_btree->root = new_root->is_leaf ? REF_NULL : new_root->children[0];
            free_node(new_root);
        }

        return std::make_pair(new_btree, removed);
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

        auto [new_root, updated] = update_value(btree->root, key, f);
        if (!updated) return btree;

        auto old_btree = btree;
        btree = current_db()->malloc<BTreeHeader<T, V, t>>();
        btree->root = new_root;
        btree->size = old_btree->size;

        return btree;
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
};

#endif //BTREE_FUNCTIONAL_H

