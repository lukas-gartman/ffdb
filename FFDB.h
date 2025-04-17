#ifndef FFDB_H
#define FFDB_H

#include "Allocator.h"
#include "MemoryMapping.h"

#include <cstddef>

class FFDB {
private:
    Allocator *alloc;
    MemoryMapping *mem_mapping;

public:
    FFDB(std::string filepath, int flags, int mode, size_t init_size);
};


#endif //FFDB_H