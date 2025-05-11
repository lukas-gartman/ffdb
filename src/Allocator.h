#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "Typedefs.h"
#include "MemoryMapping.h"
#include "Transaction.h"

#include <stdexcept>

class Allocator {
public:
    Allocator(MemoryMapping *mm);

    template <class A> Ref<A> malloc(const std::size_t extra_bytes = 0) {
        return malloc_internal(sizeof(A) + extra_bytes);
    }

    template<class A> void free(Ref<A> obj, const std::size_t extra_bytes = 0) {
        free_internal(obj.as_object(), sizeof(A) + extra_bytes);
    }

    void start_transaction();
    void rollback_transaction();
    void commit_transaction();

    void dump_block_descrs();

private:
    MemoryMapping *mmap;
    Transaction *curr_txn;

    Object malloc_internal(std::size_t bytes);
    void free_internal(Object obj, std::size_t bytes);

    Ref<BlockDescr> new_block_descr(Block block);
    Ref<BlockDescr> upd_block_descr(Ref<BlockDescr> descr);
    void insert_block_descr(Block block);
    Object delete_block_descr(std::size_t block_size);
};

#endif //ALLOCATOR_H
