#ifndef DBSTATE_H
#define DBSTATE_H

#include "Typedefs.h"
#include "Synch.h"

/**
    * \struct Block
    * \brief Represents a block of memory that has been freed and can be
    * used.
    *
    * The `Block` struct represents a block that is not allocated. It
    * free memory before top since all memory after the top is free
    * already.
    */
struct Block {
    /** \brief The position of the block in memory. */
    Object obj;

    /** \brief The size of the block. */
    std::size_t size;

    /** \brief The id of the transaction that freed this block. */
    std::size_t txn_id;
};

/**
* \struct BlockDescr
* \brief Describes a block of memory and its relationships to other blocks.
*
* The `BlockDescr` structure is used to represent a memory block and its
* connections to other free or unused blocks. It helps manage memory
* allocation and deallocation efficiently by maintaining linked lists
* of free and unused blocks. A block descriptor is either used or
* unused. Hence only its position in one of the lists is relevant.
*/
struct BlockDescr {
    /** \brief The memory block associated with this descriptor. */
    Block block;

    /** \brief A reference to the next free block. */
    Ref<BlockDescr> next_free;

    /** \brief A reference to the next unused block descriptor. */
    Ref<BlockDescr> next_unused;

    /** \brief The id of the transaction which allocated the
     * descriptor. */
    std::size_t txn_id;
};

struct Revision {
    pid_t pid;
    uint32_t ref_count;
    Object root;
    size_t txn_id;
};

struct MemState {
    size_t mmap_size;

    size_t curr_txn_id;
    size_t min_txn_id;

    Mutex write_lock;
    Mutex rev_lock;
    RWLock resize_rw_lock;

    Object top;
    Ref<BlockDescr> free_blocks;
    Ref<BlockDescr> free_descrs;

    size_t revision_count;
    Ref<Revision> active_revision;
    Revision revisions[];
};

#endif //DBSTATE_H
