#ifndef FIXEDARRAY_H
#define FIXEDARRAY_H

#include <iostream>
#include <stdexcept>

template <typename T>
class FixedArray {
private:
    T* data;
    size_t _size;
    size_t _capacity;

public:
    FixedArray() : data(nullptr), _size(0), _capacity(0) {}

    explicit FixedArray(const size_t capacity) {
        data = new T[capacity];
        _size = 0;
        _capacity = capacity;
    }

    ~FixedArray() {
        delete[] data;
    }

    // std::move constructor for new objects
    FixedArray(FixedArray&& other) noexcept
        : data(other.data), _size(other._size), _capacity(other._capacity) {
        other.data = nullptr;
        other._size = 0;
        other._capacity = 0;
    }

    // std::move assignment operator for existing objects
    FixedArray& operator=(FixedArray&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            _size = other._size;
            _capacity = other._capacity;

            other.data = nullptr;
            other._size = 0;
            other._capacity = 0;
        }
        return *this;
    }

    // Access elements with bounds checking
    T& operator[](size_t index) {
        if (index >= _capacity) {
            throw std::out_of_range("Index out of bounds at index " + std::to_string(index) + " (size: " + std::to_string(_size) + ", capacity: " + std::to_string(_capacity) + ")");
        }
        if (index >= _size) {
            _size += 1; // Update size when expanding
        }
        return data[index];
    }

    // Access elements (const version)
    const T& operator[](size_t index) const {
        if (index >= _capacity) {
            throw std::out_of_range("Index out of bounds at index " + std::to_string(index) + " (size: " + std::to_string(_size) + ", capacity: " + std::to_string(_capacity) + ")");
        }
        return data[index];
    }

    // number of stored elements
    size_t size() const {
        return _size;
    }

    // max number of elements
    size_t capacity() const {
        return _capacity;
    }

    T erase(size_t index) {
        if (index >= _size) {
            throw std::out_of_range("Index out of bounds at index " + std::to_string(index) + " (size: " + std::to_string(_size) + ", capacity: " + std::to_string(_capacity) + ")");
        }

        T deletedElement = std::move(data[index]);
        for (size_t i = index; i < _size - 1; ++i) {
            data[i] = std::move(data[i + 1]);
        }
        --_size;
        return deletedElement;
    }

    void resize(const size_t newCapacity) {
        if (newCapacity == _capacity) return;

        T *_data = new T[newCapacity];
        for (size_t i = 0; i < std::min(_size, newCapacity); i++) {
            _data[i] = std::move(data[i]);
        }
        delete[] data;
        data = _data;
        _capacity = newCapacity;
        if (_size > newCapacity) {
            _size = newCapacity;
        }
    }
};

#endif // FIXEDARRAY_H