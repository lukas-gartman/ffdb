#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "Typedefs.h"
#include "MemState.h" // Block & BlockDescr

// TODO: Solve circular dependency

/**
 * \struct Transaction
 * \brief Represents a transaction and its associated memory state.
 *
 * The Transaction struct encapsulates metadata about a transaction. It is used
 * to manage memory allocation and deallocation during the transaction's
 * lifecycle, allowing for commit or rollback operations.
 */
struct Transaction {
    /** \brief A unique identifier for the transaction. */
    std::size_t id;

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

#endif //TRANSACTION_H
