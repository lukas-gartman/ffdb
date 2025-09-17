#ifndef SBT_FUNCTIONAL_H
#define SBT_FUNCTIONAL_H

#include <algorithm>
#include "FFDB.h"
#include "FFDBKey.h"
#include "Typedefs.h"

#define DELTA 3
#define GAMMA 2

template <PrimitiveVal T, typename V>
class Node;

template <PrimitiveVal T, typename V>
class SBTFunctional;

#define REF_NULL Ref<Node<T, V>>::null()

template <PrimitiveVal T, typename V>
struct KeyValue {
    FFDBKey<T> key;
    V  value;
};

template <PrimitiveVal T, typename V>
class Node {
private:
    KeyValue<T, V> key_value;
    Ref<Node> left;
    Ref<Node> right;
    Id txn_id;
    int size;

    Node(KeyValue<T, V> key_value, Ref<Node> left, Ref<Node> right, Id txn_id, int size):
    key_value(key_value), left(left), right(right), txn_id(txn_id), size(size) {}

public:
    friend class SBTFunctional<T, V>;
};

template <PrimitiveVal T, typename V>
using NodeRef = Ref<Node<T, V>>;

template <PrimitiveVal T, typename V>
using SBTRef = NodeRef<T, V>;

template<PrimitiveVal T, typename V>
class SBTFunctional {
private:
    using NodeRef = NodeRef<T, V>;

    static NodeRef create_node(KeyValue<T, V> key_value, NodeRef left, NodeRef right) {
        auto node = current_db()->malloc<Node<T, V>>();
        new (&*node) Node<T, V>(
            key_value,
            left,
            right,
            current_db()->get_txn_id(),
            1 + get_size(left) + get_size(right)
        );

        return node;
    }

    static void free_node(NodeRef node) {
        current_db()->free(node);
    }

    static NodeRef upd_node(NodeRef node, NodeRef left, NodeRef right) {
        if (node->txn_id == current_db()->get_txn_id()) {
            node->size = 1 + get_size(left) + get_size(right);
            node->left = left;
            node->right = right;
            return node;
        }

        auto new_node = create_node(node->key_value, left, right);
        free_node(node);
        return new_node;
    }

    static std::pair<NodeRef, NodeRef> pop_first(NodeRef tree) {
        if (!tree) return std::make_pair(tree, REF_NULL);

        if (!tree->left) return std::make_pair(tree->right, tree);

        auto [new_tree_left, popped_node] = pop_first(tree->left);

        tree = balancer(upd_node(tree, new_tree_left, tree->right));
        return std::make_pair(tree, popped_node);
    }

    static std::pair<NodeRef, NodeRef> pop_last(NodeRef tree) {
        if (!tree) return std::make_pair(tree, REF_NULL);

        if (!tree->right) return std::make_pair(tree->left, tree);

        auto [new_tree_right, popped_node] = pop_last(tree->right);

        tree = balancel(upd_node(tree, tree->left, new_tree_right));
        return std::make_pair(tree, popped_node);
    }

    static NodeRef rotate_left(NodeRef tree) {
        if (!tree || !tree->right) return tree;

        auto right = tree->right;
        auto right_left = right->left;
        auto new_left = upd_node(tree, tree->left, right_left);
        return upd_node(right, new_left, right->right);
    }

    static NodeRef rotate_right(NodeRef tree) {
        if (!tree || !tree->left) return tree;

        auto left = tree->left;
        auto left_right = left->right;
        auto new_right = upd_node(tree, left_right, tree->right);
        return upd_node(left, left->left, new_right);
    }

    static NodeRef balancel(NodeRef tree) {
        if (!tree) return REF_NULL;

        auto left = tree->left;
        auto right = tree->right;
        auto [left_left, left_right] = get_children(left);

        if (get_size(left) > DELTA * get_size(right)) {
            if (get_size(left_right) < GAMMA * get_size(left_left))
                tree = rotate_right(tree);
            else {
                auto new_left = rotate_left(left);
                tree = upd_node(tree, new_left, right);
                tree = rotate_right(tree);
            }
        }

        return tree;
    }

    static NodeRef balancer(NodeRef tree) {
        if (!tree) return REF_NULL;

        auto left = tree->left;
        auto right = tree->right;
        auto [right_left, right_right] = get_children(right);

        if (get_size(right) > DELTA * get_size(left)) {
            if (get_size(right_left) < GAMMA * get_size(right_right))
                tree = rotate_left(tree);
            else {
                auto new_right = rotate_right(right);
                tree = upd_node(tree, left, new_right);
                tree = rotate_left(tree);
            }
        }

        return tree;
    }

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef, bool> insert_rec(NodeRef tree, const K &key, V value) {
        if (!tree) return std::make_pair(create_node({key, value}, 0, 0), true);

        if (tree->key_value.key > key) {
            auto [new_left, kv_inserted] = insert_rec(tree->left, key, value);
            if (kv_inserted) {
                tree = upd_node(tree, new_left, tree->right);
                return std::make_pair(balancel(tree), true);
            }
        }
        else if (tree->key_value.key < key) {
            auto [new_right, kv_inserted] = insert_rec(tree->right, key, value);
            if (kv_inserted) {
                tree = upd_node(tree, tree->left, new_right);
                return std::make_pair(balancer(tree), true);
            }
        }

        return std::make_pair(tree, false);
    }

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef, bool> remove_rec(NodeRef tree, const K &key) {
        if (!tree) return std::make_pair(tree, false);

        if (tree->key_value.key > key) {
            auto [new_left, key_removed] = remove_rec(tree->left, key);
            if (key_removed)
                return std::make_pair(balancer(upd_node(tree, new_left, tree->right)), true);
            return std::make_pair(tree, false);
        }
        if (tree->key_value.key < key) {
            auto [new_right, key_removed] = remove_rec(tree->right, key);
            if (key_removed)
                return std::make_pair(balancel(upd_node(tree, tree->left, new_right)), true);
            return std::make_pair(tree, false);
        }

        // found node to delete
        auto old_node = tree;

        if (!tree->left) tree = tree->right;
        else if (!tree->right)tree = tree->left;
        else {
            if (tree->left->size > tree->right->size) {
                auto [new_tree, new_left] = pop_last(tree->left);
                tree = balancer(upd_node(new_tree, new_left, tree->right));
            } else {
                auto [new_right, popped_node] = pop_first(tree->right);
                tree = balancel(upd_node(popped_node, tree->left, new_right));
            }
        }

        free_node(old_node);
        return std::make_pair(tree, true);
    }

    template <ValidKeyArg<T> K>
    static std::pair<NodeRef, bool> update_value_rec(NodeRef tree, const K &key, std::function<V(V)> f) {
        if (!tree) return std::make_pair(tree, false);

        if (tree->key_value.key > key) {
            auto [new_left, node_updated] = update_value_rec(tree->left, key, f);
            if (node_updated) return std::make_pair(upd_node(tree, new_left, tree->right), true);
            return std::make_pair(tree, false);
        }
        if (tree->key_value.key < key)  {
            auto [new_right, node_updated] = update_value_rec(tree->right, key, f);
            if (node_updated) return std::make_pair(upd_node(tree, tree->left, new_right), true);
            return std::make_pair(tree, false);
        }

        auto new_tree = upd_node(tree, tree->left, tree->right);
        new_tree->key_value.value = f(new_tree->key_value.value);

        return std::make_pair(new_tree, true);
    }

    static std::pair<NodeRef, NodeRef> get_children(NodeRef tree) {
        if (!tree) return std::make_pair(REF_NULL, REF_NULL);
        return std::make_pair(tree->left, tree->right);
    }

public:
    SBTFunctional() = delete;

    static NodeRef empty() {
        return REF_NULL;
    }

    template<ValidKeyArg<T> K>
    static NodeRef insert(NodeRef tree, const K &key, V value) {
        auto [new_tree, _] = insert_rec(tree, key, value);
        return new_tree;
    }

    template<ValidKeyArg<T> K>
    static NodeRef remove(NodeRef tree, const K &key) {
        auto [new_tree, _] = remove_rec(tree, key);
        return new_tree;
    }

    static Size get_size(NodeRef node) {
        return node != REF_NULL ? node->size : 0;
    }

    template <ValidKeyArg<T> K>
    static std::optional<V> get_value(NodeRef tree, const K &key) {
        if (!tree) return std::nullopt;

        if (tree->key_value.key > key) return get_value(tree->left, key);
        if (tree->key_value.key < key) return get_value(tree->right, key);

        return tree->key_value.value;
    }

    template <ValidKeyArg<T> K>
    static bool key_exists(NodeRef tree, const K &key) {
        return get_value(tree, key) != std::nullopt;
    }

    static void traverse_inorder(NodeRef tree, std::function<void(const KeyValue<T,V>&)> f){
        if (!tree) return;

        traverse_inorder(tree->left, f);
        f(tree->key_value);
        traverse_inorder(tree->right, f);
    }

    static std::vector<V> values(NodeRef tree) {
        std::vector<V> result;
        result.reserve(get_size(tree));

        traverse_inorder(tree, [&](const KeyValue<T,V> &kv) {
            result.push_back(kv.value);
        });

        return result;
    }

    template <ValidKeyArg<T> K>
    static NodeRef update_value(NodeRef tree, const K &key, std::function<V(V)> f) {
        auto [new_tree, _] = update_value_rec(tree, key, f);
        return new_tree;
    }

    static void print(NodeRef tree) {
        // Recursive helper
        std::function<void(NodeRef, std::string)> helper;
        helper = [&](NodeRef node, std::string prefix) {
            if (!node) {
                std::cout << prefix << " NULL" << std::endl;
                return;
            }

            std::cout << prefix << "(key: " << node->key_value.key << ", " << node->key_value.value << ", " << node->txn_id << ")" << std::endl;
            helper(node->left, "    " + prefix);
            helper(node->right, "    " + prefix);
        };

        helper(tree, "");
    }
};

#endif //SBT_FUNCTIONAL_H