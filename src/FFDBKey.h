#ifndef FFDBKEY_H
#define FFDBKEY_H

#include <concepts>
#include <array>
#include <cstring>
#include "Typedefs.h"

/**
 * \brief Ensures that the key type is comparable to other keys.
 * \details This concept checks that the type `T` supports comparison operators
 *          such as `<`, `<=`, `>`, `>=`, `==`, and `!=`, all of which must return
 *          a value convertible to `bool`.
 */
template <typename T>
concept Comparable = requires(T a, T b) {
    { a < b }  -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b }  -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

const int PREFIX_SIZE = 8;

template <Comparable T>
class FFDBKey {
public:
    std::array<std::byte, PREFIX_SIZE> key_prefix;
    Ref<T> key_ref;

    FFDBKey(const T &key) {
        std::memcpy(key_prefix.data(), &key, PREFIX_SIZE);
        
        Ref<T> ref = current_db()->malloc<T>();
        new (&*ref) T(key);
        key_ref = ref;
    }

    bool operator< (const FFDBKey &other) const {
        if (key_prefix < other.key_prefix) return true;
        if (key_prefix > other.key_prefix) return false;
        return *key_ref < *other.key_ref;
    }

    bool operator<=(const FFDBKey &other) const {
        if (key_prefix < other.key_prefix) return true;
        if (key_prefix > other.key_prefix) return false;
        return *key_ref <= *other.key_ref;
    }

    bool operator> (const FFDBKey &other) const {
        if (key_prefix > other.key_prefix) return true;
        if (key_prefix < other.key_prefix) return false;
        return *key_ref > *other.key_ref;
    }

    bool operator>=(const FFDBKey &other) const {
        if (key_prefix > other.key_prefix) return true;
        if (key_prefix < other.key_prefix) return false;
        return *key_ref >= *other.key_ref;
    }

    bool operator!=(const FFDBKey &other) const {
        return key_prefix != other.key_prefix || *key_ref != *other.key_ref;
    }
    
    bool operator==(const FFDBKey &other) const {
        return key_prefix == other.key_prefix && *key_ref == *other.key_ref;
    }
};

#endif //FFDBKEY_H