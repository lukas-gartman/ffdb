#include "buddy_allocator.h"

#include <cstdlib>

void BuddyAllocator::free_internal(Object obj, size_t bytes) {
    BlockDescriptor *bd = (BlockDescriptor *)::malloc(sizeof(BlockDescriptor));
    bd->obj = obj;
    bd->size = block_size;

    BlockDescriptor *current_bd = free_list;
    BlockDescriptor *prev_bd = nullptr;

    while (bd->obj > current_bd->obj) {
        prev_bd = current_bd;
        current_bd = current_bd->next;
    }

    if (prev_bd != nullptr) {
        prev_bd->next = bd;
    } else {
        free_list = bd;
    }


    if (bd->next != nullptr) bd->next = current_bd;

}

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