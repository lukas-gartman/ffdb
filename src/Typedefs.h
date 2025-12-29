#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <cstdint>
#include <stdexcept>

class FFDB;

#ifndef FFDB_ALIGN
#define FFDB_ALIGN 16 // Bits
#endif

#ifndef FFDB_PHYSICAL_PAGE_SIZE
#define FFDB_PHYSICAL_PAGE_SIZE 4096 // Bytes
#endif

#ifndef FFDB_LOGICAL_PAGE_SIZE
#define FFDB_LOGICAL_PAGE_SIZE 512 // Bytes
#endif

#ifndef FFDB_PAGE_BOUNDARY_AWARE
#define FFDB_PAGE_BOUNDARY_AWARE true
#endif

#ifndef FFDB_KEY_PREFIX_SIZE
#define FFDB_KEY_PREFIX_SIZE 8
#endif

extern __thread unsigned char *current_base __attribute__((tls_model("initial-exec")));
extern __thread FFDB *_current_db __attribute__((tls_model("initial-exec")));
inline FFDB *current_db() {
    if (!_current_db) {
        throw std::runtime_error("FFDB: the database is not initialized. Did you forget to create an FFDB instance and start a transaction?");
    }
    return _current_db;
}

typedef uintptr_t Offset;
typedef size_t Size;
typedef size_t Id;

template<typename T>
concept PrimitiveVal = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <PrimitiveVal T>
class FFDBKey;

// Ancient Mesopotamian programancy.
template <typename U, typename T>
concept ValidKeyArg = std::same_as<std::remove_cvref_t<U>, T>                 // T, T&, T&&
    || std::same_as<std::remove_cvref_t<U>, FFDBKey<T>>                       // FFDBKey<T>, FFDBKey<T>&, ...
    || (std::is_array_v<std::remove_cvref_t<U>>
        && std::same_as<std::remove_extent_t<std::remove_cvref_t<U>>, T>)     // T[n]
    || (std::same_as<T, char>
        && std::same_as<std::remove_cvref_t<U>, std::string>);                // std::string, std::string&, ... when T=char

template <class A> class Ref {
private:
    Offset offset;

public:
    Ref(): offset(0) {}
    Ref(const Offset o): offset(o) {}

    A *operator->() const { return (A *)(current_base + offset); }
    operator A *() const { return (A *)(current_base + offset); }

    bool operator ==(const Ref &other) const {
        return offset == other.as_offset();
    }

    bool operator !=(const Ref &other) const {
        return offset != other.as_offset();
    }

    bool operator ==(const Offset other_offset) const {
        return offset == other_offset;
    }

    bool operator !=(const Offset other_offset) const {
        return offset != other_offset;
    }

    Ref &operator=(const Ref &r) {
        offset = r.offset;
        return *this;
    }

    static Ref from_ptr(A *ptr) {
        return ((unsigned char *)(ptr) - current_base);
    }

    Offset as_offset() const { return offset; }

    static Ref null() { return Ref(0); }

    explicit operator bool() const { return offset != 0; }
};

#endif //TYPEDEFS_H