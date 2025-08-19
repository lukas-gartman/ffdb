#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <cstdint>
#include <cstddef>
#include <unistd.h>
#include <stdexcept>

class FFDB;

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

/** \brief The physical page size on the system.
 *
 *  The size (in bytes) of a physical memory page on the system. Is primarily used by the memory mapping
 *  (\ref MemoryMapping).
 *
 *  \see MemoryMapping
 */
const Size SYSTEM_PAGE_SIZE = sysconf(_SC_PAGESIZE);

/** \brief The cache line size on the system.
 *
 *  The size (in bytes) of a cache line on the system.
 */
const Size CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

/** \brief The logical page size within the database.
 *
 *  The size (in bytes) of a logical memory page on the system. Is primarily used by the allocator (\ref MemoryMapping)
 *  for memory alignment purposes. This constant may be tweaked to optimize memory usage and performance but it needs to
 *  be a multiple of the physical page size. It should not be to small.
 *
 *  \see Allocator
 */
const Size LOGICAL_PAGE_SIZE = SYSTEM_PAGE_SIZE;

/** \brief The minimum size needed for the database.
 *
 *  The minimum size (in bytes) the database needs to be. We need at least two physical pages. One for storing the
 *  shared state and one for the actual database content.
 *
 *  \see MemoryMapping
 */
const Size MIN_DB_SIZE = SYSTEM_PAGE_SIZE * 2;

/** \brief Alignment used for smaller objects.
 *
 *  The alignment (in bytes) used for smaller objects in the database. This is used for objects smaller than
 *  \ref ALIGNMENT_THRESHOLD. This constant may be tweaked to optimize memory usage and performance. Should not be too
 *  small.
 *
 *  \see Allocator::align_bytes
 */
const Size SMALL_ALIGNMENT = 16;

/** \brief Alignment used for large.
 *
 *  The alignment (in bytes) used for large objects in the database. This is used for objects larger than or equal to
 *  \ref ALIGNMENT_THRESHOLD. This constant may be tweaked to optimize memory usage and performance
 *
 *  \see Allocator::align_bytes
 */
const Size LARGE_ALIGNMENT = LOGICAL_PAGE_SIZE;

/** \brief Threshold that determines which alignment to use.
 *
 *  The threshold (in bytes) used to determine which alignment to use.
 *
 *  \see Allocator::align_bytes
 */
const Size ALIGNMENT_THRESHOLD = LARGE_ALIGNMENT;

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

template<typename T>
concept PrimitiveVal = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <PrimitiveVal T>
class FFDBKey;

// Ancient Mesopotamian programancy.
template <typename U, typename T>
concept ValidKeyArg = std::same_as<std::remove_cvref_t<U>, T>                  // Allow T
        || std::same_as<std::remove_cvref_t<U>, FFDBKey<T>>                    // Allow FFDBKey<T>
        || (std::is_array_v<std::remove_cvref_t<U>>                            // Allow T[n]
            && std::same_as<std::remove_extent_t<std::remove_cvref_t<U>>, T>);

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