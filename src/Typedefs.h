#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <cstdint>
#include <cstddef>
#include <unistd.h>

extern __thread unsigned char *current_base __attribute__((tls_model(
    "initial-exec")));

const size_t SYSTEM_PAGE_SIZE = sysconf(_SC_PAGESIZE);
const size_t LOGICAL_PAGE_SIZE = 2048; // Undefined behaviour if not a power of two
const size_t MIN_PAGE_SIZE = SYSTEM_PAGE_SIZE * 2;

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