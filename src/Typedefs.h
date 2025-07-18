#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <cstdint>
#include <cstddef>
#include <unistd.h>

extern __thread unsigned char *current_base __attribute__((tls_model(
    "initial-exec")));

/** \brief The physical page size on the system.
 *
 *  The size (in bytes) of a physical memory page on the system. Is primarily used by the memory mapping
 *  (\ref MemoryMapping).
 *
 *  \see MemoryMapping
 */
const size_t SYSTEM_PAGE_SIZE = sysconf(_SC_PAGESIZE);

/** \brief The cache line size on the system.
 *
 *  The size (in bytes) of a cache line on the system.
 */
const size_t CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

/** \brief The logical page size within the database.
 *
 *  The size (in bytes) of a logical memory page on the system. Is primarily used by the allocator (\ref MemoryMapping)
 *  for memory alignment purposes. This constant may be tweaked to optimize memory usage and performance but it needs to
 *  be a multiple of the physical page size. It should not be to small.
 *
 *  \see Allocator
 */
const size_t LOGICAL_PAGE_SIZE = SYSTEM_PAGE_SIZE;

/** \brief The minimum size needed for the database.
 *
 *  The minimum size (in bytes) the database needs to be. We need at least two physical pages. One for storing the
 *  shared state and one for the actual database content.
 *
 *  \see MemoryMapping
 */
const size_t MIN_DB_SIZE = SYSTEM_PAGE_SIZE * 2;

/** \brief Alignment used for smaller objects.
 *
 *  The alignment (in bytes) used for smaller objects in the database. This is used for objects smaller than
 *  \ref ALIGNMENT_THRESHOLD. This constant may be tweaked to optimize memory usage and performance. Should not be too
 *  small.
 *
 *  \see Allocator::align_bytes
 */
const size_t SMALL_ALIGNMENT = 16;

/** \brief Alignment used for large.
 *
 *  The alignment (in bytes) used for large objects in the database. This is used for objects larger than or equal to
 *  \ref ALIGNMENT_THRESHOLD. This constant may be tweaked to optimize memory usage and performance
 *
 *  \see Allocator::align_bytes
 */
const size_t LARGE_ALIGNMENT = LOGICAL_PAGE_SIZE;

/** \brief Threshold that determines which alignment to use.
 *
 *  The threshold (in bytes) used to determine which alignment to use.
 *
 *  \see Allocator::align_bytes
 */
const size_t ALIGNMENT_THRESHOLD = LARGE_ALIGNMENT;

typedef uintptr_t Object;

template <class A> class Ref {
private:
    Object offset;

public:
    Ref(): offset(0) {}
    Ref(const Object o): offset(o) {}

    A *operator->() const { return (A *)(current_base + offset); }
    operator A *() const { return (A *)(current_base + offset); }

    bool operator ==(const Ref &other) const {
        return offset == other.as_object();
    }

    bool operator !=(const Ref &other) const {
        return offset != other.as_object();
    }

    bool operator ==(const Object other_offset) const {
        return offset == other_offset;
    }

    bool operator !=(const Object other_offset) const {
        return offset != other_offset;
    }

    Ref &operator=(const Ref &r) {
        offset = r.offset;
        return *this;
    }

    static Ref from_ptr(A *ptr) {
        return ((unsigned char *)(ptr) - current_base);
    }

    Object as_object() const { return offset; }

    static Ref null() { return Ref(0); }

    explicit operator bool() const { return offset != 0; }
};

#endif //TYPEDEFS_H