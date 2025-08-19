#include <gtest/gtest.h>
#include <random>
#include <fcntl.h>
#include <vector>
#include "../src/FFDB.h"

/* TestAllocFreeTop
 * Allocates one page full of uint64_t values, then frees them all. It then allocates another page of memory. What
 * should happen then is that the allocator should reuse the freed memory and hence, values2 should point to exactly
 * the same memory as values1.
 */
TEST(AllocatorTest, TestAllocFreeTop) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_txn(DB_WRITE_TXN);

    // Calculate how many uint64_t values can fit into a single page and compensate for alignment.
    const size_t elem_size = sizeof(uint64_t) <= SMALL_ALIGNMENT ? SMALL_ALIGNMENT : sizeof(uint64_t);
    const size_t data_size = (SYSTEM_PAGE_SIZE / elem_size - 1) - SMALL_ALIGNMENT;
    Ref<uint64_t> values1[data_size];

    // Check if the memory contains the correct values.
    for (size_t i = 0; i < data_size; i++) {
        values1[i] = db.malloc<uint64_t>();
        *values1[i] = i;
    }

    for (size_t i = 0; i < data_size; i++) EXPECT_EQ(*values1[i], i);

    for (size_t i = data_size; i-- > 0;) db.free<uint64_t>(values1[i]);

    // Memory should now be freed but the data remain. Allocate a new set of ints and check that we get the same
    // addresses as before
    Ref<uint64_t> values2[data_size];

    for (size_t i = 0; i < data_size; i++) values2[i] = db.malloc<uint64_t>();

    for (size_t i = 0; i < data_size; i++) EXPECT_EQ(values1[i], values2[i]);

    db.commit_txn();

    remove("test.db");
}

/* TestArrayNotation
 * Allocates arrays of different types using the array notation and verifies that the contents are correct.
 */
TEST(AllocatorTest, TestArrayNotation) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_txn(DB_WRITE_TXN);

    const size_t arr1_size = SYSTEM_PAGE_SIZE / 4 - SMALL_ALIGNMENT; // Compensate for null ref
    const size_t arr2_size = (SYSTEM_PAGE_SIZE / 4) / sizeof(uint16_t);
    const size_t arr3_size = (SYSTEM_PAGE_SIZE / 2) / sizeof(uint32_t);

    // Fill 4 byte arrays of different sizes and verify their contents are correct.
    const Ref<uint8_t> arr1 = db.malloc<uint8_t>(arr1_size);
    const Ref<uint16_t> arr2 = db.malloc<uint16_t>(arr2_size);
    const Ref<uint32_t> arr3 = db.malloc<uint32_t>(arr3_size);

    for (size_t i = 0; i < arr1_size; ++i) arr1[i] = 0xAB;
    for (size_t i = 0; i < arr2_size; ++i) arr2[i] = 0xCDCD;
    for (size_t i = 0; i < arr3_size; ++i) arr3[i] = 0xEFEFEFEF;

    db.free<uint8_t>(arr1, arr1_size);
    db.free<uint16_t>(arr2, arr2_size);
    db.free<uint32_t>(arr3, arr3_size);

    for (size_t i = 0; i < arr1_size; ++i) EXPECT_EQ(arr1[i], 0xAB);
    for (size_t i = 0; i < arr2_size; ++i) EXPECT_EQ(arr2[i], 0xCDCD);
    for (size_t i = 0; i < arr3_size; ++i) EXPECT_EQ(arr3[i], 0xEFEFEFEF);

    db.commit_txn();

    remove("test.db");
}

/* TestFreeLFBlock
 * Tests the three different ways last free block can be merged. That is: merge up, merge down and merge with top.
 */
TEST(AllocatorTest, TestFreeLFBlock) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_txn(DB_WRITE_TXN);

    const size_t arr1_size = SMALL_ALIGNMENT;
    const size_t arr2_size = SMALL_ALIGNMENT * 3;
    const size_t arr3_size = SMALL_ALIGNMENT * 2;
    const size_t arr4_size = SMALL_ALIGNMENT;
    const size_t arr5_size = SMALL_ALIGNMENT * 6;
    const size_t arr6_size = SMALL_ALIGNMENT * 7;

    const Ref<uint8_t> arr1 = db.malloc<uint8_t>(arr1_size);
    const Ref<uint8_t> arr2 = db.malloc<uint8_t>(arr2_size);
    const Ref<uint8_t> arr3 = db.malloc<uint8_t>(arr3_size);
    const Ref<uint8_t> arr4 = db.malloc<uint8_t>(arr4_size);

    // Trigger last free block merge.
    db.free<uint8_t>(arr2, arr2_size);
    db.free<uint8_t>(arr1, arr1_size); // Merge up
    db.free<uint8_t>(arr3, arr3_size); // Merge down

    const Ref<uint8_t> arr5 = db.malloc<uint8_t>(arr5_size); // If the blocks was merged correctly, this block
                                                                    // will fill the space of the three first arrays.

    EXPECT_EQ(arr1, arr5); // Should have the same address as arr1

    // Merge top
    db.free<uint8_t>(arr5, arr5_size);
    db.free<uint8_t>(arr4, arr4_size);

    const Ref<uint8_t> arr6 = db.malloc<uint8_t>(arr6_size);

    EXPECT_EQ(arr1, arr6); // Same as above. If the allocator works correctly, arr6 should be at the same
                           // address as arr1 and arr5.

    db.commit_txn();

    remove("test.db");
}

// Test rollback: Do all kinds of operations and commit, do all kinds of operations and rollback. Verify that
// The first transaction is intact and the second transaction is rolled back.

// Test concurrent transactions: Need to test readonly transactions in different ways.

// Test block descr system in some way