#ifndef FFDB_H
#define FFDB_H

#include <cassert>
#include <optional>

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
    Offset ofs;

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

    Size tree_size;
    Ref<BlockDescr> tree_left;
    Ref<BlockDescr> tree_right;

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

// TODO: Should make this non-mutable in some way.
struct DBConf {
private:
    Size sys_page_size;
    Size log_page_size;
    Size align;
    Size bdtree_delta;
    Size bdtree_gamma;
    bool page_boundry_aware;

public:
    DBConf(
        Size sys_page_size, Size log_page_size,
        Size small_align,
        Size bdtree_delta, Size bdtree_gamma, bool page_boundry_aware
    ):
    sys_page_size(sys_page_size), log_page_size(log_page_size),
    align(small_align),
    bdtree_delta(bdtree_delta), bdtree_gamma(bdtree_gamma),
    page_boundry_aware(page_boundry_aware) {}

    DBConf() : DBConf(FFDB_PHYSICAL_PAGE_SIZE, FFDB_LOGICAL_PAGE_SIZE, FFDB_ALIGN, 3, 2, FFDB_PAGE_BOUNDARY_AWARE) {};

    Size get_sys_page_size() const { return sys_page_size; }
    Size get_log_page_size() const { return log_page_size; }
    Size get_align() const { return align; }
    Size get_bdtree_delta() const { return bdtree_delta; }
    Size get_bdtree_gamma() const { return bdtree_gamma; }
    bool is_page_boundry_aware() const { return page_boundry_aware; }
};

struct SharedState {
    Size mmap_size;
    DBConf conf;

    Id curr_txn_id;
    Id min_txn_id;

    Mutex write_lock;
    Mutex rev_lock;
    RWLock resize_rw_lock;

    Offset top;
    Ref<BlockDescr> free_blocks;
    Ref<BlockDescr> free_descrs;

    uint32_t revision_count;
    Offset active_txn_root;
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
    Offset txn_root;

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

/** \brief Represents the current database transaction state.
 *
 *  Used to determine what transaction state the database currently is in.
 */
enum DBMode {
    DB_NO_TXN,   /**< No active transaction is in progress. */
    DB_READ_TXN, /**< A read-only transaction is active. */
    DB_WRITE_TXN /**< A write transaction is active, allowing modifications. */
};

class FFDB {
private:
    pid_t pid;
    int fd;
    Size mmap_size;
    unsigned char *base;
    SharedState *shared_state;
    string filepath;
    Revision* registered_rev;

    DBMode db_mode;
    WriterState writer_state;

    // Allocation related functions
    Offset malloc_internal(Size bytes);
    void free_internal(Offset offset, Size bytes);

    void set_lf_block(Block lf_block);
    Ref<BlockDescr> new_block_descr(Block block, Ref<BlockDescr> left = 0, Ref<BlockDescr> right = 0);
    Ref<BlockDescr> upd_bd_tree_node(Ref<BlockDescr> tree, Ref<BlockDescr> left, Ref<BlockDescr> right);
    Ref<BlockDescr> ins_bd_tree(Ref<BlockDescr> tree, Block block);
    std::pair<Ref<BlockDescr>, Block> del_bd_tree(Ref<BlockDescr> tree, Size block_size);
    std::pair<Ref<BlockDescr>, Ref<BlockDescr>> pop_first_bd(Ref<BlockDescr> tree);
    std::pair<Ref<BlockDescr>, Ref<BlockDescr>> pop_last_bd(Ref<BlockDescr> tree);
    Ref<BlockDescr> rotl_bd_tree_node(Ref<BlockDescr> tree);
    Ref<BlockDescr> rotr_bd_tree_node(Ref<BlockDescr> tree);
    Ref<BlockDescr> rotll_bd_tree_node(Ref<BlockDescr> tree);
    Ref<BlockDescr> rotrr_bd_tree_node(Ref<BlockDescr> tree);
    Ref<BlockDescr> balancel_bd_tree(Ref<BlockDescr> tree);
    Ref<BlockDescr> balancer_bd_tree(Ref<BlockDescr> tree);

    Size get_size(Ref<BlockDescr> node);
    std::pair<Ref<BlockDescr>, Ref<BlockDescr>> get_children(Ref<BlockDescr> node);

    void lock();
    void unlock();

    void cleanup() noexcept;

    void resize_map(Size min_size);

    template <typename T>
    Ref<T> copy(Ref<T> obj) {
        auto copy = malloc<T>();
        memcpy(copy, obj, sizeof(T));
        return copy;
    }

public:
    FFDB(string filepath, int flags, int mode, Size init_size, std::optional<DBConf> conf = std::nullopt);

    DBConf conf;

    template<class A> Ref<A> malloc(Size elem_amount = 1) {
        if (elem_amount == 0) return 0;
        return malloc_internal(sizeof(A) * elem_amount);
    }

    template<class A> void free(Ref<A> ref, Size elem_amount = 1) {
        if (elem_amount == 0) return;
        free_internal(ref.as_offset(), sizeof(A) * elem_amount);
    }

    template<class A> Ref<A> malloc_bytes(Size bytes) {
        return malloc_internal(bytes);
    }

    template<class A> void free_bytes(Ref<A> ref, Size bytes) {
        free_internal(ref.as_offset(), bytes);
    }

    Revision *register_revision(Offset root, Id txn_id);
    void unregister_revision(Revision *rev);

    Offset start_txn(DBMode mode);
    void rollback_txn();
    void commit_txn(Offset txn_root = 0);

    Id get_txn_id() const {
        return shared_state->curr_txn_id;
    }

    void validate_tree(Ref<BlockDescr> tree) {
        if (!tree) return;

        assert(is_balanced(tree));
    }

    bool is_balanced(Ref<BlockDescr> tree) {
        if (tree == 0) return true;
        return get_size(tree->tree_left) + get_size(tree->tree_right) <= 1
            || get_size(tree->tree_left) <= conf.get_bdtree_delta() * get_size(tree->tree_right)
            && get_size(tree->tree_right) <= conf.get_bdtree_delta() * get_size(tree->tree_left)
            && is_balanced(tree->tree_left) && is_balanced(tree->tree_right);
    }
};

#endif //FFDB_H
