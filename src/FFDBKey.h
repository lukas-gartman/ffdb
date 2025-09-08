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

    FFDBKey() : size(0), key_storage(T{}) {}

    FFDBKey(const T *key, Size n) {
        size = n * sizeof(T);

        if (n == 1 && size <= PREFIX_SIZE) key_storage = *key;
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

    template<size_t n>
    FFDBKey(const T (&key)[n]): FFDBKey(key, n) {}

    FFDBKey(const std::string &key): FFDBKey(key.c_str(), key.length() + 1) {}

    FFDBKey(const T &key): FFDBKey(&key, 1) {}

    void free_ref() {
        if (std::holds_alternative<LargeKey>(key_storage)) {
            Ref<std::byte> ref = std::get<LargeKey>(key_storage).key_ref;
            if (ref == 0) return; // No extra data was needed

            current_db()->free(ref, size - PREFIX_SIZE);
        }
    }

    static int cmp(const FFDBKey &key, const FFDBKey &other) {
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
            return memcmp(lk1.key_ref, lk2.key_ref, key.size - PREFIX_SIZE);

        return 0;
    }

    static int cmp(const FFDBKey &key, const T* other, Size other_n) {
        Size other_size = other_n * sizeof(T);
        if (key.size != other_size)
            return key.size < other_size ? -1
                 : key.size > other_size ? 1
                 : 0;

        if (std::holds_alternative<T>(key.key_storage)) {
            const T &sk = std::get<T>(key.key_storage);
            return sk < *other ? -1
                 : sk > *other ? 1
                 : 0;
        }

        const LargeKey &lk = std::get<LargeKey>(key.key_storage);

        auto p_cmp = memcmp(lk.key_prefix, other, std::min(key.size, PREFIX_SIZE));
        if (p_cmp != 0) return p_cmp;

        if (key.size > PREFIX_SIZE)
            return memcmp(lk.key_ref, ((const std::byte*) other) + PREFIX_SIZE, key.size - PREFIX_SIZE);

        return 0;
    }

    template<size_t n>
    static int cmp(const FFDBKey &key, const T(&other)[n]) {
        return cmp(key, other, n);
    }

    static int cmp(const FFDBKey &key, const T &other) {
        return cmp(key, &other, 1);
    }

    template<size_t n> FFDBKey& operator=(const T(&other)[n]) {
        FFDBKey tmp(other);
        *this = tmp;
        return *this;
    }

    FFDBKey& operator=(const T &other) {
        FFDBKey tmp(other);
        *this = tmp;
        return *this;
    }

    auto operator<=>(const FFDBKey<T>& other) const {
        return cmp(*this, other);
    }

    bool operator==(const FFDBKey<T>& other) const {
        return cmp(*this, other) == 0;
    }

    template<size_t n>
    auto operator<=>(const T(&other)[n]) const {
        return cmp(*this, other, n);
    }

    template<size_t n>
    bool operator==(const T(&other)[n]) const {
        return cmp(*this, other, n) == 0;
    }

    auto operator<=>(const T& other) const {
        return cmp(*this, other);
    }

    bool operator==(const T& other) const {
        return cmp(*this, other) == 0;
    }

    auto operator<=>(const std::string& other) const {
        return cmp(*this, other.c_str(), other.length() + 1);
    }

    bool operator==(const std::string& other) const {
        return cmp(*this, other.c_str(), other.length() + 1) == 0;
    }

    friend std::ostream &operator<<(std::ostream &os, const FFDBKey &key) {
        if (std::holds_alternative<T>(key.key_storage))
            os << std::get<T>(key.key_storage);
        else {
            const auto& lk = std::get<LargeKey>(key.key_storage);

            T* result = new T[key.size / sizeof(T)];

            std::memcpy(result, lk.key_prefix, PREFIX_SIZE);
            if (key.size > PREFIX_SIZE) std::memcpy(result + PREFIX_SIZE, lk.key_ref, key.size - PREFIX_SIZE);

            if constexpr (std::is_same_v<T, char>) {
                os << "\"";
                for (size_t i = 0; i < key.size -1; ++i) {
                    os << result[i];
                }
                os << "\"";
            }
            else {
                os << "[";
                for (size_t i = 0; i < key.size / sizeof(T); ++i) {
                    os << result[i];

                    if (i != (key.size / sizeof(T)) - 1) os << " ";
                }
                os << "]";
            }
        }
        return os;
    }
};

#endif //FFDBKEY_H