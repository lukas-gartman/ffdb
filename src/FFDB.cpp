#include "FFDB.h"
#include <iostream>

FFDB::FFDB(std::string filepath, int flags, int mode, size_t init_size) {
    mmap = new MemoryMapping(std::move(filepath), flags, mode, init_size);
    alloc = new Allocator(mmap);
}

Ref<Revision> FFDB::register_revision(Object root, size_t txn_id) {
    return mmap->register_revision(root, txn_id);
}

void FFDB::unregister_revision(Ref<Revision> rev) {
    mmap->unregister_revision(rev);
}

void FFDB::start_transaction() {
    alloc->start_transaction();
}

void FFDB::rollback_transaction() {
    alloc->rollback_transaction();
}

void FFDB::commit_transaction() {
    alloc->commit_transaction();
}