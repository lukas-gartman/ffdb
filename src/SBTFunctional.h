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
    static NodeRef insert_rec(NodeRef tree, const K &key, V value) {
        if (!tree) return create_node({key, value}, 0, 0);

        if (tree->key_value.key > key) {
            auto new_left = insert(tree->left, key, value);
            if (new_left) {
                tree = upd_node(tree, new_left, tree->right);
                return balancel(tree);
            }
        }
        else if (tree->key_value.key < key) {
            auto new_right = insert(tree->right, key, value);
            if (new_right) {
                tree = upd_node(tree, tree->left, new_right);
                return balancer(tree);
            }
        }

        return REF_NULL;
    }

    template<ValidKeyArg<T> K>
static NodeRef remove_rec(NodeRef tree, const K &key) {
        if (!tree) return REF_NULL;

        if (tree->key_value.key > key) {
            auto new_left = remove_rec(tree->left, key);
            if (new_left)
                return balancer(upd_node(tree, new_left, tree->right));
            return REF_NULL;
        }
        if (tree->key_value.key < key) {
            auto new_right = remove_rec(tree->right, key);
            if (new_right)
                return balancel(upd_node(tree, tree->left, new_right));
            return REF_NULL;
        }

        // found node to delete
        auto old_node = tree;

        if (!tree->left) {
            tree = tree->right;
        } else if (!tree->right) {
            tree = tree->left;
        } else {
            if (tree->left->size > tree->right->size) {
                auto [new_left, popped_node] = pop_last(tree->left);
                tree = balancer(upd_node(popped_node, new_left, tree->right));
            } else {
                auto [new_right, popped_node] = pop_first(tree->right);
                tree = balancel(upd_node(popped_node, tree->left, new_right));
            }
        }

        free_node(old_node);
        return tree;
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
        auto new_tree = insert_rec(tree, key, value);
        if (!new_tree) return tree;
        return new_tree;
    }

    template<ValidKeyArg<T> K>
    static NodeRef remove(NodeRef tree, const K &key) {
        auto new_tree = remove_rec(tree, key);
        if (!new_tree) return tree;
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

    static void traverse(NodeRef tree, std::function<void(const KeyValue<T,V>&)> f){
        if (!tree) return;

        traverse(tree->left, f);
        f(tree->key_value);
        traverse(tree->right, f);
    }

    static std::vector<V> values(NodeRef tree) {
        std::vector<V> result;
        result.reserve(get_size(tree));

        traverse(tree, [&](const KeyValue<T,V> &kv) {
            result.push_back(kv.value);
        });

        return result;
    }
};

#endif //SBT_FUNCTIONAL_H