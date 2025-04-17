#ifndef TYPEDEFS_H
#define TYPEDEFS_H
#include <cstdint>

extern __thread unsigned char *current_base __attribute__((tls_model(
    "initial-exec")));

typedef uintptr_t Object;

template <class A> class Ref {
private:
    Object offset;

public:
    Ref(const Object o) { offset = o; }

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
};

#endif //TYPEDEFS_H