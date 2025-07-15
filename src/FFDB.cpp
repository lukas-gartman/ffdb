#include "FFDB.h"
#include <iostream>

FFDB::FFDB(std::string filepath, int flags, int mode, size_t init_size) {
    mem_mapping = new MemoryMapping(std::move(filepath), flags, mode, init_size);
    alloc = new Allocator(mem_mapping);
}

void FFDB::start_transaction() {
    alloc->start_transaction();
}

void FFDB::commit_transaction() {
    alloc->commit_transaction();
}

void FFDB::hello_ffdb() {
    std::cout << "hello ffdb" << std::endl;
}