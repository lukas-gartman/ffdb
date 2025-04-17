#include "FFDB.h"
#include <iostream>

FFDB::FFDB(std::string filepath, int flags, int mode, size_t init_size) {
    std::cout << "Filepath: " << filepath << std::endl;
    mem_mapping = new MemoryMapping(std::move(filepath), flags, mode, init_size);
    alloc = new Allocator(mem_mapping->state, mem_mapping->get_size());
}