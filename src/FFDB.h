#ifndef FFDB_H
#define FFDB_H

#include "Allocator.h"
#include "MemoryMapping.h"

#include <cstddef>

class FFDB {
private:
    Transaction *curr_txn;

public:
    Allocator *alloc;
    MemoryMapping *mem_mapping;

    FFDB(std::string filepath, int flags, int mode, size_t init_size);

    void start_transaction();
};

#endif //FFDB_H