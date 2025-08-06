#ifndef SBT_H
#define SBT_H

#include <concepts>
#include "FFDB2.h"
#include "FFDBKey.h"
#include "Typedefs.h"

namespace SBT {

template <Comparable T, typename V>
class Node {
public:
    FFDBKey<T> key;
    V value;
    Ref<Node> left;
    Ref<Node> right;
    size_t txn_id;
    int size; // subtree size

    Node(const FFDBKey<T> &k, const V &v, Id txn_id)
        : key(k), value(v), left(Ref<Node>::null()), right(Ref<Node>::null()), txn_id(txn_id), size(1) {}
};

template <Comparable T, typename V>
class SBT {
    Ref<Node<T, V>> root;
    int _size;

    Ref<Node<T, V>> copy_node(Ref<Node<T, V>> node) {
        Ref<Node<T, V>> n = current_db()->malloc<Node<T, V>>();
        new (&*n) Node<T, V>(*node);
        n->txn_id = current_db()->get_txn_id();
        return n;
    }

    void free_node(Ref<Node<T, V>> node) {
        current_db()->free(node);
    }

    // void destroy(Ref<Node<K, V>> node) {
    //     if (!node) return;
    //     destroy(node->left);
    //     destroy(node->right);
    //     free_node(node);
    // }

    void update_size(Ref<Node<T, V>> &node) {
        if (!node) return;
        node->size = 1;
        if (node->left)  node->size += node->left->size;
        if (node->right) node->size += node->right->size;
    }

    void left_rotate(Ref<Node<T, V>> &node) {
        Ref<Node<T, V>> r = node->right;
        node->right = r->left;
        r->left = node;
        update_size(node->left);
        update_size(node);
        node = r;
    }

    void right_rotate(Ref<Node<T, V>> &node) {
        Ref<Node<T, V>> l = node->left;
        node->left = l->right;
        l->right = node;
        update_size(node->right);
        update_size(node);
        node = l;
    }

    void maintain(Ref<Node<T, V>> &node, bool flag) {
        if (!node) return;
        if (!flag) {
            if (node->left && node->left->left && (!node->right || node->left->left->size > node->right->size)) {
                right_rotate(node);
            } else if (node->left && node->left->right && (!node->right || node->left->right->size > node->right->size)) {
                left_rotate(node->left);
                right_rotate(node);
            } else {
                return;
            }
        } else {
            if (node->right && node->right->right && (!node->left || node->right->right->size > node->left->size)) {
                left_rotate(node);
            } else if (node->right && node->right->left && (!node->left || node->right->left->size > node->left->size)) {
                right_rotate(node->right);
                left_rotate(node);
            } else {
                return;
            }
        }
        maintain(node->left, false);
        maintain(node->right, true);
        maintain(node, false);
        maintain(node, true);
    }

    bool insert(Ref<Node<T, V>> &node, const FFDBKey<T> &key, const V &value) {
        if (!node) {
            node = current_db()->malloc<Node<T, V>>();
            new (&*node) Node<T, V>(key, value, current_db()->get_txn_id());
            ++_size;
            return true;
        }
        if (current_db()->get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }
        bool res;
        if (key < node->key) {
            res = insert(node->left, key, value);
        } else if (key > node->key) {
            res = insert(node->right, key, value);
        } else {
            return false;
        }
        update_size(node);
        maintain(node, key > node->key);
        return res;
    }

    bool update(Ref<Node<T, V>> &node, const FFDBKey<T> &key, const V &value) {
        if (!node) return false;
        if (current_db()->get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }
        if (key < node->key) {
            return update(node->left, key, value);
        } else if (key > node->key) {
            return update(node->right, key, value);
        } else {
            node->value = value;
            return true;
        }
    }

    Ref<Node<T, V>> search(Ref<Node<T, V>> node, const FFDBKey<T> &key) const {
        if (!node || node->key == key) return node;
        if (key < node->key) return search(node->left, key);
        return search(node->right, key);
    }

    bool remove(Ref<Node<T, V>> &node, const FFDBKey<T> &key) {
        if (!node) return false;
        if (current_db()->get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }
        bool res;
        if (key < node->key) {
            res = remove(node->left, key);
        } else if (key > node->key) {
            res = remove(node->right, key);
        } else {
            if (!node->left || !node->right) {
                Ref<Node<T, V>> tmp = node->left ? node->left : node->right;
                free_node(node);
                node = tmp;
                --_size;
                return true;
            } else {
                Ref<Node<T, V>> succ = node->right;
                while (succ->left) succ = succ->left;
                node->key = succ->key;
                node->value = succ->value;
                res = remove(node->right, succ->key);
            }
        }
        if (node) update_size(node);
        return res;
    }

    template <typename Func>
    void in_order(Ref<Node<T, V>> node, Func &visit) const {
        if (!node) return;
        in_order(node->left, visit);
        visit(node->key, node->value);
        in_order(node->right, visit);
    }

public:
    explicit SBT()
        : root(Ref<Node<T, V>>::null()), _size(0) {}

    // ~SBT() { destroy(root); }

    bool insert(const FFDBKey<T> &key, const V &value) { return insert(root, key, value); }

    bool update(const FFDBKey<T> &key, const V &value) { return update(root, key, value); }

    bool search(const FFDBKey<T> &key, V &value) const {
        Ref<Node<T, V>> node = search(root, key);
        if (node) {
            value = node->value;
            return true;
        }
        return false;
    }

    bool remove(const FFDBKey<T> &key) { return remove(root, key); }

    template <typename Func>
    void in_order(Func visit) const { in_order(root, visit); }

    int size() const { return _size; }

    Ref<Node<T, V>> get_root() const { return root; }
};

}

#endif //SBT_H
