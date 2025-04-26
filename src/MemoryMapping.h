#ifndef MEMORYMAPPING_H
#define MEMORYMAPPING_H

#include "MemState.h"
#include "Transaction.h"

class MemoryMapping {
private:
    int fd;
    size_t size;
    unsigned char *base;
    std::string filepath;

    static size_t calc_mem_map_size(size_t init_size);

    void cleanup() noexcept;

public:
    constexpr static size_t PAGE_SIZE = 4096;
    constexpr static size_t MIN_PAGE_SIZE = PAGE_SIZE * 2;

    // TODO: Make read only outside of class.
    MemState *state;

    MemoryMapping(
        std::string filepath,
        int flags,
        int mode,
        size_t init_size
    );

    ~MemoryMapping();

    void init_transaction(Transaction *txn);
    void commit_transaction(Transaction *txn);

    size_t get_size() const noexcept {
        return size;
    }
};

#endif //MEMORYMAPPING_H