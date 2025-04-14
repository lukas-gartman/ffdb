#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "typedefs.h"

#include <stdexcept>

class Allocator {
public:
    /* Represents the global state of the database. That is the state that is
     * stored in the memory mapped file and shared among processes.
     *
     * It will not be a part of the allocator in the future.
     */
    struct State {
        std::size_t file_size;

        Object top; // Global top of the memory map.
        Object free_list; // List of free blocks.
        Object free_descriptors; // List of descriptors that are currently unused.
    };

    Allocator(State *state, std::size_t memory_map_size);

    template <class A>
    Ref<A> malloc(const std::size_t extra_bytes = 0) {
        return malloc_internal(sizeof(A) + extra_bytes);
    }

    template<class A>
    void free(Ref<A> obj, const std::size_t extra_bytes = 0) {
        free_internal(obj.as_object(), sizeof(A) + extra_bytes);
    }

private:
    /* A Block is a structure representing a block of memory.
     */
    struct Block {
        Object obj;
        std::size_t size;
    };

    /* A Block Descriptor is a structure that describes a free block of memory.
     * Multiple Block descriptors form a linked list of free blocks to make
     * free blocks easy to find.
     */
    struct BlockDescriptor {
        Block block;
        Ref<BlockDescriptor> next;
    };

    State *state;
    std::size_t memory_map_size;

    Object local_top; // During a transaction we don't want to change the
                      // global state until we commit. Hence why we need
                      // a local top.

    Block last_free_block; // Caches the block that was freed last.
                           // This is used to mitigate fragmentation.

    Object malloc_internal(std::size_t bytes);
    void free_internal(Object obj, std::size_t bytes);

    // Used for checking for free blocks to mitigate fragmentation.
    Ref<BlockDescriptor> block_descriptor_list;

    // Used for checking for free block descriptors for reuse.
    Ref<BlockDescriptor> unused_block_descriptor_list;

    /* Creates a new block descriptor if there is no unused block descriptor,
     * otherwise we can reuse an unused block descriptor.
     */
    Ref<BlockDescriptor> new_block_descriptor(
        Object obj,
        std::size_t block_size
    );

    /* A block has been freed so we need to get a block descriptor for it.
     */
    void insert_block_descriptor(
        Object obj,
        std::size_t block_size
    );

    /* We are allocating a block of memory. We try to find a free block
     * that fits the size needs. If one is found we will return the offset
     * of the block. If no block is found we will return 0.
     */
    Object delete_block_descriptor(std::size_t block_size);
};

#endif //ALLOCATOR_H
