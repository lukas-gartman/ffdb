#include "FFDB.h"
#include "typedefs.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>

// Align init_size to page_size
static inline ssize_t get_memory_map_size(size_t init_size, size_t page_size) {
    size_t memory_map_size =
        ((init_size + page_size - 1) / page_size) * page_size;

    if (memory_map_size < page_size * 2) memory_map_size = page_size * 2;

    return (ssize_t)memory_map_size;
}

FFDB::FFDB(const char *filepath, int flags, int mode, size_t init_size) {
    if (filepath == nullptr) throw std::invalid_argument("filepath is null");

    page_size = getpagesize();

    fd = open(filepath, flags, mode);
    if (fd == -1) throw std::runtime_error("Errno: " + std::to_string(errno));

    memory_map_size = lseek(fd, 0, SEEK_END);
    if (memory_map_size == -1) {
        close(fd);
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // Initialize file if it was just created.
    if (memory_map_size == 0) {
        memory_map_size = get_memory_map_size(init_size, page_size);
        if (ftruncate(fd, memory_map_size) == -1) {
            close(fd);
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }
    }

    this->filepath = strdup(filepath);
    if (this->filepath == nullptr) {
        close(fd);
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    base = (unsigned char *)(
        mmap(nullptr,
             memory_map_size,
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             fd,
             0
            )
    );
    if (base == MAP_FAILED) {
        ::free((void *)this->filepath);
        close(fd);
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    current_base = base;

    a = new BuddyAllocator(init_size, 32);
    Ref<unsigned int> test = a->malloc<unsigned int>();
    *test = 0xAABBCCDD;
}

FFDB::~FFDB() {
    free((void *)filepath);
}