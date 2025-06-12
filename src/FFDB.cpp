#include "FFDB.h"
#include <iostream>

FFDB::FFDB(std::string filepath, int flags, int mode, size_t init_size) {
    mmap = new MemoryMapping(std::move(filepath), flags, mode, init_size);
    alloc = new Allocator([this]() { mmap->resize_map(); });
}

Revision *FFDB::register_revision(Object root, size_t txn_id) {
    return mmap->register_revision(root, txn_id);
}

void FFDB::unregister_revision(Revision *rev) {
    mmap->unregister_revision(rev);
}

void FFDB::start_txn(DBMode mode) {
    mmap->lock(mode);

    if (mode == DB_WRITE) {
        auto *ws = new WriterState();
        mmap->init_wr_txn(ws);
        alloc->activate(ws);
    }
}

void FFDB::cancel_txn(DBMode mode) {
    if (mode == DB_WRITE) {
        auto *ws = alloc->deactivate();
        delete ws;
    }

    mmap->unlock(mode);
}

void FFDB::commit_txn(DBMode mode) {
    if (mode == DB_WRITE) {
        auto *ws = alloc->deactivate();
        mmap->apply_wr_txn(ws);
        delete ws;
    }

    mmap->unlock(mode);
}