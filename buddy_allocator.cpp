#include "buddy_allocator.h"

#include <cstdlib>

Object BuddyAllocator::malloc_internal(size_t bytes) {
    BlockDescriptor *top = free_list;
    free_list = free_list->next;
    Object object = top->obj;
    ::free(top);

    return object;
}

BuddyAllocator::BuddyAllocator(size_t init_size, size_t block_size) {
    this->block_size = block_size;

    size_t current_offset = 0;
    BlockDescriptor *bd = nullptr;
    BlockDescriptor *prev_bd = nullptr;
    while (current_offset < init_size) {
        bd = (BlockDescriptor *)::malloc(sizeof(BlockDescriptor));
        bd->obj = current_offset;
        bd->size = block_size;
        if (prev_bd != nullptr) {
            prev_bd->next = bd;
        } else {
            free_list = bd;
        }
        prev_bd = bd;
        current_offset += block_size;
    }
}