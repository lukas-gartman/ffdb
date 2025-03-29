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

struct MockBlockDescriptor {
    Object mbd1;
    Object mbd2;
    Ref<MockBlockDescriptor> mbd3;
};

/* MallocTestSeq
 * Tests if Allocator::malloc allocates data sequentially when no "holes" are
 * present in memory.
 */
TEST(AllocatorTest, MallocTestSeq) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));
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

    const std::size_t TARGET_SIZE = 16;
    unsigned char target[] = {
        0xAA, 0xBB, 0xBB, 0xBB, 0xBB, 0xCC, 0xCC, 0xDD,
        0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xEE
    };

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);
}

/* FreeTestSeq
 * Tests if Allocator::free successfully frees memory from the top when no
 * "holes" are present in memory.
 */
TEST(AllocatorTest, FreeTestSeq) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));
    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint32_t> d2 = alloc.malloc<uint32_t>();
    Ref<uint16_t> d3 = alloc.malloc<uint16_t>();

    *d1 = 0xFF;
    *d2 = 0xEEEEEEEE;
    *d3 = 0xDDDD;

    alloc.free<uint16_t>(d3);
    alloc.free<uint32_t>(d2);
    alloc.free<uint8_t>(d1);

    Ref<uint8_t> d4 = alloc.malloc<uint8_t>();
    Ref<uint32_t> d5 = alloc.malloc<uint32_t>();
    Ref<uint16_t> d6 = alloc.malloc<uint16_t>();

    *d4 = 0xAA;
    *d5 = 0xBBBBBBBB;
    *d6 = 0xCCCC;

    const size_t TARGET_SIZE = 7;
    unsigned char target[] = {
        0xAA, 0xBB, 0xBB, 0xBB, 0xBB, 0xCC, 0xCC
    };

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);
}

TEST(AllocatorTest, FreeTestMerge) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));
    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d2 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d3 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d4 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d5 = alloc.malloc<uint8_t>();

    *d1 = 0xFF;
    *d2 = 0xFF;
    *d3 = 0xFF;
    *d4 = 0xFF;
    *d5 = 0xEE;

    alloc.free<uint8_t>(d2);
    alloc.free<uint8_t>(d1);
    alloc.free<uint8_t>(d3);
    alloc.free<uint8_t>(d4);

    Ref<uint32_t> d6 = alloc.malloc<uint32_t>();
    *d6 = 0xDDCCBBAA;

    const size_t TARGET_SIZE = 5;
    unsigned char target[] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE
    };


    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);
}

TEST(AllocatorTest, BlockDescriptorTestNewTop) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));

    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    alloc.malloc<uint8_t>();
    Ref<uint8_t> d3 = alloc.malloc<uint8_t>();
    alloc.malloc<uint8_t>();

    alloc.free<uint8_t>(d3);
    alloc.free<uint8_t>(d1);

    MockBlockDescriptor mbd_target = {3, 1, 0};
    const std::size_t TARGET_MDB_INDEX = 4;

    auto mbd = *((MockBlockDescriptor *) &data[TARGET_MDB_INDEX]);
    ASSERT_EQ(mbd.mbd1, mbd_target.mbd1);
    ASSERT_EQ(mbd.mbd2, mbd_target.mbd2);
    ASSERT_EQ(mbd.mbd3, mbd_target.mbd3);
}

TEST(AllocatorTest, FreeTestOverwrite) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));

    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d2 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d3 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d4 = alloc.malloc<uint8_t>();

    *d1 = 0xFF;
    *d2 = 0xBB;
    *d3 = 0xFF;
    *d4 = 0xDD;

    alloc.free<uint8_t>(d3);
    alloc.free<uint8_t>(d1);

    Ref<uint8_t> d5 = alloc.malloc<uint8_t>(); // Overwrite last free block
    Ref<uint8_t> d6 = alloc.malloc<uint8_t>(); // Should empty block descriptor

    *d5 = 0xAA;
    *d6 = 0xCC;

    alloc.free<uint8_t>(d5);
    alloc.free<uint8_t>(d6);

    const size_t TARGET_SIZE = 4;
    unsigned char target[] = {
        0xAA, 0xBB, 0xCC, 0xDD
    };

    for (int i = 0; i < 40; i++) std::cout << std::hex << (unsigned int) data[i] << std::endl;

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);

    MockBlockDescriptor mbd_target = {1, 1, 0};

    // Block descriptor now points at itself which is wrong

    auto mbd = *((MockBlockDescriptor *) &data[TARGET_SIZE]);
    ASSERT_EQ(mbd.mbd1, mbd_target.mbd1);
    ASSERT_EQ(mbd.mbd2, mbd_target.mbd2);
    ASSERT_EQ(mbd.mbd3, mbd_target.mbd3);

}
