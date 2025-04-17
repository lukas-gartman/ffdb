#include <gtest/gtest.h>
#include <cstdint>
#include "../Allocator.h" // Should include typedefs.h
#include "../MemState.h"

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
 * When allocating memory when no segmentation is present, the memory should be
 * allocated at the top of the memory map.
 */

TEST(AllocatorTest, MallocTestSeq) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));

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
 * When freeing memory from the top, this should not result in fragmentation.
 */
TEST(AllocatorTest, FreeTestSeq) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));
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

/* FreeTestMerge
 * When freeing memory that is not on the top, if the last free memory block is
 * adjacent to the freed memory block, the two should be merged into one larger block.
 */
TEST(AllocatorTest, FreeTestMerge) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));
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

/* BlockDescriptorTestNewTop
 * When freeing memory that is not on the top, if the last free memory block is
 * adjacent to the freed memory block, the two should be merged into one larger block.
 */
TEST(AllocatorTest, BlockDescriptorTestNewTop) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));

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

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);
    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));

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

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);

    MockBlockDescriptor mbd_target = {1, 1, 0};

    auto mbd = *((MockBlockDescriptor *) &data[TARGET_SIZE]);
    ASSERT_EQ(mbd.mbd1, mbd_target.mbd1);
    ASSERT_EQ(mbd.mbd2, mbd_target.mbd2);
    ASSERT_EQ(mbd.mbd3, mbd_target.mbd3);
}

TEST(AllocatorTest, FreeTestOverwriteHalfBD) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);

    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));

    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint16_t> d2 = alloc.malloc<uint16_t>();
    Ref<uint8_t> d3 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d4 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d5 = alloc.malloc<uint8_t>();

    *d1 = 0xAA;
    *d2 = 0xFFFF;
    *d3 = 0xDD;
    *d4 = 0xFF;
    *d5 = 0xFF;

    // Last free block is now d2, 16 bytes
    alloc.free<uint16_t>(d2);

    // Last free block is now d4, 8 bytes. Insert BD for d2.
    alloc.free<uint8_t>(d4);

    // Allocate 8 bytes in last free block, this will set last free block to 0.
    Ref<uint8_t> d6 = alloc.malloc<uint8_t>();

    // Allocate 8 bytes, this should overwrite half the block descriptor for d2.
    // Since last free block is 0, it will not be inserted as a block descriptor.
    // The remaining 8 bytes will be pointed to by last free block.
    Ref<uint8_t> d7 = alloc.malloc<uint8_t>();

    // Last free block which now points to the second half of d2, will be
    // overwritten.
    Ref<uint8_t> d8 = alloc.malloc<uint8_t>();

    *d6 = 0xEE;
    *d7 = 0xBB;
    *d8 = 0xCC;

    const size_t TARGET_SIZE = 6;
    unsigned char target[] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);

    MockBlockDescriptor mbd_target = {3, 1, 0};

    auto mbd = *((MockBlockDescriptor *) &data[TARGET_SIZE]);
    ASSERT_EQ(mbd.mbd1, mbd_target.mbd1);
    ASSERT_EQ(mbd.mbd2, mbd_target.mbd2);
    ASSERT_EQ(mbd.mbd3, mbd_target.mbd3);
}

TEST(AllocatorTest, FreeTestOverwriteHalfBestFit) {
    MemoryMapState mm_state(1024);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (MemState *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(MemState);

    unsigned char *data = (Ref<unsigned char>) alloc_state->top;

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(MemState));

    Ref<uint8_t> d1 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d2 = alloc.malloc<uint8_t>();
    Ref<uint8_t> d3 = alloc.malloc<uint8_t>();
    Ref<uint32_t> d4 = alloc.malloc<uint32_t>();
    Ref<uint8_t> d5 = alloc.malloc<uint8_t>();
    Ref<uint16_t> d6 = alloc.malloc<uint16_t>();
    Ref<uint8_t> d7 = alloc.malloc<uint8_t>();

    *d1 = 0xFF;
    *d2 = 0xFF;
    *d3 = 0xFF;
    *d4 = 0xFFFFFFFF;
    *d5 = 0xFF;
    *d6 = 0xFFFF;
    *d7 = 0xFF;

    alloc.free<uint8_t>(d1);
    alloc.free<uint32_t>(d4);
    alloc.free<uint16_t>(d6);
    alloc.free<uint8_t>(d3);
    alloc.malloc<uint8_t>(); // Pad

    Ref<uint32_t> d8 = alloc.malloc<uint32_t>();
    Ref<uint16_t> d9 = alloc.malloc<uint16_t>();
    Ref<uint8_t> d10 = alloc.malloc<uint8_t>();

    *d8 = 0xBBBBBBBB;
    *d9 = 0xCCCC;
    *d10 = 0xAA;

    const size_t TARGET_SIZE = 11;
    unsigned char target[] = {
        0xAA, 0xFF, 0xFF, 0xBB, 0xBB, 0xBB, 0xBB, 0xFF, 0xCC, 0xCC, 0xFF
    };

    for (int i = 0; i < TARGET_SIZE; i++) ASSERT_EQ(data[i], target[i]);

    MockBlockDescriptor mbd_target1 = {2, 0, 0x3C};
    MockBlockDescriptor mbd_target2 = {8, 0, 0};
    MockBlockDescriptor mbd_target3 = {0xB, 0, 0x24};

    auto mbd1 = *((MockBlockDescriptor *) &data[TARGET_SIZE]);
    auto mbd2 = *((MockBlockDescriptor *) &data[TARGET_SIZE + sizeof(MockBlockDescriptor)]);
    auto mbd3 = *((MockBlockDescriptor *) &data[TARGET_SIZE + 2 * sizeof(MockBlockDescriptor)]);

    ASSERT_EQ(mbd1.mbd1, mbd_target1.mbd1);
    ASSERT_EQ(mbd1.mbd2, mbd_target1.mbd2);
    ASSERT_EQ(mbd1.mbd3, mbd_target1.mbd3);

    ASSERT_EQ(mbd2.mbd1, mbd_target2.mbd1);
    ASSERT_EQ(mbd2.mbd2, mbd_target2.mbd2);
    ASSERT_EQ(mbd2.mbd3, mbd_target2.mbd3);

    ASSERT_EQ(mbd3.mbd1, mbd_target3.mbd1);
    ASSERT_EQ(mbd3.mbd2, mbd_target3.mbd2);
    ASSERT_EQ(mbd3.mbd3, mbd_target3.mbd3);
}