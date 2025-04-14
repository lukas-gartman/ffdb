//
// Created by andreasp on 2025-03-28.
//

#ifndef REF_LIST_H
#define REF_LIST_H

#include <stdexcept>
#include "typedefs.h"

template <typename T>
class RefList {
public:
    struct Node {
        T val;
        Ref<Node> next;
    };

    class Iterator {
        friend class RefList;
    private:
        Iterator(Ref<Node> start, Ref<RefList> parent_list):
            prev(0), current(start), parent_list(parent_list) {}

        Ref<Node> prev;
        Ref<Node> current;
        Ref<RefList> parent_list;

    public:
        void next();
        Ref<Node> remove();
        Ref<Node> get() const;
    };

    RefList(): head(0) {}

    void push(Ref<Node> node);
    Ref<Node> pop();
    Ref<Node> top() const;
    bool is_empty() const;
    Iterator get_iterator() const;

private:
    Ref<Node> head;
};

template <typename T>
void RefList<T>::Iterator::next() {
    prev = current;
    current = current->next;
}

template <typename T>
Ref<typename RefList<T>::Node> RefList<T>::Iterator::remove() {
    if (current == 0) throw std::runtime_error("End of list");

    Ref<Node> to_remove = current;
    current = current->next;

    if (prev != 0) prev->next = current;
    else parent_list->head = current;

    to_remove->next = 0;

    return to_remove;
}

template <typename T>
Ref<typename RefList<T>::Node> RefList<T>::Iterator::get() const {
    return current;
}

template <typename T>
void RefList<T>::push(Ref<Node> node){
    node->next = head;
    head = node;
}

template <typename T>
Ref<typename RefList<T>::Node> RefList<T>::pop() {
    if (is_empty()) throw std::runtime_error("Empty list.");

    Ref<Node> to_remove = head;
    head = head->next;

    to_remove->next = 0;

    return to_remove;
}

template <typename T>
Ref<typename RefList<T>::Node> RefList<T>::top() const {
    return head;
}

template <typename T>
bool RefList<T>::is_empty() const {
    return head == 0;
}

template <typename T>
typename RefList<T>::Iterator RefList<T>::get_iterator() const {
    return Iterator(head, Ref<RefList>::from_ptr(const_cast<RefList*>(this)));
}

#endif //REF_LIST_H