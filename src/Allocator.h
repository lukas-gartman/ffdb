#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <functional>

#include "Typedefs.h"
#include "WriterState.h"

class Allocator {
public:
    Allocator(std::function<size_t(size_t)> resize_fn)
        : curr_ws(nullptr), resize_callback(std::move(resize_fn)) {}

    template<class A> Ref<A> malloc(const size_t elem_amount = 1) {
        if (elem_amount == 0) return 0;
        return malloc_internal(align_bytes(sizeof(A) * elem_amount));
    }

    template<class A> void free(Ref<A> obj, const size_t elem_amount = 1) {
        if (elem_amount == 0) return;
        free_internal(obj.as_object(), align_bytes(sizeof(A) * elem_amount));
    }

    void activate(WriterState *ws);
    WriterState *deactivate();

    bool is_active() const {
        return curr_ws != nullptr;
    }

    void dump_block_descrs() const;

private:
    WriterState *curr_ws;

    static size_t align_bytes(size_t bytes);
    static size_t bytes_left_in_page(Object offset);

    Object malloc_internal(size_t bytes);
    void free_internal(Object obj, size_t bytes);

    Ref<BlockDescr> new_block_descr(Block block);
    Ref<BlockDescr> upd_block_descr(Ref<BlockDescr> descr);
    void insert_block_descr(Block block);
    Object delete_block_descr(size_t block_size);

    std::function<size_t(size_t)> resize_callback;
};

#endif //ALLOCATOR_H
