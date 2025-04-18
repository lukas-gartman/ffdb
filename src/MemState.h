#ifndef DBSTATE_H
#define DBSTATE_H

#include "Typedefs.h"
#include "Synch.h"

struct MemState {
    std::size_t file_size;

    std::size_t curr_txn_id;
    std::size_t min_txn_id;

    Mutex write_lock;
    Mutex rev_lock;
    RWLock resize_rw_lock;

    Object top;              // Top of the memory map
    Object free_list;        // List of free block descriptors
    Object free_descriptors; // List of unused block descriptors
};

#endif //DBSTATE_H
