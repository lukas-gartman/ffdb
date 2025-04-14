#include "buddy_allocator.h"
#include <cstdlib>
#include <stdexcept>

BuddyAllocator::BuddyAllocator(size_t init_size, size_t block_size) {
    this->block_size = block_size;

    free_list = (BlockDescriptor *)::malloc(sizeof(BlockDescriptor));
    free_list->obj = 0;
    free_list->size = init_size;
    free_list->next = nullptr;
}

Object BuddyAllocator::malloc_internal(size_t bytes) {
    BlockDescriptor *current = free_list;
    BlockDescriptor *prev = nullptr;

    // Find the first block that is large enough
    while (current != nullptr && current->size < bytes) {
        prev = current;
        current = current->next;
    }

    // No block is large enough
    if (current == nullptr) { throw std::bad_alloc(); }

    // Split the block if it is larger than the requested size
    if (current->size > bytes) {
        split_block(current, bytes);
    }

    // Remove the current block from the free list (pointer magic)
    if (prev != nullptr) {
        prev->next = current->next;
    } else {
        free_list = current->next;
    }

    Object obj = current->obj;
    ::free(current);
    return obj;
}

void BuddyAllocator::free_internal(Object obj, size_t bytes) {
    BlockDescriptor *bd = (BlockDescriptor *)::malloc(sizeof(BlockDescriptor));
    bd->obj = obj;
    bd->size = bytes;
    bd->next = nullptr;

    BlockDescriptor *current = free_list;
    BlockDescriptor *prev = nullptr;

    // Find the correct position to insert the block
    while (current != nullptr && current->obj < obj) {
        prev = current;
        current = current->next;
    }

    // Insert the block back into the free list
    if (prev != nullptr) {
        prev->next = bd;
    } else {
        free_list = bd;
    }

    bd->next = current;
    coalesce_blocks();
}

void BuddyAllocator::split_block(BlockDescriptor *block, size_t size) {
    while (block->size > size && block->size > block_size) {
        size_t new_size = block->size / 2;
        BlockDescriptor *new_block = (BlockDescriptor *)::malloc(
            sizeof(BlockDescriptor));
        new_block->obj = block->obj + new_size;
        new_block->size = new_size;
        new_block->next = block->next;
        block->size = new_size;
        block->next = new_block;
    }
}

void BuddyAllocator::coalesce_blocks() {
    BlockDescriptor *current = free_list;

    while (current != nullptr && current->next != nullptr) {
        // If two blocks are contiguous, merge them
        if (current->obj + current->size == current->next->obj) {
            current->size *= 2;
            BlockDescriptor *temp = current->next;
            current->next = current->next->next;
            ::free(temp);
        } else { // Move to the next block
            current = current->next;
        }
    }
}