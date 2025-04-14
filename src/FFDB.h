#ifndef FFDB_H
#define FFDB_H
#include "buddy_allocator.h"

#include <cstddef>

class FFDB {
private:
    int fd;
    size_t page_size;
    size_t memory_map_size;
    unsigned char *base;
    const char *filepath;

    BuddyAllocator *a;

public:
    FFDB(const char *filepath, int flags, int mode, size_t init_size);

    ~FFDB();
};

#endif //FFDB_H