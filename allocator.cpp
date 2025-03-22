#include "allocator.h"

#include <stdexcept>

Allocator::Allocator(Ref<State> state, std::size_t memory_map_size):
    state(state),
    memory_map_size(memory_map_size),
    local_top(state->top),
    block_descriptor_list(state->free_list),
    unused_block_descriptor_list(state->free_descriptors),
    last_free_block({0, 0}) {}

Object Allocator::malloc_internal(const std::size_t bytes) {
    Object obj;

    // Try using the last free block.
    if (last_free_block.obj != 0 &&
        last_free_block.size >= bytes) {

        obj = last_free_block.obj;
        last_free_block.obj += bytes;
        last_free_block.size -= bytes;

        if (last_free_block.size == 0)
            last_free_block.obj = 0;

        return obj;
    }

    // Try finding a fitting block in the block descriptor list.
    obj = delete_block_descriptor(bytes);
    if (obj != 0)
        return obj;

    // The above failed. Get memory from the top.
    std::size_t free_size = memory_map_size - local_top;
    // TODO: Temp until we can resize the memory map
    if (bytes > free_size)
        throw std::out_of_range("Block size too large");

    obj = local_top;
    local_top += bytes;
    return obj;
}

void Allocator::free_internal(Object obj, std::size_t bytes) {
}

Ref<Allocator::BlockDescriptor>
Allocator::new_block_descriptor(Object obj, std::size_t block_size) {
    Ref<BlockDescriptor> block_descriptor = 0;

    if (unused_block_descriptor_list != 0) {
        // Reuse the first unused block descriptor
        block_descriptor = unused_block_descriptor_list;
        unused_block_descriptor_list = block_descriptor->next;
    }
    else {
        // Allocate a new block descriptor
        const size_t free_size = memory_map_size - local_top;
        // TODO: Temp until we can resize the memory map
        if (block_size > free_size)
            throw std::out_of_range("Block size too large");

        block_descriptor = local_top;
        local_top += sizeof(BlockDescriptor);
    }

    // TODO: Temp tag for identifying block descriptors in the memory map. Remove later.
    block_descriptor->bd_tag = 0xBD;

    block_descriptor->block.obj = obj;
    block_descriptor->block.size = block_size;
    block_descriptor->next = 0;

    return block_descriptor;
}

void Allocator::insert_block_descriptor(Object obj, std::size_t block_size) {
    Ref<BlockDescriptor> new_list =
        new_block_descriptor(obj, block_size);

    if (block_descriptor_list != 0)
        new_list->next = block_descriptor_list;

    block_descriptor_list = new_list;
}

Object Allocator::delete_block_descriptor(std::size_t block_size) {
    // If block_descriptor_list is empty, there is nothing to delete.
    if (block_descriptor_list == 0)
        return 0;

    Ref<BlockDescriptor> best_fit_prev = 0;
    Ref<BlockDescriptor> best_fit = 0;

    Ref<BlockDescriptor> prev_descriptor = 0;
    Ref<BlockDescriptor> current_descriptor = block_descriptor_list;

    // Find the best fitting block.
    while (current_descriptor->next != 0) {
        const std::size_t descr_block_size = current_descriptor->block.size;

        if (block_size >= descr_block_size) {
            best_fit_prev = prev_descriptor;
            best_fit = current_descriptor;

            // Perfect fit. No need to search further.
            if (block_size == descr_block_size) break;
        }

        prev_descriptor = current_descriptor;
        current_descriptor = current_descriptor->next;
    }

    // Found no fit at all.
    if (best_fit == 0)
        return 0;

    Object offset = best_fit->block.obj;
    best_fit->block.obj += block_size;
    best_fit->block.size -= block_size;

    // best_fit block descriptor is now unused. Remove it from the
    // block descriptor list and add it to the unused block descriptor list.
    if (best_fit_prev != 0)
        best_fit_prev->next = best_fit->next;

    best_fit->next = unused_block_descriptor_list;
    unused_block_descriptor_list = best_fit;

    // If the block is that best_fit pointed to is now empty it becomes the
    // new last freed block
    if (best_fit->block.size != 0) {
        insert_block_descriptor(last_free_block.obj, last_free_block.size);
        last_free_block.obj = best_fit->block.obj;
        last_free_block.size = best_fit->block.size;
    }

    return offset;
}