#ifndef DBSTATE_H
#define DBSTATE_H

#include "typedefs.h"

struct DBState {
    std::size_t file_size;

    Object top;              // Top of the memory map
    Object free_list;        // List of free block descriptors
    Object free_descriptors; // List of unused block descriptors
};

#endif //DBSTATE_H
