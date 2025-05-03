#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/mman.h>

#include "../src/Allocator.h" // Should include typedefs.h
#include "../src/FFDB.h"

TEST(AllocatorTest, SimpleAllocTest) {
    constexpr int TEST_SIZE = 10;
    Ref<int> elems[TEST_SIZE];

    {
        FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);

        db.start_transaction();

        for (int i = 0; i < TEST_SIZE; ++i)
            elems[i] = db.alloc->malloc<int>();

        for (int i = 0; i < TEST_SIZE; ++i)
            *elems[i] = i;

        for (int i = 0; i < TEST_SIZE; ++i)
            EXPECT_EQ(*elems[i], i);

        db.commit_transaction();
    }

    remove("test.db");
}

TEST(AllocatorTest, MergeLfBlockTest) {
    Ref<int> i1;
    Ref<int> i2;
    Ref<int> i3;
    Ref<int> i4;

    {
        FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);

        db.start_transaction();

        i1 = db.alloc->malloc<int>();
        i2 = db.alloc->malloc<int>();
        i3 = db.alloc->malloc<int>();
        i4 = db.alloc->malloc<int>();

        db.alloc->free(i2);
        db.alloc->free(i3);
        db.alloc->free(i1);
        db.alloc->free(i4);

        db.commit_transaction();
    }

    remove("test.db");
}

TEST(AllocatorTest, TestTest) {
    Ref<int> i1;
    Ref<int> i2;
    Ref<int> i3;
    Ref<int> i4;
    Ref<int> i5;
    Ref<int> i6;

    {
        FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);

        std::cout << sizeof(BlockDescr) << std::endl;

        db.start_transaction();

        i1 = db.alloc->malloc<int>();
        i2 = db.alloc->malloc<int>();
        i3 = db.alloc->malloc<int>();
        i4 = db.alloc->malloc<int>();
        i5 = db.alloc->malloc<int>();
        i6 = db.alloc->malloc<int>();

        db.alloc->free(i1);
        db.alloc->free(i3);
        db.alloc->free(i5);

        db.commit_transaction();

        db.start_transaction();
        db.alloc->malloc<int>();
        db.alloc->malloc<int>();
        db.alloc->dump_block_descrs();
        db.commit_transaction();

        db.start_transaction();
        db.alloc->malloc<int>();
        db.alloc->dump_block_descrs();
        db.commit_transaction();

        db.start_transaction();
        db.alloc->dump_block_descrs();
        db.commit_transaction();
    }

    remove("test.db");
}