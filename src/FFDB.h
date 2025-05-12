#ifndef FFDB_H
#define FFDB_H

#include "Allocator.h"
#include "MemoryMapping.h"

#include <cstddef>

class FFDB {
public:
    Allocator *alloc;
    MemoryMapping *mmap;

    FFDB(std::string filepath, int flags, int mode, size_t init_size);

    Ref<Revision> register_revision(Object root, size_t txn_id);
    void unregister_revision(Ref<Revision> rev);

    void start_transaction();
    void rollback_transaction();
    void commit_transaction();
};

#endif //FFDB_H