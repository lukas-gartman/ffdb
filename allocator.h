#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "typedefs.h"
#include "DBState.h"

#include <stdexcept>

class Allocator {
public:
    Allocator(DBState *state, std::size_t memory_map_size);

    template <class A> Ref<A> malloc(const std::size_t extra_bytes = 0) {
        return malloc_internal(sizeof(A) + extra_bytes);
    }

    template<class A> void free(Ref<A> obj, const std::size_t extra_bytes = 0) {
        free_internal(obj.as_object(), sizeof(A) + extra_bytes);
    }

private:
    struct Block {
        Object obj;
        std::size_t size;
    };

    struct BlockDescr {
        Block block;
        Ref<BlockDescr> next;
    };

    DBState *state;
    std::size_t memory_map_size;

    Object local_top;

    Block last_free_block;

    Object malloc_internal(std::size_t bytes);
    void free_internal(Object obj, std::size_t bytes);

    // Used for checking for free blocks to mitigate fragmentation.
    Ref<BlockDescr> block_descriptor_list;

    // Used for checking for free block descriptors for reuse.
    Ref<BlockDescr> unused_block_descriptor_list;

    Ref<BlockDescr> new_block_descriptor(
        Object obj,
        std::size_t block_size
    );

    void insert_block_descriptor(
        Object obj,
        std::size_t block_size
    );

    // Returns the offset of where we can allocate block_size bytes.
    Object delete_block_descriptor(std::size_t block_size);
};

#endif //ALLOCATOR_H
