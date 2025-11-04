#include "FFDB.h"
#include "Memutils.h"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/mman.h>
#include <cmath>
#include <map>

Offset FFDB::malloc_internal(Size bytes) {
    if (db_mode != DB_WRITE_TXN)
        throw std::runtime_error("Database not in write mode.");

    if (conf.is_page_boundry_aware())
        bytes = align_bytes(bytes, bytes >= conf.get_align_threshold() ? conf.get_large_align() : conf.get_small_align());
    else
        bytes = align_bytes(bytes, conf.get_small_align());

    Offset offset;

    // Check if there is a free block cached that can be used.
    if (writer_state.lf_block.ofs != 0 &&
        writer_state.lf_block.size >= bytes &&
        writer_state.lf_block.txn_id < shared_state->min_txn_id) {

        offset = writer_state.lf_block.ofs;
        writer_state.lf_block.ofs += bytes;
        writer_state.lf_block.size -= bytes;

        if (writer_state.lf_block.size == 0) {
            writer_state.lf_block.ofs = 0;
            writer_state.lf_block.txn_id = 0;
        }

        return offset;
    }

    // TODO: Any case where there is a free block that is large enough for a node but not correctly aligned?
    // Try finding a fitting block in the block descriptor list.
    auto [new_tree, found_block] = del_bd_tree(writer_state.free_blocks, bytes);
    writer_state.free_blocks = new_tree;
    offset = found_block.ofs;
    if (offset != 0) {
        if (found_block.size > bytes)
            set_lf_block({offset + bytes, found_block.size - bytes, found_block.txn_id});

        return offset;
    }

    if (bytes > mmap_size - writer_state.top) resize_map(bytes);

    if (conf.is_page_boundry_aware() && bytes >= conf.get_align_threshold()) {
        auto bytes_left = bytes_left_in_page(writer_state.top, conf.get_log_page_size());

        // Maybe not obvious: at first glance it may look like we need a specialized resize check here where we check
        // bytes + bytes_left instead. This is not necessary as bytes and bytes + bytes_left occupies the exact same pages.
        // We have that bytes == 4096 * amount_of_pages - bytes_left, where bytes_left is the amount of bytes not occupied by
        // bytes in the last page and amount_of_pages > 1. Hence bytes + bytes_left == amount_of_pages.

        // TODO: What txn_id is appropriate here?
        Block lo_block = {bytes_left > 0 ? writer_state.top : 0, bytes_left, 0};

        // Block descriptor fits in lo_block and lo_block fits the smallest allocatable size.
        Size bd_size = sizeof(BlockDescr);
        if (bytes_left > bd_size + conf.get_small_align()) {
            lo_block.ofs += bd_size;
            lo_block.size -= bd_size;

            writer_state.free_blocks = ins_bd_tree(writer_state.free_blocks, lo_block);
            bytes_left -= bd_size; // Update bytes_left because ins_bd_descr updated writer_state.top.
            lo_block = {0, 0, 0};
        }

        writer_state.top += bytes_left;
        offset = writer_state.top;
        writer_state.top += bytes;

        // lo_block exists and has not been inserted yet.
        if (lo_block.ofs > 0) set_lf_block(lo_block);
    }
    else {
        offset = writer_state.top;
        writer_state.top += bytes;
    }

    return offset;
}

void FFDB::free_internal(Offset offset, Size bytes) {
    if (db_mode != DB_WRITE_TXN)
        throw std::runtime_error("Database not in write mode.");

    if (offset == 0) return;

    Size align = bytes > conf.get_align_threshold() ? conf.get_large_align() : conf.get_small_align();
    bytes = align_bytes(bytes, align);

    Size txn_id = (offset >= shared_state->top) ? 0 : shared_state->curr_txn_id;

    if (txn_id == 0 && offset + bytes == writer_state.top) {
        writer_state.top -= bytes;

        if (writer_state.lf_block.ofs != 0 &&
            writer_state.lf_block.txn_id == 0 &&
            writer_state.lf_block.ofs + writer_state.lf_block.size == writer_state.top) {

            writer_state.top -= writer_state.lf_block.size;
            writer_state.lf_block = {0, 0, 0};
        }

        return;
    }

    if (writer_state.lf_block.ofs != 0) {
        // Merge with lf_block is adjacent.
        const bool lf_block_is_before =
                writer_state.lf_block.ofs + writer_state.lf_block.size == offset;
        const bool lf_block_is_after =
                offset + bytes == writer_state.lf_block.ofs;

        if ((lf_block_is_before || lf_block_is_after) &&
            writer_state.lf_block.txn_id == txn_id) {

            writer_state.lf_block.size += bytes;
            if (lf_block_is_after) writer_state.lf_block.ofs = offset;

            return;
        }

        // Otherwise, it will get replaced.
        writer_state.free_blocks = ins_bd_tree(writer_state.free_blocks, writer_state.lf_block);
    }

    // 0 as an id here means that the last free block is located beyond
    // the top of the memory map. This is the case when the last free block
    // couldn't be merged with the transaction local top. This signifies
    // that the block can be immediately reused if it would be inserted
    // into the blockdescriptor list.
    writer_state.lf_block = {offset, bytes, txn_id};
}

void FFDB::set_lf_block(Block lf_block) {
    if (writer_state.lf_block.ofs != 0) {
        writer_state.free_blocks = ins_bd_tree(
            writer_state.free_blocks,
            writer_state.lf_block
        );
    }

    writer_state.lf_block = lf_block;
}

Ref<BlockDescr> FFDB::new_block_descr(Block block, Ref<BlockDescr> left, Ref<BlockDescr> right) {
    Ref<BlockDescr> descr = 0;

    if (writer_state.unused_descrs != 0) {
        descr = writer_state.unused_descrs;
        writer_state.unused_descrs =
                writer_state.unused_descrs->next_unused;
    }
    else {
        const Size bytes = align_bytes(sizeof(BlockDescr), conf.get_small_align());

        if (bytes > shared_state->mmap_size - writer_state.top)
            resize_map(bytes);

        descr = writer_state.top;
        writer_state.top += bytes;
    }

    Size tree_size = 1 + get_size(left) + get_size(right);

    *descr = {
        block,
        tree_size,
        left,
        right,
        0,
        shared_state->curr_txn_id
    };

    return descr;
}

Ref<BlockDescr> FFDB::upd_bd_tree_node(Ref<BlockDescr> tree, Ref<BlockDescr> left, Ref<BlockDescr> right) {
    // If the descr is not from a previous transaction, there is no need
    // to do copy-on-write.
    if (tree->txn_id == shared_state->curr_txn_id) {
        tree->tree_size = 1 + get_size(left) + get_size(right);
        tree->tree_left = left;
        tree->tree_right = right;
        return tree;
    }

    // Add the old descriptor to the pending unused list. In case of
    // a rollback, this descriptor will still have its old position in
    // the free list from before the start of the current_transaction.
    tree->next_unused = writer_state.pending_unused_descrs;
    writer_state.pending_unused_descrs = tree;
    if (writer_state.pending_unused_last == 0)
        writer_state.pending_unused_last = tree;

    Ref<BlockDescr> new_node = new_block_descr(tree->block, left, right);

    return new_node;
}

Ref<BlockDescr> FFDB::ins_bd_tree(Ref<BlockDescr> tree, Block block) {
    if (tree == 0) return new_block_descr(block, 0, 0);

    bool traverse_left =
        block.size != tree->block.size ? block.size < tree->block.size :
        block.txn_id != tree->block.txn_id ? block.txn_id < tree->block.txn_id :
        get_size(tree->tree_left) < get_size(tree->tree_right);

    if (traverse_left) {
        auto new_left = ins_bd_tree(tree->tree_left, block);
        tree = upd_bd_tree_node(tree, new_left, tree->tree_right);
        tree = balancel_bd_tree(tree);
    }
    else {
        auto new_right = ins_bd_tree(tree->tree_right, block);
        tree = upd_bd_tree_node(tree, tree->tree_left, new_right);
        tree = balancer_bd_tree(tree);
    }

    return tree;
}

std::pair<Ref<BlockDescr>, Block> FFDB::del_bd_tree(Ref<BlockDescr> tree, Size block_size) {
    if (tree == 0) return std::make_pair(0, Block{0, 0, 0});

    // We traverse left if block_size fits but are too large or block size is a perfect fit but the block belongs
    // to a revision that is still in use.
    bool traverse_left =
            block_size != tree->block.size ? block_size < tree->block.size :
            shared_state->min_txn_id < tree->block.txn_id;

    // If the block is a perfect fit and belongs to a revision that is not in use, we can use it and don't need to
    // search further.
    bool found =
        (block_size == tree->block.size &&
            shared_state->min_txn_id >= tree->block.txn_id);

    if (!traverse_left && !found) {
        // No need to fit block in this case as if it needs to be done, the if statement above will be called in some
        // recursive call and hence fit will be carried out anyways.
        auto [new_tree_right, found_block] =
            del_bd_tree(tree->tree_right, block_size);

        if (found_block.ofs != 0) {
            tree = upd_bd_tree_node(tree, tree->tree_left, new_tree_right);
            tree = balancel_bd_tree(tree);
        }

        return std::make_pair(tree, found_block);
    }

    if (traverse_left && !found) {
        auto [new_tree_left, found_block] =
            del_bd_tree(tree->tree_left, block_size);

        // We have found a fitting block. Return.
        if (found_block.ofs != 0) {
            tree = balancer_bd_tree(upd_bd_tree_node(tree, new_tree_left, tree->tree_right));
            return std::make_pair(tree, found_block);
        }

        // No perfect fit found in the left subtree and all larger blocks are in use.
        if (tree->block.txn_id >= shared_state->min_txn_id)
            return std::make_pair(tree, Block{0, 0, 0});

        // We have not found a perfect fit in the left subtree, but we can use the
        // current block.
    }

    auto bd_to_delete = tree;

    // Found a fit, remove the current descriptor and replace it with a fitting one.
    if (!tree->tree_left) tree = tree->tree_right;
    else if (!tree->tree_right) tree = tree->tree_left;
    else {
        if (tree->tree_left->tree_size > tree->tree_right->tree_size) {
            auto [new_tree, new_left] = pop_last_bd(tree->tree_left);
            tree = balancer_bd_tree(upd_bd_tree_node(new_left, new_tree, tree->tree_right));
        }
        else {
            auto [new_tree, new_right] = pop_first_bd(tree->tree_right);
            tree = balancel_bd_tree(upd_bd_tree_node(new_right, tree->tree_left, new_tree));
        }
    }

    if (bd_to_delete->txn_id == shared_state->curr_txn_id) {
        bd_to_delete->next_unused = writer_state.unused_descrs;
        writer_state.unused_descrs = bd_to_delete;
    }
    else {
        bd_to_delete->next_unused = writer_state.pending_unused_descrs;
        writer_state.pending_unused_descrs = bd_to_delete;
        if (!writer_state.pending_unused_last)
            writer_state.pending_unused_last = bd_to_delete;
    }

    return std::make_pair(tree, bd_to_delete->block);
}

std::pair<Ref<BlockDescr>, Ref<BlockDescr>> FFDB::pop_first_bd(Ref<BlockDescr> tree) {
    if (tree == 0) return std::make_pair(tree, 0);

    if (tree->tree_left == 0) return std::make_pair(tree->tree_right, tree);

    auto [new_tree_left, popped_bd] = pop_first_bd(tree->tree_left);

    tree = balancer_bd_tree(upd_bd_tree_node(tree, new_tree_left, tree->tree_right));
    return std::make_pair(tree, popped_bd);
}

std::pair<Ref<BlockDescr>, Ref<BlockDescr>> FFDB::pop_last_bd(Ref<BlockDescr> tree) {
    if (tree == 0) return std::make_pair(tree, 0);

    if (tree->tree_right == 0) return std::make_pair(tree->tree_left, tree);

    auto [new_tree_right, popped_bd] = pop_last_bd(tree->tree_right);

    tree = balancel_bd_tree(upd_bd_tree_node(tree, tree->tree_left, new_tree_right));
    return std::make_pair(tree, popped_bd);
}

Ref<BlockDescr> FFDB::rotl_bd_tree_node(Ref<BlockDescr> tree) {
    if (tree == 0 || tree->tree_right == 0) return tree;

    auto l = tree->tree_left;
    auto r = tree->tree_right;
    auto rl = r->tree_left;
    auto rr = r->tree_right;

    return upd_bd_tree_node(r, upd_bd_tree_node(tree, l, rl), rr);
}

Ref<BlockDescr> FFDB::rotr_bd_tree_node(Ref<BlockDescr> tree) {
    if (tree == 0 || tree->tree_left == 0) return tree;

    auto l = tree->tree_left;
    auto ll = l->tree_left;
    auto lr = l->tree_right;
    auto r = tree->tree_right;

    return upd_bd_tree_node(l, ll, upd_bd_tree_node(tree, lr, r));
}

Ref<BlockDescr> FFDB::rotll_bd_tree_node(Ref<BlockDescr> tree) {
    if (tree == 0 || tree->tree_right == 0 || tree->tree_right->tree_left == 0)
        return tree;

    auto l  = tree->tree_left;
    auto r  = tree->tree_right;
    auto rl = r->tree_left;
    auto rr = r->tree_right;
    auto rll = rl->tree_left;
    auto rlr = rl->tree_right;

    return upd_bd_tree_node(rl, upd_bd_tree_node(tree, l, rll), upd_bd_tree_node(r, rlr, rr)
    );
}

Ref<BlockDescr> FFDB::rotrr_bd_tree_node(Ref<BlockDescr> tree) {
    if (tree == 0 || tree->tree_left == 0 || tree->tree_left->tree_right == 0)
        return tree;

    auto l = tree->tree_left;
    auto ll = l->tree_left;
    auto lr = l->tree_right;
    auto lrl = lr->tree_left;
    auto lrr = lr->tree_right;
    auto r = tree->tree_right;

    return upd_bd_tree_node(lr, upd_bd_tree_node(l, ll, lrl), upd_bd_tree_node(tree, lrr, r));
}

Ref<BlockDescr> FFDB::balancel_bd_tree(Ref<BlockDescr> tree) {
    if (tree == 0) return 0;

    auto left = tree->tree_left;
    auto right = tree->tree_right;
    auto [left_left, left_right] = get_children(left);

    if (get_size(left) > conf.get_bdtree_delta() * get_size(right)) {
        if (get_size(left_right) < conf.get_bdtree_gamma() * get_size(left_left))
            tree = rotr_bd_tree_node(tree);
        else
            tree = rotrr_bd_tree_node(tree);
    }

    return tree;
}

Ref<BlockDescr> FFDB::balancer_bd_tree(Ref<BlockDescr> tree) {
    if (tree == 0) return 0;

    auto left = tree->tree_left;
    auto right = tree->tree_right;
    auto [right_left, right_right] = get_children(right);

    if (get_size(right) > conf.get_bdtree_delta() * get_size(left)) {
        if (get_size(right_left) < conf.get_bdtree_gamma() * get_size(right_right))
            tree = rotl_bd_tree_node(tree);
        else
            tree = rotll_bd_tree_node(tree);
    }

    return tree;
}

Size FFDB::get_size(Ref<BlockDescr> node) {
    return node ? node->tree_size : 0;
}

std::pair<Ref<BlockDescr>, Ref<BlockDescr>> FFDB::get_children(Ref<BlockDescr> node) {
    if (node == 0) return std::make_pair(0, 0);
    return std::make_pair(node->tree_left, node->tree_right);
}


// TODO: Handle DB_NO_TXN
void FFDB::lock() {
    if (mmap_size != shared_state->mmap_size) resize_map(0);

    if (db_mode == DB_WRITE_TXN) shared_state->write_lock.lock();
    else if (db_mode == DB_READ_TXN) shared_state->resize_rw_lock.read_lock();
    else throw std::invalid_argument("Invalid DBMode");
}

void FFDB::unlock() {
    if (db_mode == DB_WRITE_TXN) shared_state->write_lock.unlock();
    else if (db_mode == DB_READ_TXN) shared_state->resize_rw_lock.unlock();
    else throw std::invalid_argument("Invalid DBMode");
}

// TODO: Throws no exceptions but close and munmap may fail. Correct use of noexcept?
void FFDB::cleanup() noexcept {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    if (shared_state != nullptr) {
        munmap(shared_state, mmap_size);
        shared_state = nullptr;
    }
}

void FFDB::resize_map(Size min_size) {
    shared_state->resize_rw_lock.write_lock();

    // It may happen that another process already resized the map, in
    // that case we only need to remap the memory and update size.
    const size_t new_size =
        shared_state->mmap_size != mmap_size ? shared_state->mmap_size : next_pow2_size(mmap_size, min_size);

    // Should not need to check fd >= 0 as we don't support anon mappings.
    if (shared_state->mmap_size != new_size) {
        if (ftruncate(fd, conf.get_sys_page_size() + new_size) == -1) {
            shared_state->resize_rw_lock.unlock();
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }
    }

    // In case mremap fails, we have still made the file larger and
    // need to signal this to other processes.
    shared_state->mmap_size = new_size;

    // Remap everything except the first page
    auto *new_base = (unsigned char *) mremap(
            base,
            mmap_size,
            new_size,
            MREMAP_MAYMOVE
    );

    if (new_base == MAP_FAILED) {
        shared_state->resize_rw_lock.unlock();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    mmap_size = new_size;
    base = new_base;
    current_base = new_base;

    shared_state->resize_rw_lock.unlock();
}

FFDB::FFDB(std::string filepath, int flags, int mode, Size init_size, std::optional<DBConf> conf):
pid(getpid()),
fd(-1),
mmap_size(0),
base(nullptr),
filepath(std::move(filepath)) {
    if (this->filepath.empty()) throw std::invalid_argument("filepath is empty");

    bool is_new = false;

    fd = open(this->filepath.c_str(), flags, mode);
    if (fd == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    mmap_size = lseek(fd, 0, SEEK_END);
    if (mmap_size == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // Initialize file if it was just created.
    if (mmap_size == 0) {
        if (conf) this->conf = conf.value();

        mmap_size = align_bytes(init_size, this->conf.get_sys_page_size());
        Size min_size = this->conf.get_sys_page_size() * 2;

        if (mmap_size < min_size) mmap_size = min_size; // Need one page for shared state and one for alloc page at min.

        if (ftruncate(fd, mmap_size) == -1) {
            cleanup();
            throw std::runtime_error("Errno: " + std::to_string(errno));
        }

        is_new = true;
    }

    shared_state = (SharedState *)
        mmap(nullptr, mmap_size, PROT_READ, MAP_SHARED, fd, 0);

    if (!is_new) this->conf = shared_state->conf;

    if (shared_state == MAP_FAILED) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    if (mprotect(shared_state, this->conf.get_sys_page_size(), PROT_READ | PROT_WRITE) == -1) {
        cleanup();
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    mmap_size -= this->conf.get_sys_page_size();
    base = ((unsigned char *) shared_state) + this->conf.get_sys_page_size();

    if (is_new) {
        shared_state->mmap_size = mmap_size;
        shared_state->conf = this->conf;

        shared_state->curr_txn_id = 1;
        shared_state->min_txn_id = 1; // TODO: Noone uses a revision yet. Should this be +inf?

        shared_state->write_lock.init();
        shared_state->rev_lock.init();
        shared_state->resize_rw_lock.init();

        shared_state->top = this->conf.get_small_align(); // 0 is null

        // The rest is zeroed by ftruncate
    }
}

Revision *FFDB::register_revision(Offset root, Id txn_id) {
    shared_state->rev_lock.lock();

    Revision *found_rev = nullptr;
    bool found = false;

    shared_state->min_txn_id = SIZE_MAX;
    for (size_t i = 0; i < shared_state->revision_count; i++) {
        Revision *curr_rev = &shared_state->revisions[i];

        if (curr_rev->ref_count == 0) {
            if (found_rev == nullptr) found_rev = curr_rev;
        }
        else {
            if (curr_rev->pid == pid &&
                curr_rev->root == root) {

                curr_rev->ref_count++;
                curr_rev->txn_id = txn_id;
                found_rev = curr_rev;
                found = true;
            }

            if (shared_state->min_txn_id > curr_rev->txn_id)
                shared_state->min_txn_id = curr_rev->txn_id;
        }
    }

    if (!found) {
        if (found_rev == nullptr) {
            // TODO: Am I sure about sys instead of log?
            size_t max_revisions = (conf.get_sys_page_size() - sizeof(SharedState)) / sizeof(Revision);
            if (shared_state->revision_count >= max_revisions) {
                shared_state->rev_lock.unlock();
                throw std::runtime_error("No more revisions available");
            }
            found_rev = &shared_state->revisions[shared_state->revision_count++];
        }

        found_rev->pid = pid;
        found_rev->root = root;
        found_rev->ref_count = 1;
        found_rev->txn_id = txn_id;

        if (shared_state->min_txn_id > txn_id)
            shared_state->min_txn_id = txn_id;
    }

    shared_state->rev_lock.unlock();

    return found_rev;
}

void FFDB::unregister_revision(Revision *rev) {
    shared_state->rev_lock.lock();

    rev->ref_count--;
    if (rev->ref_count == 0) {
        while (shared_state->revisions[shared_state->revision_count - 1].ref_count == 0) {
            shared_state->revision_count--;
            if (shared_state->revision_count == 0) break;
        }
    }

    shared_state->min_txn_id = SIZE_MAX;
    for (size_t i = 0; i < shared_state->revision_count; i++) {
        Revision *curr_rev = &shared_state->revisions[i];

        if (curr_rev->ref_count > 0 && shared_state->min_txn_id > curr_rev->txn_id)
            shared_state->min_txn_id = curr_rev->txn_id;
    }

    shared_state->rev_lock.unlock();
}

// TODO: Use these three functions to connect all code.

void FFDB::start_txn(DBMode mode) {
    db_mode = mode;
    lock();

    current_base = base;
    _current_db = this;

    if (db_mode == DB_WRITE_TXN) {
        if (mprotect(base, mmap_size, PROT_READ | PROT_WRITE) == -1)
            throw std::runtime_error("Errno: " + std::to_string(errno));

        writer_state.txn_root = shared_state->active_txn_root;
        writer_state.top = shared_state->top;
        writer_state.free_blocks = shared_state->free_blocks;
        writer_state.unused_descrs = shared_state->free_descrs;
        writer_state.pending_unused_descrs = 0;
        writer_state.pending_unused_last = 0;
        writer_state.lf_block.ofs = 0;
        writer_state.lf_block.size = 0;
        writer_state.lf_block.txn_id = 0;
    }
    else if (db_mode == DB_READ_TXN) {
        // Reg revision?
    }
}

void FFDB::rollback_txn() {
    if (mprotect(base, mmap_size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (db_mode == DB_READ_TXN) {
        // unreg revision?

        unlock();
        db_mode = DB_NO_TXN;
        return;
    }

    // shared_state->writer_pid = 0;

    _current_db = nullptr;

    unlock();
    db_mode = DB_NO_TXN;
}

void FFDB::commit_txn()  {
    if (db_mode == DB_NO_TXN)
        throw std::runtime_error("No transaction started!");

    if (db_mode == DB_READ_TXN) {
        // unreg revision?

        unlock();
        db_mode = DB_NO_TXN;
        return;
    }

    auto saved_root = shared_state->active_txn_root;
    auto saved_top = shared_state->top;
    auto saved_free_list = shared_state->free_blocks;
    auto saved_free_descriptors = shared_state->free_descrs;

    if (writer_state.pending_unused_last != 0) {
        writer_state.pending_unused_last->next_unused = writer_state.unused_descrs;
        writer_state.unused_descrs = writer_state.pending_unused_descrs;
    }

    if (msync(base, mmap_size, MS_SYNC | MS_INVALIDATE) != 0)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    shared_state->active_txn_root = writer_state.txn_root;
    shared_state->top = writer_state.top;
    shared_state->free_blocks = writer_state.free_blocks;
    shared_state->free_descrs = writer_state.unused_descrs;
    shared_state->curr_txn_id++;

    if (mprotect(base, mmap_size, PROT_READ) == -1)
        throw std::runtime_error("Errno: " + std::to_string(errno));

    if (msync(base, mmap_size, MS_SYNC | MS_INVALIDATE) != 0) {
        shared_state->active_txn_root = saved_root;
        shared_state->top = saved_top;
        shared_state->free_blocks = saved_free_list;
        shared_state->free_descrs = saved_free_descriptors;
        shared_state->curr_txn_id--;
        throw std::runtime_error("Errno: " + std::to_string(errno));
    }

    // state->writer_pid = 0;
    _current_db = nullptr;

    unlock();
    db_mode = DB_NO_TXN;
}

void FFDB::set_new_txn_root(Offset root) {
    writer_state.txn_root = root;
}