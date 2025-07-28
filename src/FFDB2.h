#ifndef FFDB2_H
#define FFDB2_H

#include "Typedefs.h"
#include "Synch.h"

using string = std::string;

/**
 * \struct Block
 * \brief Represents a block of memory that has been freed and can be used.
 *
 * The `Block` struct represents a block that is not allocated. It
 * free memory before top since all memory after the top is free
 * already.
 */
struct Block {
    /** \brief The position of the block in memory. */
    Offset obj;

    /** \brief The size of the block. */
    Size size;

    /** \brief The id of the transaction that freed this block. */
    Id txn_id;
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
    Id txn_id;
};

struct Revision {
    pid_t pid;
    uint32_t ref_count;
    Offset root;
    Id txn_id;
};

struct SharedState {
    Size mmap_size;

    Id curr_txn_id;
    Id min_txn_id;

    Mutex write_lock;
    Mutex rev_lock;
    RWLock resize_rw_lock;

    Offset top;
    Ref<BlockDescr> free_blocks;
    Ref<BlockDescr> free_descrs;

    uint32_t revision_count;
    Ref<Revision> active_revision;
    Revision revisions[];
};

/**
 * \struct WriterState
 * \brief Represents a transaction and its associated memory state.
 *
 * The Transaction struct stores data about a transaction. It is used to manage memory allocation and deallocation
 * during the transaction's lifecycle, allowing for commit or rollback operations.
 */
struct WriterState {
    /** \brief The offset to the top adress associated with the transaction. */
    Offset top;

    /** \brief A reference to the list of free memory blocks. */
    Ref<BlockDescr> free_blocks;

    /** \brief A reference to the list of free memory block descriptors. During
     * a transaction the descriptors in this list can be reused. */
    Ref<BlockDescr> unused_descrs;

    /** \brief A reference to the list of block descriptors freed during this
     * transaction.  */
    Ref<BlockDescr> pending_unused_descrs;

    /** \brief A reference to the last element of the \ref pending_unused_descrs
     * list. */
    Ref<BlockDescr> pending_unused_last;

    /** \brief Last free block. A reference to the block that was freed most recently. */
    Block lf_block;

    /** \brief Left over block. A reference to the block that was left over because of alignment. */
    Block lo_block;
};

enum DBMode {
    DB_NO_TXN,
    DB_READ_TXN,
    DB_WRITE_TXN
};

class FFDB {
private:
    pid_t pid;
    int fd;
    Size mmap_size;
    unsigned char *base;
    SharedState *shared_state;
    string filepath;

    DBMode db_mode;
    WriterState writer_state;

    // Allocation related functions
    Offset malloc_internal(Size bytes);
    void free_internal(Offset obj, Size bytes);

    Ref<BlockDescr> new_block_descr(Block block);
    Ref<BlockDescr> upd_block_descr(Ref<BlockDescr> descr);
    void insert_block_descr(Block block);
    Offset delete_block_descr(Size block_size);

    void lock();
    void unlock();

    void cleanup() noexcept;

    void resize_map(Size min_size);

public:
    FFDB(string filepath, int flags, int mode, Size init_size);

    template<class A> Ref<A> malloc(Size elem_amount = 1) {
        if (elem_amount == 0) return 0;
        return malloc_internal(sizeof(A) * elem_amount);
    }

    template<class A> void free(Ref<A> obj, Size elem_amount = 1) {
        if (elem_amount == 0) return;
        free_internal(obj.as_offset(), sizeof(A) * elem_amount);
    }

    Revision *register_revision(Offset root, Id txn_id);
    void unregister_revision(Revision *rev);

    void start_txn(DBMode mode);
    void rollback_txn();
    void commit_txn();

    Id get_txn_id() const {
        return shared_state->curr_txn_id;
    }

    void dump_block_descrs() const;
};

#endif //FFDB2_H
