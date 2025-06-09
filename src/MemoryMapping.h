#ifndef MEMORYMAPPING_H
#define MEMORYMAPPING_H

#include "MemState.h"
#include "WriterState.h"

class MemoryMapping {
private:
    int fd;
    size_t size;
    unsigned char *base;
    std::string filepath;
    pid_t pid; // Store so we won't have to call getpid() which is a syscall every time.

    static size_t calc_mem_map_size(size_t init_size);

    void cleanup() noexcept;

public:
    constexpr static size_t PAGE_SIZE = 4096;
    constexpr static size_t MIN_PAGE_SIZE = PAGE_SIZE * 2;

    // TODO: Make read only outside of class.
    MemState *shared_state;

    MemoryMapping(
        std::string filepath,
        int flags,
        int mode,
        size_t init_size
    );

    ~MemoryMapping();

    Revision *register_revision(Object root, size_t txn_id);
    void unregister_revision(Revision *rev);

    void init_wr_txn(WriterState *ws);
    void cancel_wr_txn();
    void apply_wr_txn(WriterState *ws);

    void resize_map();

    size_t get_size() const noexcept {
        return size;
    }

    unsigned char *get_base() const noexcept {
        return base;
    }
};

#endif //MEMORYMAPPING_H