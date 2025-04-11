#ifndef DBSTATE_H
#define DBSTATE_H

#include "typedefs.h"
#include "Synch.h"

struct DBState {
    std::size_t file_size;

    // Parallelism here

    std::size_t curr_txn_id;
    std::size_t min_txn_id;

    Mutex write_lock;
    Mutex read_lock;
    RWLock resize_rw_lock;

    Object top;              // Top of the memory map
    Object free_list;        // List of free block descriptors
    Object free_descriptors; // List of unused block descriptors
};

#endif //DBSTATE_H
