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

    // Initialize the allocator with 1024 bytes and a block size of 32 bytes
    a = new BuddyAllocator(init_size, 32);

    // Allocate a block of 64 bytes
    Ref<unsigned int> block1 = a->malloc<unsigned int>(64 - sizeof(unsigned int));
    assert(block1 != nullptr);
    *block1 = 0xAABBCCDD;
    std::cout << "Allocated 64 bytes and set value to 0xAABBCCDD\n";

    // Allocate a block of 128 bytes
    Ref<unsigned int> block2 = a->malloc<unsigned int>(128 - sizeof(unsigned int));
    assert(block2 != nullptr);
    *block2 = 0x11223344;
    std::cout << "Allocated 128 bytes and set value to 0x11223344\n";

    // Free the first block
    a->free(block1, 64 - sizeof(unsigned int));
    std::cout << "Freed 64 bytes\n";

    // Free the second block
    a->free(block2, 128 - sizeof(unsigned int));
    std::cout << "Freed 128 bytes\n";

    // Allocate a block of 256 bytes
    Ref<unsigned int> block3 = a->malloc<unsigned int>(256 - sizeof(unsigned int));
    assert(block3 != nullptr);
    *block3 = 0x55667788;
    std::cout << "Allocated 256 bytes and set value to 0x55667788\n";

    // Free the third block
    a->free(block3, 256 - sizeof(unsigned int));
    std::cout << "Freed 256 bytes\n";

    // Test allocation of a block that is too large
    try {
        Ref<unsigned int> block4 = a->malloc<unsigned int>(2048 - sizeof(unsigned int)); // Should throw std::bad_alloc
        assert(false); // Should not reach here
    } catch (const std::bad_alloc&) {
        std::cout << "Caught std::bad_alloc as expected\n";
    }

    // Test fragmentation and coalescing
    Ref<unsigned int> block5 = a->malloc<unsigned int>(32 - sizeof(unsigned int));
    Ref<unsigned int> block6 = a->malloc<unsigned int>(32 - sizeof(unsigned int));
    a->free(block5, 32 - sizeof(unsigned int));
    a->free(block6, 32 - sizeof(unsigned int));
    Ref<unsigned int> block7 = a->malloc<unsigned int>(64 - sizeof(unsigned int)); // Should coalesce the two 32-byte blocks
    assert(block7 != nullptr);
    std::cout << "Allocated 64 bytes after coalescing\n";

    // Validate the coalesced block by writing to it
    *block7 = 0x99AABBCC;

    // This should appear after the coalesced block (64 bytes)
    Ref<unsigned int> block8 = a->malloc<unsigned int>(64 - sizeof(unsigned int)); // Should coalesce the two 32-byte blocks
    *block8 = 0xDDEEFF00;
}

FFDB::~FFDB() {
    free((void *)filepath);
}