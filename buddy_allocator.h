#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include "typedefs.h"
#include <cstddef>

struct BlockDescriptor {
    Object obj;
    size_t size;
    BlockDescriptor *next;
};

class BuddyAllocator {
private:
    BlockDescriptor* free_list;
    size_t block_size;

    Object malloc_internal(size_t bytes);
    void free_internal(Object obj, size_t bytes);
    void split_block(BlockDescriptor* block, size_t size);
    void coalesce_blocks();

public:
    BuddyAllocator(size_t init_size, size_t block_size);

    template <class A> Ref<A> malloc(size_t extra_bytes=0) {
        return malloc_internal(sizeof(A) + extra_bytes);
    }

    template<class A> void free(Ref<A> obj, size_t extra_bytes=0) {
        free_internal(obj.as_object(), sizeof(A) + extra_bytes);
    }
};

#endif //BUDDY_ALLOCATOR_H