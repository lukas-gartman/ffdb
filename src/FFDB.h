#ifndef FFDB_H
#define FFDB_H

#include "Allocator.h"
#include "MemoryMapping.h"

class FFDB {
public:
    Allocator *alloc;
    MemoryMapping *mmap;

    FFDB(std::string filepath, int flags, int mode, size_t init_size);

    Revision *register_revision(Object root, size_t txn_id);
    void unregister_revision(Revision *rev);

    void start_txn();
    void cancel_txn();
    void commit_txn();
};

#endif //FFDB_H