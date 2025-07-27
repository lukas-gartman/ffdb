#include "FFDB2.h"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/mman.h>
#include <cmath>

/** \brief Align `bytes` depending on amount.
 *
 *  This function aligns the given `bytes` to \ref SMALL_ALIGNMENT or \ref LARGE_ALIGNMENT depending on whether `bytes`
 *  is larger than or equal to \ref ALIGNMENT_THRESHOLD.
 *
 *  @param bytes the bytes to align
 *  @return the aligned bytes
 */
static Size align_bytes(const Size bytes) {
    const Size alignment =
        bytes >= ALIGNMENT_THRESHOLD ? LARGE_ALIGNMENT : SMALL_ALIGNMENT;

    return (bytes + alignment - 1) & ~(alignment - 1);
}

/** \brief Calculate the number of bytes left logical page that has `offset`.
 *
 *  This function calculates the number of bytes left in the logical page that contains the given `offset`. It does so
 *  rather efficiently so it can be called often without performance concerns.
 *
 *  @param offset the offset from which to calculate the bytes left in the logical page
 *  @return the bytes left in the logical page
 */
static Size bytes_left_in_page(const Offset offset) {
    return LOGICAL_PAGE_SIZE - (offset & (LOGICAL_PAGE_SIZE - 1));
}

// TODO: Should be SYSTEM_PAGE_SIZE?
static Size calc_mem_map_size(const Size init_size) {
    Size new_size =
            ((init_size + LOGICAL_PAGE_SIZE - 1) / LOGICAL_PAGE_SIZE) * LOGICAL_PAGE_SIZE;

    if (new_size < MIN_DB_SIZE)
        new_size = MIN_DB_SIZE;

    return new_size;
}

/** \brief Find the next power of 2 that fits the sum of both arguments.
 *
 *  This function calculates the next size that is a power of 2 and fits the sum of `min_size` and `curr_size`. This is
 *  done by solving the equation `2^n >= min_size + curr_size`, where `n` is the smallest integer such that the equation
 *  holds, and then returning `2^n`.
 *
 *  @param curr_size the current size of the memory map
 *  @param min_size the minimum additional size we need
 *  @return `2^n` where `n` is the smallest integer such that `2^n >= min_size + curr_size`.
 */
static Size next_size(const Size curr_size, const Size min_size) {
    const Size n = ceil(log2((double) (min_size + curr_size)));
    return 1 << n;
}

Offset FFDB::malloc_internal(Size bytes) {
    if (db_mode != DB_WRITE_TXN)
        throw std::runtime_error("No transaction started");

    Offset obj;

    // Check if there is a free block cached that can be used.
    if (writer_state.lf_block.obj != 0 &&
        writer_state.lf_block.size >= bytes &&
        writer_state.lf_block.txn_id < shared_state->min_txn_id) {

        obj = writer_state.lf_block.obj;
        writer_state.lf_block.obj += bytes;
        writer_state.lf_block.size -= bytes;

        if (writer_state.lf_block.size == 0) {
            writer_state.lf_block.obj = 0;
            writer_state.lf_block.txn_id = 0;
        }

        return obj;
    }

    // Try finding a fitting block in the block descriptor list.
    obj = delete_block_descr(bytes);
    if (obj != 0) return obj;

    // The above failed. Get memory from the top.
    if (bytes > mmap_size - writer_state.top)
        resize_map(bytes);

    obj = writer_state.top;
    writer_state.top += bytes;

    return obj;
}

void FFDB::free_internal(Offset obj, Size bytes) {
    if (db_mode != DB_WRITE_TXN)
        throw std::runtime_error("No transaction started");

    if (obj == 0) return;

    Size txn_id = (obj >= shared_state->top) ? 0 : shared_state->curr_txn_id;

    if (txn_id == 0 && obj + bytes == writer_state.top) {
        writer_state.top -= bytes;

        if (writer_state.lf_block.obj != 0 &&
            writer_state.lf_block.txn_id == 0 &&
            writer_state.lf_block.obj + writer_state.lf_block.size == writer_state.top) {

            writer_state.top -= writer_state.lf_block.size;
            writer_state.lf_block = {0, 0, 0};
        }
        return;
    }

    if (writer_state.lf_block.obj != 0) {
        // Merge with lf_block is adjacent.
        const bool lf_block_is_before =
                writer_state.lf_block.obj + writer_state.lf_block.size == obj;
        const bool lf_block_is_after =
                obj + bytes == writer_state.lf_block.obj;

        if ((lf_block_is_before || lf_block_is_after) &&
            writer_state.lf_block.txn_id == txn_id) {

            writer_state.lf_block.size += bytes;
            if (lf_block_is_after)
                writer_state.lf_block.obj = obj;

            return;
        }

        // Otherwise, it will get replaced.
        insert_block_descr(writer_state.lf_block);
    }

    // 0 as an id here means that the last free block is located beyond
    // the top of the memory map. This is the case when the last free block
    // couldn't be merged with the transaction local top. This signifies
    // that the block can be immediately reused if it would be inserted
    // into the blockdescriptor list.
    writer_state.lf_block = {obj, bytes, txn_id};
}

Ref<BlockDescr> FFDB::new_block_descr(Block block) {
    Ref<BlockDescr> descr = 0;

    if (writer_state.unused_descrs != 0) {
        descr = writer_state.unused_descrs;
        writer_state.unused_descrs =
                writer_state.unused_descrs->next_unused;
    }
    else {
        const Size bytes = align_bytes(sizeof(BlockDescr));

        if (bytes > shared_state->mmap_size - writer_state.top)
            resize_map(bytes);

        descr = writer_state.top;
        writer_state.top += bytes;
    }

    *descr = {block, 0, 0, shared_state->curr_txn_id};

    return descr;
}

Ref<BlockDescr> FFDB::upd_block_descr(Ref<BlockDescr> descr) {
    // If the descr is not from a previous transaction, there is no need
    // to do copy-on-write.
    if (descr->txn_id == shared_state->curr_txn_id) return descr;

    // Add the old descriptor to the pending unused list. In case of
    // a rollback, this descriptor will still have its old position in
    // the free list from before the start of the current_transaction.
    descr->next_unused = writer_state.pending_unused_descrs;
    writer_state.pending_unused_descrs = descr;
    if (writer_state.pending_unused_last == 0)
        writer_state.pending_unused_last = descr;

    Ref<BlockDescr> new_descr = new_block_descr(descr->block);
    new_descr->next_free = descr->next_free;
    new_descr->txn_id = shared_state->curr_txn_id;

    return new_descr;
}

void FFDB::insert_block_descr(Block block) {
    const Ref<BlockDescr> descr = new_block_descr(block);

    Ref<BlockDescr> prev_descr = 0;
    Ref<BlockDescr> curr_descr = writer_state.free_blocks;

    // Block descr is sorted by size first and then transaction id.
    while (curr_descr != 0) {
        const bool found =
            (descr->block.size < curr_descr->block.size) ||
            (descr->block.size == curr_descr->block.size &&
            descr->block.txn_id <= curr_descr->block.txn_id);

        // If we found a position to place our block descr in,
        // we will not need to CoW the current_descr.
        if (found) {
            descr->next_free = curr_descr;

            if (prev_descr != 0)
                prev_descr->next_free = descr;
            else
                writer_state.free_blocks = descr; // Insert at the head

            return;
        }

        // CoW this bad boy
        curr_descr = upd_block_descr(curr_descr);

        if (prev_descr == 0)
            writer_state.free_blocks = curr_descr;
        else
            prev_descr->next_free = curr_descr;

        prev_descr = curr_descr;
        curr_descr = curr_descr->next_free;
    }

    descr->next_free = 0;
    if (prev_descr != 0)
        prev_descr->next_free = descr;
    else
        writer_state.free_blocks = descr; // Insert as the only element
}

Offset FFDB::delete_block_descr(const Size block_size) {
    if (writer_state.free_blocks == 0)
        return 0;

    Ref<BlockDescr> curr_descr = writer_state.free_blocks;

    // Find the best fitting block.
    while (curr_descr != 0) {
        const bool found =
            curr_descr->block.size >= block_size &&
            curr_descr->block.txn_id < shared_state->min_txn_id;

        if (found) break;

        curr_descr = curr_descr->next_free;
    }

    const Ref<BlockDescr> best_fit = curr_descr;

    // Found no fit at all.
    if (best_fit == 0) return 0;

    // If a fitting block was found, we need to CoW the path leading to it.
    Ref<BlockDescr> prev_descr = 0;
    curr_descr = writer_state.free_blocks;

    while (curr_descr != best_fit) {
        curr_descr = upd_block_descr(curr_descr);

        if (prev_descr == 0)
            writer_state.free_blocks = curr_descr;
        else
            prev_descr->next_free = curr_descr;

        prev_descr = curr_descr;
        curr_descr = curr_descr->next_free;
    }

    // Remove the block descriptor from the free list.
    if (prev_descr != 0) prev_descr->next_free = curr_descr->next_free;
    else writer_state.free_blocks = curr_descr->next_free;

    // If the block was allocated this transaction, it is added to the
    // unused list. Otherwise it is added to the pending unused list.
    if (curr_descr->txn_id == shared_state->curr_txn_id) {
        curr_descr->next_unused = writer_state.unused_descrs;
        writer_state.unused_descrs = curr_descr;
    }
    else {
        curr_descr->next_unused = writer_state.pending_unused_descrs;
        writer_state.pending_unused_descrs = curr_descr;

        if (writer_state.pending_unused_last == 0)
            writer_state.pending_unused_last = curr_descr;
    }

    // Block offset to return
    Offset offset = curr_descr->block.obj;

    auto found_block = curr_descr->block;

    found_block.obj += block_size;
    found_block.size -= block_size;

    // If the block that best_fit pointed to is not empty it becomes the
    // new last freed block
    if (found_block.size != 0) {
        if (writer_state.lf_block.obj != 0)
            insert_block_descr(writer_state.lf_block);
        writer_state.lf_block = found_block;
    }

    return offset;
}

// TODO: Handle DB_NO_TXN
void FFDB::lock() {
    if (mmap_size != shared_state->mmap_size) resize_map(0);

    if (db_mode == DB_WRITE_TXN) shared_state->write_lock.lock();
    else if (db_mode == DB_READ_TXN) shared_state->resize_rw_lock.read_lock();
    else throw std::invalid_argument("Invalid DBMode");
}

void FFDB::unlock() {
    if (db_mode == DB_WRITE_TXN) shared_state->write_lock.unlock();
    else if (db_mode == DB_READ_TXN) shared_state->resize_rw_lock.unlock();
    else throw std::invalid_argument("Invalid DBMode");
}

// TODO: Throws no exceptions but close and munmap may fail. Correct use of noexcept?
void FFDB::cleanup() noexcept {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    if (shared_state != nullptr) {
        munmap(shared_state, mmap_size);
        shared_state = nullptr;
    }
}

void FFDB::resize_map(Size min_size) {
    shared_state->resize_rw_lock.write_lock();

    // It may happen that another process already resized the map, in
    // that case we only need to remap the memory and update size.
    const size_t new_size =
        shared_state->mmap_size != mmap_size ? shared_state->mmap_size : next_size(mmap_size, min_size);

    // Should not need to check fd >= 0 as we don't support anon mappings.
    if (shared_state->mmap_size != new_size) {
        if (ftruncate(fd, SYSTEM_PAGE_SIZE + new_size) == -1) {
            shared_state->resize_rw_lock.unlock();
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }
    }

    // In case mremap fails, we have still made the file larger and
    // need to signal this to other processes.
    shared_state->mmap_size = new_size;

    // Remap everything except the first page
    auto *new_base = (unsigned char *) mremap(
            base,
            mmap_size,
            new_size,
            MREMAP_MAYMOVE
    );

    if (new_base == MAP_FAILED) {
        shared_state->resize_rw_lock.unlock();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    mmap_size = new_size;
    base = new_base;
    current_base = new_base;

    shared_state->resize_rw_lock.unlock();
}

FFDB::FFDB(std::string filepath, int flags, int mode, Size init_size):
pid(getpid()),
fd(-1),
mmap_size(0),
base(nullptr),
filepath(std::move(filepath)) {
    if (this->filepath.empty()) throw std::invalid_argument("filepath is empty");

    bool is_new = false;

    fd = open(this->filepath.c_str(), flags, mode);
    if (fd == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    mmap_size = lseek(fd, 0, SEEK_END);
    if (mmap_size == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // Initialize file if it was just created.
    if (mmap_size == 0) {
        mmap_size = calc_mem_map_size(init_size);
        if (ftruncate(fd, mmap_size) == -1) {
            cleanup();
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }
        is_new = true;
    }

    shared_state = (SharedState *)
        mmap(nullptr, mmap_size, PROT_READ, MAP_SHARED, fd, 0);

    if (shared_state == MAP_FAILED) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    if (mprotect(shared_state, SYSTEM_PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    mmap_size -= SYSTEM_PAGE_SIZE;
    base = ((unsigned char *) shared_state) + SYSTEM_PAGE_SIZE;

    if (is_new) {
        shared_state->mmap_size = mmap_size;

        shared_state->curr_txn_id = 1;
        shared_state->min_txn_id = 1;

        shared_state->write_lock.init();
        shared_state->rev_lock.init();
        shared_state->resize_rw_lock.init();

        shared_state->top = SMALL_ALIGNMENT; // 0 is null

        // The rest is zeroed by ftruncate
    }
}

Revision *FFDB::register_revision(Offset root, Id txn_id) {
    shared_state->rev_lock.lock();

    Revision *found_rev = nullptr;
    bool found = false;

    shared_state->min_txn_id = SIZE_MAX;
    for (size_t i = 0; i < shared_state->revision_count; i++) {
        Revision *curr_rev = &shared_state->revisions[i];

        if (curr_rev->ref_count == 0) {
            if (found_rev == nullptr) found_rev = curr_rev;
        }
        else {
            if (curr_rev->pid == pid &&
                curr_rev->root == root) {

                curr_rev->ref_count++;
                curr_rev->txn_id = txn_id;
                found_rev = curr_rev;
                found = true;
            }

            if (shared_state->min_txn_id > curr_rev->txn_id)
                shared_state->min_txn_id = curr_rev->txn_id;
        }
    }

    if (!found) {
        if (found_rev == nullptr) {
            size_t max_revisions = (LOGICAL_PAGE_SIZE - sizeof(SharedState)) / sizeof(Revision);
            if (shared_state->revision_count >= max_revisions) {
                shared_state->rev_lock.unlock();
                throw std::runtime_error("No more revisions available");
            }
            found_rev = &shared_state->revisions[shared_state->revision_count++];
        }

        found_rev->pid = pid;
        found_rev->root = root;
        found_rev->ref_count = 1;
        found_rev->txn_id = txn_id;

        if (shared_state->min_txn_id > txn_id)
            shared_state->min_txn_id = txn_id;
    }

    shared_state->rev_lock.unlock();

    return found_rev;
}

void FFDB::unregister_revision(Revision *rev) {
    shared_state->rev_lock.lock();

    rev->ref_count--;
    if (rev->ref_count == 0) {
        while (shared_state->revisions[shared_state->revision_count - 1].ref_count == 0) {
            shared_state->revision_count--;
            if (shared_state->revision_count == 0) break;
        }
    }

    shared_state->min_txn_id = SIZE_MAX;
    for (size_t i = 0; i < shared_state->revision_count; i++) {
        Revision *curr_rev = &shared_state->revisions[i];

        if (curr_rev->ref_count > 0 && shared_state->min_txn_id > curr_rev->txn_id)
            shared_state->min_txn_id = curr_rev->txn_id;
    }

    shared_state->rev_lock.unlock();
}

// TDOD: Use these three functions to connect all code.

void FFDB::start_txn(DBMode mode) {
    db_mode = mode;
    lock();

    current_base = base;

    if (db_mode == DB_WRITE_TXN) {
        if (mprotect(base, mmap_size, PROT_READ | PROT_WRITE) == -1)
            throw std::runtime_error("Errno: " + std::to_string(errno));

        writer_state.top = shared_state->top;
        writer_state.free_blocks = shared_state->free_blocks;
        writer_state.unused_descrs = shared_state->free_descrs;
        writer_state.pending_unused_descrs = 0;
        writer_state.pending_unused_last = 0;
        writer_state.lf_block.obj = 0;
        writer_state.lf_block.size = 0;
        writer_state.lf_block.txn_id = 0;
    }
    else if (db_mode == DB_READ_TXN) {
        // Reg revision?
    }
}

void FFDB::rollback_txn() {
    if (mprotect(base, mmap_size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (db_mode == DB_READ_TXN) {
        // unreg revision?

        unlock();
        db_mode = DB_NO_TXN;
        return;
    }

    // shared_state->writer_pid = 0;

    unlock();
    db_mode = DB_NO_TXN;
}

void FFDB::commit_txn()  {
    if (db_mode == DB_NO_TXN)
        throw std::runtime_error("No transaction started!");

    if (db_mode == DB_READ_TXN) {
        // unreg revision?

        unlock();
        db_mode = DB_NO_TXN;
        return;
    }

    auto saved_top = shared_state->top;
    auto saved_free_list = shared_state->free_blocks;
    auto saved_free_descriptors = shared_state->free_descrs;

    if (writer_state.pending_unused_last != 0) {
        writer_state.pending_unused_last->next_unused = writer_state.unused_descrs;
        writer_state.unused_descrs = writer_state.pending_unused_descrs;
    }

    if (msync(base, mmap_size, MS_SYNC | MS_INVALIDATE) != 0)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    shared_state->top = writer_state.top;
    shared_state->free_blocks = writer_state.free_blocks;
    shared_state->free_descrs = writer_state.unused_descrs;
    shared_state->curr_txn_id++;

    if (mprotect(base, mmap_size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (msync(base, mmap_size, MS_SYNC | MS_INVALIDATE) != 0) {
        shared_state->top = saved_top;
        shared_state->free_blocks = saved_free_list;
        shared_state->free_descrs = saved_free_descriptors;
        shared_state->curr_txn_id--;
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // state->writer_pid = 0;

    unlock();
    db_mode = DB_NO_TXN;
}