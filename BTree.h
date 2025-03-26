// A B-Tree using CLRS's definition
// Introduction to Algorithms 3rd Edition (ISBN-10 978-0-262-03384-8)

#ifndef BTREE_H
#define BTREE_H

#include <vector>
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

template <Comparable T>
class BTree; // Forward declaration

template <Comparable T>
class Node {
private:
    std::vector<T> keys;
    std::vector<Node> *children;
    int t; // Minimum degree, the lower bound
    bool is_leaf;

public:
    Node(int t, bool is_leaf);

    Node *search(T key);
    void insertNonFull(T key);
    void splitChild(int i, Node *y);

    friend class BTree<T>;
};

template <Comparable T>
class BTree {
private:
    Node<T> *root;
    int t; // Minimum degree, the lower bound

public:
    explicit(false)
    BTree(int t);

    Node<T> *search(T key);
    void insert(T key);
    void remove(T key);
};

#endif //BTREE_H
