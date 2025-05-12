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
shared_state(nullptr) {
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
        shared_state->free_blocks = 0;
        shared_state->free_descrs = 0;
    }
}

MemoryMapping::~MemoryMapping() {
    cleanup();
}

void MemoryMapping::init_txn(Transaction *txn) {
    if (txn == nullptr)
        throw std::runtime_error("No transaction started!");

    shared_state->write_lock.lock();

    if (mprotect(base, size, PROT_READ | PROT_WRITE) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    current_base = base;

    txn->id = shared_state->curr_txn_id;
    txn->top = shared_state->top;
    txn->free_blocks = shared_state->free_blocks;
    txn->unused_descrs = shared_state->free_descrs;
    txn->pending_unused_descrs = 0;
    txn->pending_unused_last = 0;
    txn->lf_block.obj = 0;
    txn->lf_block.size = 0;
    txn->lf_block.txn_id = 0;
}

void MemoryMapping::rollback_txn(Transaction *txn) {
    if (txn == nullptr)
        throw std::runtime_error("No transaction started!");

    // shared_state->writer_pid = 0;

    if (mprotect(base, size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    shared_state->write_lock.unlock();
}

void MemoryMapping::commit_txn(Transaction *txn) {
    if (txn == nullptr)
        throw std::runtime_error("No transaction started!");

    auto saved_top = shared_state->top;
    auto saved_free_list = shared_state->free_blocks;
    auto saved_free_descriptors = shared_state->free_descrs;

    if (txn->pending_unused_last != 0) {
        txn->pending_unused_last->next_unused = txn->unused_descrs;
        txn->unused_descrs = txn->pending_unused_descrs;
    }

    if (msync((void *) base, size, MS_SYNC | MS_INVALIDATE) != 0)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    shared_state->top = txn->top;
    shared_state->free_blocks = txn->free_blocks;
    shared_state->free_descrs = txn->unused_descrs;
    shared_state->curr_txn_id++;

    if (mprotect(base, size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (msync((void *) base, size, MS_SYNC | MS_INVALIDATE) != 0) {
        shared_state->top = saved_top;
        shared_state->free_blocks = saved_free_list;
        shared_state->free_descrs = saved_free_descriptors;
        shared_state->curr_txn_id--;
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // state->writer_pid = 0;

    shared_state->write_lock.unlock();

}

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
    std::cout << "resize" << std::endl;
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
    shared_state->mmap_size = new_size;
    base = new_base;
    current_base = (unsigned char *) new_base;

    shared_state->resize_rw_lock.unlock();
}