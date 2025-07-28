#ifndef BINARYTREE_H
#define BINARYTREE_H

#include <concepts>
#include "FFDB2.h"

namespace BinaryTree {

template <typename T>
concept Comparable = requires(T a, T b) {
    { a < b }  -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b }  -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

template <Comparable K, typename V>
class Node {
public:
    K key;
    V value;
    Ref<Node> left;
    Ref<Node> right;
    size_t txn_id;

    Node(const K& k, const V& v, Id txn_id)
        : key(k), value(v), left(Ref<Node>::null()), right(Ref<Node>::null()), txn_id(txn_id) {}
};

template <Comparable K, typename V>
class BinaryTree {
    Ref<Node<K, V>> root;
    FFDB& db;
    int _size;

    Ref<Node<K, V>> copy_node(Ref<Node<K, V>> node) {
        Ref<Node<K, V>> n = db.malloc<Node<K, V>>();
        *n = *node;
        n->txn_id = db.get_txn_id();
        return n;
    }

    void free_node(Ref<Node<K, V>> node) {
        db.free(node);
    }

    // void destroy(Ref<Node<K, V>> node) {
    //     if (!node) return;
    //     destroy(node->left);
    //     destroy(node->right);
    //     free_node(node);
    // }

    bool insert(Ref<Node<K, V>>& node, const K& key, const V& value) {
        if (!node) {
            node = db.malloc<Node<K, V>>();
            *node = Node<K, V>(key, value, db.get_txn_id());
            ++_size;
            return true;
        }
        if (db.get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }
        if (key < node->key) {
            return insert(node->left, key, value);
        } else if (key > node->key) {
            return insert(node->right, key, value);
        } else {
            return false;
        }
    }

    bool update(Ref<Node<K, V>>& node, const K& key, const V& value) {
        if (!node) return false;
        if (db.get_txn_id() != node->txn_id) {
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

    Ref<Node<K, V>> search(Ref<Node<K, V>> node, const K& key) const {
        if (!node || node->key == key) return node;
        if (key < node->key) return search(node->left, key);
        return search(node->right, key);
    }

    bool remove(Ref<Node<K, V>>& node, const K& key) {
        if (!node) return false;
        if (db.get_txn_id() != node->txn_id) {
            node = copy_node(node);
        }
        if (key < node->key) {
            return remove(node->left, key);
        } else if (key > node->key) {
            return remove(node->right, key);
        } else {
            if (!node->left) {
                Ref<Node<K, V>> tmp = node->right;
                free_node(node);
                node = tmp;
                --_size;
                return true;
            } else if (!node->right) {
                Ref<Node<K, V>> tmp = node->left;
                free_node(node);
                node = tmp;
                --_size;
                return true;
            } else {
                Ref<Node<K, V>> succ = node->right;
                while (succ->left) succ = succ->left;
                node->key = succ->key;
                node->value = succ->value;
                return remove(node->right, succ->key);
            }
        }
    }

    template <typename Func>
    void in_order(Ref<Node<K, V>> node, Func& visit) const {
        if (!node) return;
        in_order(node->left, visit);
        visit(node->key, node->value);
        in_order(node->right, visit);
    }

public:
    explicit BinaryTree(FFDB& db)
        : root(Ref<Node<K, V>>::null()), db(db), _size(0) {}

    // ~BinaryTree() { destroy(root); }

    bool insert(const K& key, const V& value) { return insert(root, key, value); }

    bool update(const K &key, const V &value) { return update(root, key, value); }

    bool search(const K& key, V& value) const {
        Ref<Node<K, V>> node = search(root, key);
        if (node) {
            value = node->value;
            return true;
        }
        return false;
    }

    bool remove(const K& key) { return remove(root, key); }

    template <typename Func>
    void in_order(Func visit) const { in_order(root, visit); }

    int size() const { return _size; }
};

}

#endif //BINARYTREE_H
