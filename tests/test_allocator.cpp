#include <gtest/gtest.h>
#include <cstdint>
#include "../allocator.h" // Should include typedefs.h

static const std::size_t PAGE_SIZE = 4096;

class MemoryMapState {
public:
    void *memory_map;
    std::size_t memory_map_size;

    MemoryMapState(std::size_t memory_map_size):
        memory_map_size(memory_map_size) {

        memory_map = new char[memory_map_size];
    }

    ~MemoryMapState() {
        delete[] (char*) memory_map;
    }
};

/* MallocTestSeq
 * Tests if Allocator::malloc allocates data sequentially when no "holes" are
 * present in memory.
 */
TEST(AllocatorTest, MallocTestSeq) {
    MemoryMapState mm_state(
        sizeof(Allocator::State) + 16
    );

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 0;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));
    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint32_t> d2 = alloc.malloc<uint32_t>();
    Ref<uint16_t> d3 = alloc.malloc<uint16_t>();
    Ref<uint64_t> d4 = alloc.malloc<uint64_t>();
    Ref<uint8_t> d5 = alloc.malloc<uint8_t>();

    *d1 = 0xAA;
    *d2 = 0xBBBBBBBB;
    *d3 = 0xCCCC;
    *d4 = 0xDDDDDDDDDDDDDDDD;
    *d5 = 0xEE;

    unsigned char target[] = {
        0xAA, 0xBB, 0xBB, 0xBB, 0xBB, 0xCC, 0xCC, 0xDD,
        0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xEE
    };

    const size_t DATA_SIZE =
        mm_state.memory_map_size - sizeof(Allocator::State);

    for (int i = 0; i < DATA_SIZE; i++) {
        ASSERT_EQ(data[i], target[i]);
    }
}