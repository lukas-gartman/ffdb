#ifndef FFDBKEY_H
#define FFDBKEY_H

#include <concepts>
#include <array>
#include <cstring>
#include <variant>

#include "FFDB.h"
#include "Typedefs.h"

const size_t PREFIX_SIZE = 8;

template <PrimitiveVal T>
class FFDBKey {
public:
    // The variant used is as big as this struct anyways so might as well store everything in this struct.
    struct LargeKey {
        std::byte key_prefix[PREFIX_SIZE];
        Ref<std::byte> key_ref;
    };

    size_t size;
    std::variant<T, LargeKey> key_storage;

    FFDBKey() : key_storage(T{}) {}

    template<size_t n>
    FFDBKey(const T (&key)[n]) {
        size = sizeof(T) * n;

        if (n == 1 && size <= PREFIX_SIZE) key_storage = key[0];
        else {
            LargeKey lk;
            std::memcpy(lk.key_prefix, key, std::min(PREFIX_SIZE, size));

            if (size > PREFIX_SIZE) {
                Size of_size = size - PREFIX_SIZE;
                lk.key_ref = current_db()->malloc<std::byte>(of_size);
                std::memcpy(lk.key_ref, ((std::byte*) key) + PREFIX_SIZE, of_size);
            }

            key_storage = lk;
        }
    }

    // Syntax jiu jitsu.
    FFDBKey(const T &key): FFDBKey((const T[1]) {key}) {}

    static int cmp(const FFDBKey<T> &key, const FFDBKey<T> &other) {
        if (key.size != other.size)
            return key.size < other.size ? -1 : 1;

        // If this holds other.key_storage also has a T.
        if (std::holds_alternative<T>(key.key_storage)) {
            const T &sk1 = std::get<T>(key.key_storage);
            const T &sk2 = std::get<T>(other.key_storage);
            return sk1 < sk2 ? -1
                 : sk1 > sk2 ? 1
                 : 0;
        }

        const LargeKey &lk1 = std::get<LargeKey>(key.key_storage);
        const LargeKey &lk2 = std::get<LargeKey>(other.key_storage);

        auto p_cmp = memcmp(lk1.key_prefix, lk2.key_prefix, std::min(key.size, PREFIX_SIZE));
        if (p_cmp != 0) return p_cmp;

        if (key.size > PREFIX_SIZE)
            return memcmp(lk1.key_ref, lk2.key_ref, key.size - PREFIX_SIZE);;

        return 0;
    }

    template<size_t n>
    static int cmp(const FFDBKey<T> &key, const T(&other)[n]) {
        Size other_size = sizeof(T) * n;
        if (key.size < other_size)
            return key.size < other_size ? -1
                 : key.size > other_size ? 1
                 : 0;

        // If this holds n is guaranteed to be 1 because the if above.
        if (std::holds_alternative<T>(key.key_storage)) {
            const T &sk = std::get<T>(key.key_storage);
            return sk < other[0] ? -1
                 : sk > other[0] ? 1
                 : 0;
        }

        const LargeKey &lk = std::get<LargeKey>(key.key_storage);

        auto p_cmp = memcmp(lk.key_prefix, other, std::min(key.size, PREFIX_SIZE));
        if (p_cmp != 0) return p_cmp;

        if (key.size > PREFIX_SIZE)
            return memcmp(lk.key_ref, ((const std::byte*) other) + PREFIX_SIZE, key.size - PREFIX_SIZE);;

        return 0;
    }

    static int cmp(const FFDBKey<T> &key, const T &other) {
        T arr[1] = { other };
        return cmp(key, arr);
    }

    void free_ref() {
        if (std::holds_alternative<LargeKey>(key_storage)) {
            Ref<std::byte> ref = std::get<LargeKey>(key_storage).key_ref;
            if (ref == 0) return; // No extra data was needed

            current_db()->free(ref, size - PREFIX_SIZE);
        }
    }

    template<size_t n> FFDBKey<T>& operator=(const FFDBKey<T> &other) {
        if (this == &other) return *this;

        free_ref();

        size = other.size;
        key_storage = other.key_storage;

        return *this;
    }

    template<size_t n> FFDBKey<T>& operator=(const T(&other)[n]) {
        FFDBKey<T> tmp(other);
        *this = tmp;
        return *this;
    }

    template<size_t n> FFDBKey<T>& operator=(const T &other) {
        FFDBKey<T> tmp(other);
        *this = tmp;
        return *this;
    }

    // Boilerplate from hell.
    bool operator==(const FFDBKey<T> &other) const { return cmp(*this, other) == 0; }
    bool operator!=(const FFDBKey<T> &other) const { return cmp(*this, other) != 0; }
    bool operator>=(const FFDBKey<T> &other) const { return cmp(*this, other) >= 0; }
    bool operator<=(const FFDBKey<T> &other) const { return cmp(*this, other) <= 0; }
    bool operator> (const FFDBKey<T> &other) const { return cmp(*this, other) > 0; }
    bool operator< (const FFDBKey<T> &other) const { return cmp(*this, other) < 0; }

    template<size_t n> bool operator==(const T(&other)[n]) const { return cmp(*this, other) == 0; }
    template<size_t n> bool operator!=(const T(&other)[n]) const { return cmp(*this, other) != 0; }
    template<size_t n> bool operator>=(const T(&other)[n]) const { return cmp(*this, other) >= 0; }
    template<size_t n> bool operator<=(const T(&other)[n]) const { return cmp(*this, other) <= 0; }
    template<size_t n> bool operator> (const T(&other)[n]) const { return cmp(*this, other) > 0; }
    template<size_t n> bool operator< (const T(&other)[n]) const { return cmp(*this, other) < 0; }

    bool operator==(const T &other) const { return cmp(*this, other) == 0; }
    bool operator!=(const T &other) const { return cmp(*this, other) != 0; }
    bool operator>=(const T &other) const { return cmp(*this, other) >= 0; }
    bool operator<=(const T &other) const { return cmp(*this, other) <= 0; }
    bool operator> (const T &other) const { return cmp(*this, other) > 0; }
    bool operator< (const T &other) const { return cmp(*this, other) < 0; }

    friend std::ostream &operator<<(std::ostream &os, const FFDBKey<T> &key) {
        if (std::holds_alternative<T>(key.key_storage)) {
            os << std::get<T>(key.key_storage);
        } else {
            const auto& lk = std::get<typename FFDBKey<T>::LargeKey>(key.key_storage);
            os << "[";
            const std::byte* data = &*lk.key_ref;
            for (size_t i = 0; i < key.size; ++i) {
                os << std::hex << std::to_integer<int>(data[i]) << " ";
            }
            os << "]";
        }
        return os;
    }
};

// Boilerplate hell the sequel
template<PrimitiveVal T, size_t n> bool operator==(const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) == 0; }
template<PrimitiveVal T, size_t n> bool operator!=(const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) != 0; }
template<PrimitiveVal T, size_t n> bool operator>=(const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) <= 0; }
template<PrimitiveVal T, size_t n> bool operator<=(const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) >= 0; }
template<PrimitiveVal T, size_t n> bool operator> (const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) < 0; }
template<PrimitiveVal T, size_t n> bool operator< (const T(&other)[n], const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) > 0; }

template<PrimitiveVal T> bool operator==(const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) == 0; }
template<PrimitiveVal T> bool operator!=(const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) != 0; }
template<PrimitiveVal T> bool operator>=(const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) <= 0; }
template<PrimitiveVal T> bool operator<=(const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) >= 0; }
template<PrimitiveVal T> bool operator> (const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) < 0; }
template<PrimitiveVal T> bool operator< (const T &other, const FFDBKey<T> &key) { return FFDBKey<T>::cmp(key, other) > 0; }

#endif //FFDBKEY_H