#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "Typedefs.h"
#include "MemState.h" // Block & BlockDescr

/**
 * \struct WriterState
 * \brief Represents a transaction and its associated memory state.
 *
 * The Transaction struct stores data about a transaction. It is used to manage memory allocation and deallocation
 * during the transaction's lifecycle, allowing for commit or rollback operations.
 */
struct WriterState {
    /** \brief The unique identifier of the transaction that created the oldest version of the database still in use.*/
    size_t min_txn_id;

    /** \brief A unique identifier for the transaction. */
    size_t id;

    /** \brief The size of the memory map in bytes. */
    size_t mmap_size;

    /** \brief The offset to the top adress outside of the transaction. */
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

    /** \brief Last free block. A reference to the block that was freed most recently. */
    Block lf_block;

    /** \brief Left over block. A reference to the block that was left over because of alignment. */
    Block lo_block;
};

enum TxnMode {
    TXN_READ,
    TXN_WRITE
};

#endif //TRANSACTION_H
