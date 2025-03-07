#include "FFDB.h"
#include "typedefs.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>

// Align init_size to page_size
static inline ssize_t get_memory_map_size(size_t init_size, size_t page_size) {
    size_t memory_map_size =
        ((init_size + page_size - 1) / page_size) * page_size;

    if (memory_map_size < page_size * 2) memory_map_size = page_size * 2;

    return (ssize_t)memory_map_size;
}

FFDB::FFDB(const char *filepath, int flags, int mode, size_t init_size) {
    assert(filepath != nullptr);

    //fd = -1;
    page_size = getpagesize();

    fd = open(filepath, flags, mode);
    assert(fd >= 0);

    memory_map_size = lseek(fd, 0, SEEK_END);
    assert(memory_map_size != (size_t) (-1));

    // Assume memory_map_size == 0
    memory_map_size = get_memory_map_size(init_size, page_size);
    assert(ftruncate(fd, memory_map_size) >= 0);

    this->filepath = strdup(filepath);
    assert(this->filepath != nullptr);

    base = (unsigned char *)(
        mmap(nullptr,
             memory_map_size,
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             fd,
             0
            )
    );

    current_base = base;

    unsigned char *byte = current_base;

    a = new BuddyAllocator(init_size, 32);
    Ref<unsigned int> firstInt = a->malloc<unsigned int>();
    Ref<unsigned char> firstByte = a->malloc<unsigned char>();
    *firstInt = 0xAABBCCDD;
    *firstByte = 0xFF;
    delete a;
}

FFDB::~FFDB() {
    free((void *)filepath);
}