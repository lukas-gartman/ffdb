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
    static NodeRef<T, V> create_node(KeyValue<T, V> key_value, NodeRef<T, V> left, NodeRef<T, V> right) {
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

    static void free_node(NodeRef<T, V> node) {
        current_db()->free(node);
    }

    static NodeRef<T, V> upd_node(NodeRef<T, V> node, NodeRef<T, V> left, NodeRef<T, V> right) {
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

    static std::pair<NodeRef<T, V>, NodeRef<T, V>> pop_first(NodeRef<T, V> tree) {
        if (!tree) return std::make_pair(tree, REF_NULL);

        if (!tree->left) return std::make_pair(tree->right, tree);

        auto [new_tree_left, popped_node] = pop_first(tree->left);

        tree = balancer(upd_node(tree, new_tree_left, tree->right));
        return std::make_pair(tree, popped_node);
    }

    static std::pair<NodeRef<T, V>, NodeRef<T, V>> pop_last(NodeRef<T, V> tree) {
        if (!tree) return std::make_pair(tree, REF_NULL);

        if (!tree->right) return std::make_pair(tree->left, tree);

        auto [new_tree_right, popped_node] = pop_last(tree->right);

        tree = balancel(upd_node(tree, tree->left, new_tree_right));
        return std::make_pair(tree, popped_node);
    }

    static NodeRef<T, V> rotate_left(NodeRef<T, V> tree) {
        if (!tree || !tree->right) return tree;

        auto l = tree->left;
        auto r = tree->right;
        auto rl = r->left;
        auto rr = r->right;

        auto new_tree = upd_node(tree, l, rl);
        return upd_node(r, new_tree, rr);
    }

    static NodeRef<T, V> rotate_right(NodeRef<T, V> tree) {
        if (!tree || !tree->left) return tree;

        auto l = tree->left;
        auto ll = l->left;
        auto lr = l->right;
        auto r = tree->right;

        auto new_tree = upd_node(tree, lr, r);
        return upd_node(l, ll, new_tree);
    }

    static NodeRef<T, V> rotate_dleft(NodeRef<T, V> tree) {
        if (!tree || !tree->right || !tree->right->left) return tree;

        auto l = tree->left;
        auto r = tree->right;
        auto rl = r->left;
        auto rr = r->right;
        auto rll = rl->left;
        auto rlr = rl->right;

        auto new_left = upd_node(tree, l, rll);
        auto new_right = upd_node(r, rlr, rr);
        return upd_node(rl, new_left, new_right);
    }

    static NodeRef<T, V> rotate_dright(NodeRef<T, V> tree) {
        if (!tree || !tree->left || !tree->left->right) return tree;

        auto l = tree->left;
        auto ll = l->left;
        auto lr = l->right;
        auto lrl = lr->left;
        auto lrr = lr->right;
        auto r = tree->right;

        auto new_left = upd_node(l, ll, lrl);
        auto new_right = upd_node(tree, lrr, r);
        return upd_node(lr, new_left, new_right);
    }

    static NodeRef<T, V> balancel(NodeRef<T, V> tree) {
        if (!tree) return REF_NULL;

        auto left = tree->left;
        auto right = tree->right;
        auto [left_left, left_right] = get_children(left);

        if (get_size(left) > DELTA * get_size(right)) {
            if (get_size(left_right) < GAMMA * get_size(left_left))
                tree = rotate_right(tree);
            else {
                tree = rotate_dright(tree);
            }
        }

        return tree;
    }

    static NodeRef<T, V> balancer(NodeRef<T, V> tree) {
        if (!tree) return REF_NULL;

        auto left = tree->left;
        auto right = tree->right;
        auto [right_left, right_right] = get_children(right);

        if (get_size(right) > DELTA * get_size(left)) {
            if (get_size(right_left) < GAMMA * get_size(right_right))
                tree = rotate_left(tree);
            else
                tree = rotate_dleft(tree);
        }

        return tree;
    }

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V>, bool> insert_rec(
        NodeRef<T, V> tree, 
        const K &key, 
        V value,
        std::optional<std::function<V(V)>> update_fn = std::nullopt) {
        
        if (!tree) return std::make_pair(create_node({key, value}, 0, 0), true);

        if (tree->key_value.key > key) {
            auto [new_left, kv_inserted] = insert_rec(tree->left, key, value, update_fn);
            if (kv_inserted) {
                tree = upd_node(tree, new_left, tree->right);
                return std::make_pair(balancel(tree), true);
            }
        }
        else if (tree->key_value.key < key) {
            auto [new_right, kv_inserted] = insert_rec(tree->right, key, value, update_fn);
            if (kv_inserted) {
                tree = upd_node(tree, tree->left, new_right);
                return std::make_pair(balancer(tree), true);
            }
        }
        else {
            // Key exists - apply update if provided
            if (update_fn.has_value()) {
                auto new_tree = upd_node(tree, tree->left, tree->right);
                new_tree->key_value.value = (*update_fn)(new_tree->key_value.value);
                return std::make_pair(new_tree, false); // false = no new key inserted
            }
        }

        return std::make_pair(tree, false);
    }

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V>, bool> remove_rec(NodeRef<T, V> tree, const K &key) {
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
                auto [new_left, popped_node] = pop_last(tree->left);
                tree = balancer(upd_node(popped_node, new_left, tree->right));
            } else {
                auto [new_right, popped_node] = pop_first(tree->right);
                tree = balancel(upd_node(popped_node, tree->left, new_right));
            }
        }

        free_node(old_node);
        return std::make_pair(tree, true);
    }

    template <ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V>, bool> update_value_rec(NodeRef<T, V> tree, const K &key, std::function<V(V)> f) {
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

    static std::pair<NodeRef<T, V>, NodeRef<T, V>> get_children(NodeRef<T, V> tree) {
        if (!tree) return std::make_pair(REF_NULL, REF_NULL);
        return std::make_pair(tree->left, tree->right);
    }

    static bool balanced(NodeRef<T, V> tree) {
        if (!tree) return true;

        auto l = tree->left;
        auto r = tree->right;
        auto sl = get_size(l);
        auto sr = get_size(r);

        return (sl + sr <= 1 || (sl <= DELTA * sr && sr <= DELTA * sl)) && balanced(l) && balanced(r);
    }

    static bool ordered(NodeRef<T, V> tree) {
        if (!tree) return true;

        auto l = tree->left;
        auto r = tree->right;

        auto ord_left = l ? l->key_value.key < tree->key_value.key : true;
        auto ord_right = r ? tree->key_value.key < r->key_value.key : true;

        return ord_left && ord_right && ordered(l) && ordered(r);
    }

    static bool valid_size(NodeRef<T, V> tree) {
        std::function<std::optional<int>(NodeRef<T, V>)> real_size;

        real_size = [&](NodeRef<T, V> node) -> std::optional<int> {
            if (!node) return 0;

            auto ls = real_size(node->left);
            auto rs = real_size(node->right);

            if (!ls || !rs) return std::nullopt;

            if (*ls + *rs + 1 == node->size) return node->size;

            return std::nullopt;
        };

        return real_size(tree) == tree->size;
    }

public:
    SBTFunctional() = delete;

    static NodeRef<T, V> empty() {
        return REF_NULL;
    }

    template<ValidKeyArg<T> K>
    static NodeRef<T, V> insert(NodeRef<T, V> tree, const K &key, V value) {
        auto [new_tree, _] = insert_rec(tree, key, value);
        return new_tree;
    }

    template<ValidKeyArg<T> K>
    static NodeRef<T, V> upsert(NodeRef<T, V> tree, const K &key, V value, std::function<V(V)> update_fn) {
        auto [new_tree, _] = insert_rec(tree, key, value, update_fn);
        return new_tree;
    }

    template<ValidKeyArg<T> K>
    static std::pair<NodeRef<T, V>, bool> remove(NodeRef<T, V> tree, const K &key) {
        return remove_rec(tree, key);
    }

    static Size get_size(NodeRef<T, V> node) {
        return node != REF_NULL ? node->size : 0;
    }

    template <ValidKeyArg<T> K>
    static std::optional<V> get_value(NodeRef<T, V> tree, const K &key) {
        if (!tree) return std::nullopt;

        if (tree->key_value.key > key) return get_value(tree->left, key);
        if (tree->key_value.key < key) return get_value(tree->right, key);

        return tree->key_value.value;
    }

    template <ValidKeyArg<T> K>
    static bool key_exists(NodeRef<T, V> tree, const K &key) {
        return get_value(tree, key) != std::nullopt;
    }

    static void traverse_inorder(NodeRef<T, V> tree, std::function<void(const KeyValue<T,V>&)> f){
        if (!tree) return;

        traverse_inorder(tree->left, f);
        f(tree->key_value);
        traverse_inorder(tree->right, f);
    }

    static std::vector<V> values(NodeRef<T, V> tree) {
        std::vector<V> result;
        result.reserve(get_size(tree));

        traverse_inorder(tree, [&](const KeyValue<T,V> &kv) {
            result.push_back(kv.value);
        });

        return result;
    }

    template <ValidKeyArg<T> K>
    static NodeRef<T, V> update_value(NodeRef<T, V> tree, const K &key, std::function<V(V)> f) {
        auto [new_tree, _] = update_value_rec(tree, key, f);
        return new_tree;
    }

    template <ValidKeyArg<T> K>
    static NodeRef<T, V> replace_value(NodeRef<T, V> tree, const K &key, V value) {
        return update_value(tree, key, [&](V v){ return value; });
    }

    static void print(NodeRef<T, V> tree) {
        // Recursive helper
        std::function<void(NodeRef<T, V>, std::string)> helper;
        helper = [&](NodeRef<T, V> node, std::string prefix) {
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

    static bool valid(NodeRef<T, V> tree) {
        if (!tree) return true;
        return ordered(tree) && balanced(tree) && valid_size(tree);
    }
};

#endif //SBT_FUNCTIONAL_H