#ifndef FFDBKEY_H
#define FFDBKEY_H

#include <concepts>
#include <array>
#include <cstring>
#include <variant>
#include "FFDB2.h"
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

const size_t PREFIX_SIZE = 8;

template <Comparable T>
class FFDBKey {
public:
    struct LargeKey {
        std::array<std::byte, PREFIX_SIZE> key_prefix;
        Ref<T> key_ref;
        size_t size;
    };

    mutable std::variant<T, LargeKey> key_storage;

    FFDBKey() : key_storage(T{}) {}

    FFDBKey(const T &key) {
        if (sizeof(T) <= PREFIX_SIZE) {
            key_storage = key;
        } else {
            LargeKey lk;
            lk.key_prefix.fill(std::byte{0});
            size_t copy_size = std::min(PREFIX_SIZE, sizeof(T));
            std::memcpy(lk.key_prefix.data(), &key, copy_size);

            Ref<T> ref = current_db()->malloc<T>();
            lk.key_ref = ref;
            *ref = key;
            lk.size = sizeof(T);
            key_storage = lk;
        }
    }

    FFDBKey(const T *key, size_t n) {
        const size_t key_size = sizeof(T) * n;
        if (key_size <= PREFIX_SIZE) {
            T small_key{};
            std::memcpy(&small_key, key, key_size);
            key_storage = small_key;
        } else {
            LargeKey lk;
            lk.key_prefix.fill(std::byte{0});
            size_t copy_size = std::min(PREFIX_SIZE, key_size);
            std::memcpy(lk.key_prefix.data(), key, copy_size);

            Ref<T> ref = current_db()->malloc<T>(n);
            lk.key_ref = ref;
            std::memcpy(ref, key, key_size);
            lk.size = key_size;
            key_storage = lk;
        }
    }

    bool operator==(const FFDBKey &other) const {
        if (key_storage.index() != other.key_storage.index())
            return false;

        if (std::holds_alternative<T>(key_storage)) {
            return std::get<T>(key_storage) == std::get<T>(other.key_storage);
        } else {
            const LargeKey &a = std::get<LargeKey>(key_storage);
            const LargeKey &b = std::get<LargeKey>(other.key_storage);
            return a.key_prefix == b.key_prefix && std::memcmp(a.key_ref, b.key_ref, std::max(a.size, b.size)) == 0;
        }
    }

    bool operator<(const FFDBKey &other) const {
        if (key_storage.index() != other.key_storage.index())
            return key_storage.index() < other.key_storage.index();

        if (std::holds_alternative<T>(key_storage)) {
            return std::get<T>(key_storage) < std::get<T>(other.key_storage);
        } else {
            const LargeKey &a = std::get<LargeKey>(key_storage);
            const LargeKey &b = std::get<LargeKey>(other.key_storage);
            if (a.key_prefix < b.key_prefix) return true;
            if (a.key_prefix > b.key_prefix) return false;
            return std::memcmp(a.key_ref, b.key_ref, std::max(a.size, b.size)) < 0;
        }
    }

    bool operator!=(const FFDBKey &other) const { return !(*this == other); }
    bool operator> (const FFDBKey &other) const { return other < *this; }
    bool operator<=(const FFDBKey &other) const { return !(*this > other); }
    bool operator>=(const FFDBKey &other) const { return !(*this < other); }

    friend std::ostream &operator<<(std::ostream &os, const FFDBKey<T> &key) {
        if (std::holds_alternative<T>(key.key_storage)) {
            os << std::get<T>(key.key_storage);
        } else {
            const auto& lk = std::get<typename FFDBKey<T>::LargeKey>(key.key_storage);
            os << "[";
            const std::byte* data = reinterpret_cast<const std::byte*>(static_cast<const void*>(lk.key_ref));
            for (size_t i = 0; i < lk.size; ++i) {
                os << std::hex << std::to_integer<int>(data[i]) << " ";
            }
            os << "]";
        }
        return os;
    }
};

#endif //FFDBKEY_H