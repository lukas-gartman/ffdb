#include "Allocator.h"

#include <cassert>
#include <stdexcept>

Allocator::Allocator(MemoryMapping *mmap):
        mmap(mmap), curr_txn(nullptr) {}

Object Allocator::malloc_internal(std::size_t bytes) {

    if (curr_txn == nullptr)
        throw std::runtime_error("No transaction started");

    Object obj;

    // Check if there is a free block cached that can be used.
    if (curr_txn->lf_block.obj != 0 &&
        curr_txn->lf_block.size >= bytes &&
        curr_txn->lf_block.txn_id < mmap->state->min_txn_id) {

        obj = curr_txn->lf_block.obj;
        curr_txn->lf_block.obj += bytes;
        curr_txn->lf_block.size -= bytes;

        if (curr_txn->lf_block.size == 0) {
            curr_txn->lf_block.obj = 0;
            curr_txn->lf_block.txn_id = 0;
        }

        return obj;
    }

    // Try finding a fitting block in the block descriptor list.
    obj = delete_block_descr(bytes);
    if (obj != 0) return obj;

    // The above failed. Get memory from the top.
    std::size_t free_size = mmap->get_size() - curr_txn->top;
    // TODO: Temp until we can resize the memory map
    if (bytes > free_size)
        throw std::out_of_range("Block size too large");

    obj = curr_txn->top;
    curr_txn->top += bytes;

    return obj;
}

void Allocator::free_internal(Object obj, std::size_t bytes) {
    if (curr_txn == nullptr)
        throw std::runtime_error("No transaction started");

    if (obj == 0) return;

    size_t txn_id = (obj >= mmap->state->top) ? 0 : curr_txn->id;

    if (txn_id == 0 && obj + bytes == curr_txn->top) {
        curr_txn->top -= bytes;

        // TODO: Maybe instead of checking if lf_block.txn_id == 0, check if lf_block.obj >= mmap->state->top?
        if (curr_txn->lf_block.obj != 0 &&
            curr_txn->lf_block.txn_id == 0 &&
            curr_txn->lf_block.obj + curr_txn->lf_block.size == curr_txn->top) {

            curr_txn->top -= curr_txn->lf_block.size;
            curr_txn->lf_block.obj = 0;
            curr_txn->lf_block.size = 0;
            curr_txn->lf_block.txn_id = 0;
        }
        return;
    }

    if (curr_txn->lf_block.obj != 0) {
        // Merge with lf_block is adjacent.
        const bool lf_block_is_before =
                curr_txn->lf_block.obj + curr_txn->lf_block.size == obj;
        const bool lf_block_is_after =
                obj + bytes == curr_txn->lf_block.obj;

        if ((lf_block_is_before || lf_block_is_after) &&
            curr_txn->lf_block.txn_id == txn_id) {

            curr_txn->lf_block.size += bytes;
            if (lf_block_is_after)
                curr_txn->lf_block.obj = obj;

            return;
        }

        // Otherwise, it will get replaced.
        insert_block_descr(curr_txn->lf_block);
    }

    curr_txn->lf_block.obj = obj;
    curr_txn->lf_block.size = bytes;

    // 0 as an id here means that the last free block is located beyond
    // the top of the memory map. This is the case when the last free block
    // couldn't be merged with the transaction local top. This signifies
    // that the block can be immediately reused if it would be inserted
    // into the blockdescriptor list.
    // TODO: If it can be immediately reused, why even insert it into the
    // TODO: block descriptor tree. Finish the database assuming this is
    // TODO: needed, create tests, change this and see if the tests still
    // TODO: pass. THis code is confusing.
    curr_txn->lf_block.txn_id = txn_id;
}

void Allocator::start_transaction() {
    if (curr_txn != nullptr)
        throw std::runtime_error("A transaction is already active");

    auto *txn = new Transaction();
    mmap->init_transaction(txn);

    curr_txn = txn;
}

void Allocator::commit_transaction() {
    if (curr_txn == nullptr)
        throw std::runtime_error("No transaction to end");

    if (curr_txn->lf_block.obj != 0) {
        insert_block_descr(curr_txn->lf_block);
        curr_txn->lf_block = {0, 0, 0};
    }

    mmap->commit_transaction(curr_txn);

    delete curr_txn;
    curr_txn = nullptr;
}

size_t Allocator::get_txn_id() const {
    if (curr_txn == nullptr)
        throw std::runtime_error("No transaction started");

    return curr_txn->id;
}

Ref<BlockDescr>
Allocator::new_block_descr(Block block) {
    Ref<BlockDescr> descr = 0;

    if (curr_txn->unused_descrs != 0) {
        descr = curr_txn->unused_descrs;
        curr_txn->unused_descrs =
                curr_txn->unused_descrs->next_unused;
    }
    else {
        // Allocate a new block descriptor
        const size_t free_size = mmap->get_size() - curr_txn->top;
        // TODO: Temp until we can resize the memory map
        if (block.size > free_size)
            throw std::out_of_range("Block size too large");

        descr = curr_txn->top;
        curr_txn->top += sizeof(BlockDescr);
    }

    descr->block = block;
    descr->next_free = 0;
    descr->next_unused = 0;
    descr->txn_id = curr_txn->id;

    return descr;
}

Ref<BlockDescr> Allocator::upd_block_descr(Ref<BlockDescr> descr) {
    // If the descr is not from a previous transaction, there is no need
    // to do copy-on-write.
    if (descr->txn_id == curr_txn->id) return descr;

    // Add the old descriptor to the pending unused list. In case of
    // a rollback, this descriptor will still have its old position in
    // the free list from before the start of the current_transaction.
    descr->next_unused = curr_txn->pending_unused_descrs;
    if (curr_txn->pending_unused_last == 0)
        curr_txn->pending_unused_last = descr;

    Ref<BlockDescr> new_descr = new_block_descr(descr->block);

    return new_descr;
}

void Allocator::insert_block_descr(Block block) {
    Ref<BlockDescr> descr = new_block_descr(block);

    Ref<BlockDescr> prev_descr = 0;
    Ref<BlockDescr> curr_descr = curr_txn->free_blocks;

    // Block descr is sorted by size first and then transaction id.
    while (curr_descr != 0) {
        bool found =
            (descr->block.size < curr_descr->block.size) ||
            (descr->block.size == curr_descr->block.size &&
                descr->block.txn_id <= curr_descr->block.txn_id);

        // If we found a position to place our block descr in,
        // we will not need to CoW the current_descr.
        if (found) {
            descr->next_free = curr_descr;
            if (prev_descr != 0) prev_descr->next_free = descr;
            else curr_txn->free_blocks = descr; // Insert at the head

            return;
        }

        // CoW this bad boy
        curr_descr = upd_block_descr(curr_descr);
        if (prev_descr == 0) {
            curr_txn->free_blocks = curr_descr;
        } else {
            prev_descr->next_free = curr_descr;
        }

        prev_descr = curr_descr;
        curr_descr = curr_descr->next_free;
    }

    descr->next_free = 0;
    if (prev_descr != 0) {
        prev_descr->next_free = descr;
    } else {
        curr_txn->free_blocks = descr; // Insert as the only element
    }
}

Object Allocator::delete_block_descr(std::size_t block_size) {
    if (curr_txn->free_blocks == 0)
        return 0;

    Ref<BlockDescr> prev_descr = 0;
    Ref<BlockDescr> curr_descr = curr_txn->free_blocks;

    // Find the best fitting block.
    while (curr_descr != 0) {
        bool found =
            curr_descr->block.size >= block_size &&
            curr_descr->block.txn_id < mmap->state->min_txn_id;

        if (found) break;

        prev_descr = curr_descr;
        curr_descr = curr_descr->next_free;
    }

    Ref<BlockDescr> best_fit = curr_descr;

    // Found no fit at all.
    if (best_fit == 0) return 0;

    // If a fitting block was found, we need to CoW the path leading to it.
    prev_descr = 0;
    curr_descr = curr_txn->free_blocks;

    while (curr_descr != best_fit) {
        curr_descr = upd_block_descr(curr_descr);
        if (prev_descr == 0) {
            curr_txn->free_blocks = curr_descr;
        } else {
            prev_descr->next_free = curr_descr;
        }

        prev_descr = curr_descr;
        curr_descr = curr_descr->next_free;
    }

    // Remove the block descriptor from the free list.
    if (prev_descr != 0)
        prev_descr->next_free = curr_descr->next_free;
    else
        curr_txn->free_blocks = curr_descr->next_free;

    // If the block was allocated this transaction, it is added to the
    // unused list. Otherwise it is added to the pending unused list.
    if (curr_descr->txn_id == curr_txn->id) {
        curr_descr->next_unused = curr_txn->unused_descrs;
        curr_txn->unused_descrs = curr_descr;
    }
    else {
        curr_descr->next_unused = curr_txn->pending_unused_descrs;
        curr_txn->pending_unused_descrs = curr_descr;

        if (curr_txn->pending_unused_last == 0)
            curr_txn->pending_unused_last = curr_descr;
    }

    // Block offset to return
    Object offset = curr_descr->block.obj;

    curr_descr->block.obj += block_size;
    curr_descr->block.size -= block_size;

    Block block_left = curr_descr->block;

    // If the block that best_fit pointed to is not empty it becomes the
    // new last freed block
    if (block_left.size != 0) {
        if (curr_txn->lf_block.obj != 0) {
            insert_block_descr(curr_txn->lf_block);
        }
        curr_txn->lf_block.obj = block_left.obj;
        curr_txn->lf_block.size = block_left.size;
        curr_txn->lf_block.txn_id = block_left.txn_id;
    }

    return offset;
}

void Allocator::dump_block_descrs() {
    if (curr_txn == nullptr)
        throw std::runtime_error("No transaction started");

    std::cout << "---- Shared state:" << std::endl;
    std::cout << "-- Free blocks:" << std::endl;
    auto block_descr = mmap->state->free_blocks;
    while (block_descr != 0) {
        std::cout << "  Block: " << block_descr->block.obj
                  << ", Size: " << block_descr->block.size
                  << ", Txn ID: " << block_descr->block.txn_id
                  << std::endl;
        block_descr = block_descr->next_free;
    }
    std::cout << std::endl;

    std::cout << "-- Unused descrs:" << std::endl;
    block_descr = mmap->state->free_descrs;
    while (block_descr != 0) {
        std::cout << "  Block: " << block_descr->block.obj
                  << ", Size: " << block_descr->block.size
                  << ", Txn ID: " << block_descr->block.txn_id
                  << std::endl;
        block_descr = block_descr->next_unused;
    }
    std::cout << std::endl;

    std::cout << "---- Txn state:" << std::endl;
    std::cout << "-- Free blocks:" << std::endl;
    block_descr = curr_txn->free_blocks;
    while (block_descr != 0) {
        std::cout << "  Block: " << block_descr->block.obj
                  << ", Size: " << block_descr->block.size
                  << ", Txn ID: " << block_descr->block.txn_id
                  << std::endl;
        block_descr = block_descr->next_free;
    }
    std::cout << std::endl;

    std::cout << "-- Unused descrs:" << std::endl;
    block_descr = curr_txn->unused_descrs;
    while (block_descr != 0) {
        std::cout << "  Block: " << block_descr->block.obj
                  << ", Size: " << block_descr->block.size
                  << ", Txn ID: " << block_descr->block.txn_id
                  << std::endl;
        block_descr = block_descr->next_unused;
    }
    std::cout << std::endl;

    std::cout << "-- Pending unused descrs:" << std::endl;
    block_descr = curr_txn->pending_unused_descrs;
    while (block_descr != 0) {
        std::cout << "  Block: " << block_descr->block.obj
                  << ", Size: " << block_descr->block.size
                  << ", Txn ID: " << block_descr->block.txn_id
                  << std::endl;
        block_descr = block_descr->next_unused;
    }
}