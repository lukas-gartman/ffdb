#include "MemoryMapping.h"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/mman.h>

MemoryMapping::MemoryMapping(
    std::string filepath,
    int flags, int mode,
    size_t init_size):
fd(-1),
size(0),
base(nullptr),
filepath(std::move(filepath)),
pid(getpid()),
shared_state(nullptr) {
    if (this->filepath.empty()) throw std::invalid_argument("filepath is empty");

    bool is_new = false;

    fd = open(this->filepath.c_str(), flags, mode);
    if (fd == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // Initialize file if it was just created.
    if (size == 0) {
        size = calc_mem_map_size(init_size);
        if (ftruncate(fd, size) == -1) {
            cleanup();
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }
        is_new = true;
    }

    shared_state = (MemState *)
        mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);

    if (shared_state == MAP_FAILED) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    if (mprotect(shared_state, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    size -= PAGE_SIZE;
    base = ((unsigned char *) shared_state) + PAGE_SIZE;

    if (is_new) {
        shared_state->mmap_size = size;

        shared_state->curr_txn_id = 1;
        shared_state->min_txn_id = 1;

        shared_state->write_lock.init();
        shared_state->rev_lock.init();
        shared_state->resize_rw_lock.init();

        shared_state->top = 1; // 0 is null

        // The rest is zeroed by ftruncate
    }
}

MemoryMapping::~MemoryMapping() {
    cleanup();
}

Revision *MemoryMapping::register_revision(Object root, size_t txn_id) {
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
            size_t max_revisions = (PAGE_SIZE - sizeof(MemState)) / sizeof(Revision);
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

void MemoryMapping::unregister_revision(Revision *rev) {
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

void MemoryMapping::init_wr_txn(WriterState *ws) {
    if (ws == nullptr)
        throw std::runtime_error("No transaction started!");

    shared_state->write_lock.lock();

    if (mprotect(base, size, PROT_READ | PROT_WRITE) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    current_base = base;

    ws->min_txn_id = shared_state->min_txn_id;
    ws->id = shared_state->curr_txn_id;
    ws->mmap_size = shared_state->mmap_size;
    ws->old_top = shared_state->top;
    ws->top = shared_state->top;
    ws->free_blocks = shared_state->free_blocks;
    ws->unused_descrs = shared_state->free_descrs;
    ws->pending_unused_descrs = 0;
    ws->pending_unused_last = 0;
    ws->lf_block.obj = 0;
    ws->lf_block.size = 0;
    ws->lf_block.txn_id = 0;
}

void MemoryMapping::cancel_wr_txn() {
    shared_state->write_lock.unlock();
}

void MemoryMapping::apply_wr_txn(WriterState *ws) {
    if (ws == nullptr)
        throw std::runtime_error("No transaction started!");

    auto saved_top = shared_state->top;
    auto saved_free_list = shared_state->free_blocks;
    auto saved_free_descriptors = shared_state->free_descrs;

    if (ws->pending_unused_last != 0) {
        ws->pending_unused_last->next_unused = ws->unused_descrs;
        ws->unused_descrs = ws->pending_unused_descrs;
    }

    if (msync(base, size, MS_SYNC | MS_INVALIDATE) != 0)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    shared_state->top = ws->top;
    shared_state->free_blocks = ws->free_blocks;
    shared_state->free_descrs = ws->unused_descrs;
    shared_state->curr_txn_id++;

    if (mprotect(base, size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (msync(base, size, MS_SYNC | MS_INVALIDATE) != 0) {
        shared_state->top = saved_top;
        shared_state->free_blocks = saved_free_list;
        shared_state->free_descrs = saved_free_descriptors;
        shared_state->curr_txn_id--;
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // state->writer_pid = 0;

    shared_state->write_lock.unlock();
}

// TODO need a function that releases writer lock in case of rollback

void MemoryMapping::cleanup() noexcept {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    if (shared_state != nullptr) {
        munmap(shared_state, size);
        shared_state = nullptr;
    }
}

size_t MemoryMapping::calc_mem_map_size(size_t init_size) {
    size_t new_size =
            ((init_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    if (new_size < MIN_PAGE_SIZE)
        new_size = MIN_PAGE_SIZE;

    return new_size;
}

void MemoryMapping::resize_map() {
    shared_state->resize_rw_lock.write_lock();

    // It may happen that another process already resized the map, in
    // that case we only need to remap the memory and update size.
    const size_t new_size =
        shared_state->mmap_size != size ? shared_state->mmap_size : size * 2;

    // Should not need to check fd >= 0 as we don't support anon mappings.
    if (shared_state->mmap_size != new_size) {
        if (ftruncate(fd, PAGE_SIZE + new_size) == -1) {
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
            size,
            new_size,
            MREMAP_MAYMOVE
    );

    if (new_base == MAP_FAILED) {
        shared_state->resize_rw_lock.unlock();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    size = new_size;
    base = new_base;
    current_base = new_base;

    shared_state->resize_rw_lock.unlock();
}