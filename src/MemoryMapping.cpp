#include "MemoryMapping.h"

#include <cstring>
#include <fcntl.h>
#include <iostream>
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
        state->free_list = 0;
        state->free_descriptors = 0;
    }
}

MemoryMapping::~MemoryMapping() {
    cleanup();
}

ssize_t MemoryMapping::calc_mem_map_size(size_t init_size) {
    // Round init_size to nearest multiple of page_size
    size_t new_size =
        ((init_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    if (new_size < MIN_PAGE_SIZE)
        new_size = MIN_PAGE_SIZE;

    return new_size;
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

size_t MemoryMapping::get_size() const noexcept {
    return size;
}