#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "Typedefs.h"

// TODO: Temporary hack to avoid circular dependency
namespace Allocator {
    struct BlockDescr;
    struct Block;
}

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
    size_t id;

    /** \brief The offset to the top adress associated with the transaction. */
    Object top;

    /** \brief A reference to the list of free memory blocks. */
    Ref<Allocator::BlockDescr> free_blocks;

    /** \brief A reference to the list of free memory block descriptors. */
    Ref<Allocator::BlockDescr> free_descrs;

    /** \brief A reference to the list of block descriptors freed during this
     * transaction. */
    Ref<Allocator::BlockDescr> pending_free_descrs;

    /** \brief A reference to the last element of the \ref pending_free_descrs
     * list. */
    Ref<Allocator::BlockDescr> pending_free_last;

    /** \brief A reference to the block that was freed most recently. */
    Ref<Allocator::Block> last_free_block;
};

#endif //TRANSACTION_H
