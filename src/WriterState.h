#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "Typedefs.h"
#include "MemState.h" // Block & BlockDescr

/**
 * \struct WriterState
 * \brief Represents a transaction and its associated memory state.
 *
 * The Transaction struct encapsulates metadata about a transaction. It is used
 * to manage memory allocation and deallocation during the transaction's
 * lifecycle, allowing for commit or rollback operations.
 */
struct WriterState {
    size_t min_txn_id;

    /** \brief A unique identifier for the transaction. */
    size_t id;

    size_t mmap_size;

    Object old_top;

    /** \brief The offset to the top adress associated with the transaction. */
    Object top;

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

    /** \brief A reference to the block that was freed most recently. */
    Block lf_block;
};

enum TxnMode {
    TXN_READ,
    TXN_WRITE
};

#endif //TRANSACTION_H
