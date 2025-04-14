#include "allocator.h"

#include <cassert>
#include <stdexcept>

Allocator::Allocator(State* state, std::size_t memory_map_size):
    state(state),
    memory_map_size(memory_map_size),
    local_top(state->top),
    block_descriptor_list(state->free_list),
    unused_block_descriptor_list(state->free_descriptors),
    last_free_block({0, 0}) {}

Object Allocator::malloc_internal(std::size_t bytes) {
    Object obj;

    // Check if there is a free block cached that can be used.
    if (last_free_block.obj != 0 &&
        last_free_block.size >= bytes) {

        // Allocate exactly the needed amount of data in the last free block.
        obj = last_free_block.obj;
        last_free_block.obj += bytes;
        last_free_block.size -= bytes;

        // If the last free block is empty, set it to 0, meaning that no free
        // block is cached anymore.
        if (last_free_block.size == 0) last_free_block.obj = 0;

        return obj;
    }

    // Check if there if a block descriptor points to a free block that can be
    // used.
    obj = delete_block_descriptor(bytes);
    if (obj != 0) return obj;

    // If the above failed, allocate memory on the top of the memory map.
    std::size_t free_size = memory_map_size - local_top;

    if (bytes > free_size)
        throw std::out_of_range("Block size too large");

    obj = local_top;
    local_top += bytes;

    return obj;
}

void Allocator::free_internal(Object obj, std::size_t bytes) {
    // Can't free the null object.
    if (obj == 0) return;

    // Check if the object we want to free is at the end of the memory map.
    if (obj + bytes == local_top) {
        // If it is we just decrease the top. This will not be possible across
        // transactions!
        local_top -= bytes;

        // Now if the last free block is now at the end of the memory map, we
        // can decrease the local top even more.
        if (last_free_block.obj != 0 &&
            last_free_block.obj + last_free_block.size == local_top) {

            local_top -= last_free_block.size;
            last_free_block.obj = 0;
            last_free_block.size = 0;
        }

        return;
    }

    // Now check if we have a cached block.
    if (last_free_block.obj != 0) {
        // Check is the last free block is located before the block we want to
        // free.
        const bool lsb_is_before =
            last_free_block.obj + last_free_block.size == obj;

        // Check is the last free block is located after the block we want to
        // free.
        const bool lsb_is_after = obj + bytes == last_free_block.obj;

        // In both cases we can merge the two blocks by increasing the size of
        // the last free block.
        if (lsb_is_before || lsb_is_after) {
            last_free_block.size += bytes;

            // If last free block is located after the block we want to free,
            // the merged last free block must now be at the same offset as
            // the block we want to free.
            if (lsb_is_after) last_free_block.obj = obj;

            return;
        }

        // If last free block was not adjacent to the block we want to free,
        // we need to insert the last free block into the list of free blocks,
        // as the new last free flock will be the block we are trying to free.
        insert_block_descriptor(last_free_block.obj, last_free_block.size);
    }

    // At this point, our block was not located at the end of the memory map and
    // either the last free block was not adjacent or it did not exist at all.
    // Either way we last free block can be overwritten.
    last_free_block.obj = obj;
    last_free_block.size = bytes;
}

Ref<Allocator::BlockDescriptor>
Allocator::new_block_descriptor(Object obj, std::size_t block_size) {
    Ref<BlockDescriptor> block_descriptor = 0;

    // Check if we have a block descriptor that can be reused.
    if (unused_block_descriptor_list != 0) {
        block_descriptor = unused_block_descriptor_list;
        unused_block_descriptor_list =
            unused_block_descriptor_list->next;
    }
    // If not we allocate a new block descriptor
    else {
        const size_t free_size = memory_map_size - local_top;
        // TODO: Temp until we can resize the memory map
        if (block_size > free_size)
            throw std::out_of_range("Block size too large");

        block_descriptor = local_top;
        local_top += sizeof(BlockDescriptor);
    }

    block_descriptor->block.obj = obj;
    block_descriptor->block.size = block_size;
    block_descriptor->next = 0;

    return block_descriptor;
}

void Allocator::insert_block_descriptor(Object obj, std::size_t block_size) {
    // Get a block descriptor and simply insert it to the list of free blocks.
    Ref<BlockDescriptor> new_list =
        new_block_descriptor(obj, block_size);

    new_list->next = block_descriptor_list;
    block_descriptor_list = new_list;
}

Object Allocator::delete_block_descriptor(std::size_t block_size) {
    // If block_descriptor_list is empty, there is nothing to delete.
    if (block_descriptor_list == 0)
        return 0;

    // Used to remember what block fits the requirements best.
    Ref<BlockDescriptor> best_fit_prev = 0;
    Ref<BlockDescriptor> best_fit = 0;

    // Used to iterate through the block descriptor list.
    Ref<BlockDescriptor> prev_descriptor = 0;
    Ref<BlockDescriptor> current_descriptor = block_descriptor_list;

    // Iterate through the block descriptor list.
    while (current_descriptor != 0) {
        const std::size_t descr_block_size = current_descriptor->block.size;

        // If the block descriptor points to a block that contains enough space
        // and this space is smaller than the best fit we have found so far,
        // we can update best fit.
        if (block_size <= descr_block_size &&
            (best_fit == 0 || descr_block_size < best_fit->block.size)) {

            best_fit_prev = prev_descriptor;
            best_fit = current_descriptor;

            // Perfect fit. No need to search further.
            if (best_fit->block.size == block_size) break;
        }

        prev_descriptor = current_descriptor;
        current_descriptor = current_descriptor->next;
    }

    // Iterated through every block descriptor and found every block too tight.
    if (best_fit == 0) return 0;

    // Update the block found.
    Object offset = best_fit->block.obj;

    best_fit->block.obj += block_size;
    best_fit->block.size -= block_size;

    // best_fit block descriptor is now unused. Remove it from the
    // block descriptor list and add it to the unused block descriptor list.
    if (best_fit_prev != 0)
        best_fit_prev->next = best_fit->next;
    else
        block_descriptor_list = best_fit->next;

    best_fit->next = unused_block_descriptor_list;
    unused_block_descriptor_list = best_fit;

    // If the block did not fit perfectly, the remaining space is cached.
    Block block_left = best_fit->block;

    if (block_left.size != 0) {
        // Send the last cached block to the list of free blocks.
        if (last_free_block.obj != 0) {
            insert_block_descriptor(last_free_block.obj, last_free_block.size);
        }
        last_free_block.obj = block_left.obj;
        last_free_block.size = block_left.size;
    }

    return offset;
}