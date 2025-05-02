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
state(nullptr) {
    if (this->filepath.empty()) throw std::invalid_argument("filepath is empty");

    bool is_new = false;

    fd = open(this->filepath.c_str(), flags, mode);
    if (fd == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    // Get map size
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

    state = (MemState *)
        mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);

    if (state == MAP_FAILED) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    if (mprotect(state, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    size -= PAGE_SIZE;
    base = ((unsigned char *) state) + PAGE_SIZE;

    if (is_new) {
        state->file_size = size;

        state->curr_txn_id = 1;
        state->min_txn_id = 1;

        state->write_lock.init();
        state->rev_lock.init();
        state->resize_rw_lock.init();

        state->top = 1; // 0 is null
        state->free_blocks = 0;
        state->free_descrs = 0;
    }
}

MemoryMapping::~MemoryMapping() {
    cleanup();
}

void MemoryMapping::init_transaction(Transaction *txn) {
    state->write_lock.lock();

    if (mprotect(base, size, PROT_READ | PROT_WRITE) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    current_base = base;

    txn->id = state->curr_txn_id;
    txn->top = state->top;
    txn->free_blocks = state->free_blocks;
    txn->unused_descrs = state->free_descrs;
    txn->pending_unused_descrs = 0;
    txn->pending_unused_last = 0;
    txn->lf_block.obj = 0;
    txn->lf_block.size = 0;
    txn->lf_block.txn_id = 0;
}

void MemoryMapping::commit_transaction(Transaction *txn) {
    auto saved_top = state->top;
    auto saved_free_list = state->free_blocks;
    auto saved_free_descriptors = state->free_descrs;

    if (txn->pending_unused_last != 0) {
        txn->pending_unused_last->next_unused = txn->unused_descrs;
        txn->unused_descrs = txn->pending_unused_descrs;
    }

    if (msync((void *) base, size, MS_SYNC | MS_INVALIDATE) != 0)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    state->top = txn->top;
    state->free_blocks = txn->free_blocks;
    state->free_descrs = txn->unused_descrs;
    state->curr_txn_id++;

    if (mprotect(base, size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (msync((void *) base, size, MS_SYNC | MS_INVALIDATE) != 0) {
        state->top = saved_top;
        state->free_blocks = saved_free_list;
        state->free_descrs = saved_free_descriptors;
        state->curr_txn_id--;
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // state->writer_pid = 0;

    state->write_lock.unlock();

}

void MemoryMapping::cleanup() noexcept {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    if (state != nullptr) {
        munmap(state, size);
        state = nullptr;
    }
}

size_t MemoryMapping::calc_mem_map_size(size_t init_size) {
    size_t new_size =
            ((init_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    if (new_size < MIN_PAGE_SIZE)
        new_size = MIN_PAGE_SIZE;

    return new_size;
}