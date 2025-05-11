#include <gtest/gtest.h>
#include <random>
#include <fcntl.h>
#include <climits>
#include <vector>
#include "../src/Allocator.h"
#include "../src/FFDB.h"

struct History {
    int type;
    int index;
};

TEST(AllocatorTest, RandomOperationsTest) {
    constexpr int NUM_OPERATIONS = 1000;
    constexpr int MAX_ALLOCATIONS = 100;

    std::vector<Ref<int>> allocations_int(MAX_ALLOCATIONS / 2, 0);
    std::vector<int> ctrl_vals_int(MAX_ALLOCATIONS / 2, 0);
    std::vector<Ref<char>> allocations_char(MAX_ALLOCATIONS / 2, 0);
    std::vector<char> ctrl_vals_char(MAX_ALLOCATIONS / 2, 0);
    std::vector<double> weights = {4, 4, 2, 1};

    std::mt19937 rng(std::random_device{}());
    std::discrete_distribution<int> op_dist(weights.begin(), weights.end());
    std::uniform_int_distribution<int> type_dist(0, 1);
    std::uniform_int_distribution<int> index_dist(0, MAX_ALLOCATIONS / 2 - 1);
    std::uniform_int_distribution<int> value_dist_int(1, INT_MAX);
    std::uniform_int_distribution<char> value_dist_char(1, CHAR_MAX);

    std::vector<History> history;

    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    std::cout << "Starting new transaction..." << std::endl;
    db.start_transaction();

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        int op = op_dist(rng);
        int type = type_dist(rng);
        int index = index_dist(rng);

        if (op == 0) { // Allocate and write
            if (type == 0 && allocations_int[index] == 0) {
                std::cout << "Allocating int... Storing at " << index << std::endl;
                int value = value_dist_int(rng);
                allocations_int[index] = db.alloc->malloc<int>();
                *allocations_int[index] = value;
                ctrl_vals_int[index] = value;
            }
            else if (type == 1 && allocations_char[index] == 0) {
                std::cout << "Allocating char... Storing at " << index << std::endl;
                char value = value_dist_char(rng);
                allocations_char[index] = db.alloc->malloc<char>();
                *allocations_char[index] = value;
                ctrl_vals_char[index] = value;
            }
            history.push_back({type, index});
        } else if (op == 1) { // Free
            if (type == 0 && allocations_int[index] != 0) {
                std::cout << "Freeing int at index " << index << std::endl;
                db.alloc->free<int>(allocations_int[index]);
                allocations_int[index] = 0;
                ctrl_vals_int[index] = 0;
            } else if (type == 1 && allocations_char[index] != 0) {
                std::cout << "Freeing char at index " << index << std::endl;
                db.alloc->free<char>(allocations_char[index]);
                allocations_char[index] = 0;
                ctrl_vals_char[index] = 0;
            }
        } else if (op == 2) { // Commit/Start new transaction
            std::cout << "Committing transaction..." << std::endl;
            db.commit_transaction();
            std::cout << "Starting new transaction..." << std::endl;
            db.start_transaction();

            history.clear();
        } else if (op == 3) { // Rollback
            std::cout << "Rolling back transaction..." << std::endl;
            db.rollback_transaction();

            while (!history.empty()) {
                auto [t, indx] = history.back();
                if (t == 0) {
                    allocations_int[indx] = 0;
                    ctrl_vals_int[indx] = 0;
                } else if (t == 1) {
                    allocations_char[indx] = 0;
                    ctrl_vals_char[indx] = 0;
                }
                history.pop_back();
            }

            std::cout << "Starting new transaction..." << std::endl;
            db.start_transaction();
        }

        // Verify
        for (int j = 0; j < MAX_ALLOCATIONS / 2; ++j) {
            if (allocations_int[j] != 0) {
                EXPECT_EQ(*allocations_int[j], ctrl_vals_int[j]);
            }
            else if (allocations_char[j] != 0) {
                EXPECT_EQ(*allocations_char[j], ctrl_vals_char[j]);
            }
        }
    }
    std::cout << "Committing transaction..." << std::endl;
    db.commit_transaction();

    remove("test.db");
}

// Tests allocation on the top.
TEST(AllocatorTest, TestAllocFreeTop) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_transaction();
    auto a1 = db.alloc->malloc<uint64_t>();
    auto b1 = db.alloc->malloc<uint32_t>();
    auto c1 = db.alloc->malloc<uint16_t>();
    auto d1 = db.alloc->malloc<uint8_t>();

    *a1 = 0xAAAAAAAAAAAAAAAA;
    *b1 = 0xBBBBBBBB;
    *c1 = 0xCCCC;
    *d1 = 0xDD;

    EXPECT_EQ(*a1, 0xAAAAAAAAAAAAAAAA);
    EXPECT_EQ(*b1, 0xBBBBBBBB);
    EXPECT_EQ(*c1, 0xCCCC);
    EXPECT_EQ(*d1, 0xDD);

    db.alloc->free<uint8_t>(d1);
    db.alloc->free<uint16_t>(c1);
    db.alloc->free<uint32_t>(b1);
    db.alloc->free<uint64_t>(a1);

    auto a2 = db.alloc->malloc<uint8_t>();
    auto b2 = db.alloc->malloc<uint64_t>();
    auto c2 = db.alloc->malloc<uint16_t>();
    auto d2 = db.alloc->malloc<uint32_t>();

    *a2 = 0xAA;
    *b2 = 0xBBBBBBBBBBBBBBBB;
    *c2 = 0xCCCC;
    *d2 = 0xDDDDDDDD;

    EXPECT_EQ(*a1, 0xBBBBBBBBBBBBBBAA);
    EXPECT_EQ(*b1, 0xDDCCCCBB);
    EXPECT_EQ(*c1, 0xDDDD);
    EXPECT_EQ(*d1, 0xDD);

    EXPECT_EQ(*a2, 0xAA);
    EXPECT_EQ(*b2, 0xBBBBBBBBBBBBBBBB);
    EXPECT_EQ(*c2, 0xCCCC);
    EXPECT_EQ(*d2, 0xDDDDDDDD);

    db.commit_transaction();

    remove("test.db");
}

TEST(AllocatorTest, TestFreeMergeLFBlock) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_transaction();
    auto a = db.alloc->malloc<uint8_t>();
    auto b = db.alloc->malloc<uint8_t>();
    auto c = db.alloc->malloc<uint8_t>();
    auto d = db.alloc->malloc<uint8_t>();

    *a = 0xAA;
    *b = 0xBB;
    *c = 0xCC;
    *d = 0xDD;

    EXPECT_EQ(*a, 0xAA);
    EXPECT_EQ(*b, 0xBB);
    EXPECT_EQ(*c, 0xCC);
    EXPECT_EQ(*d, 0xDD);

    db.alloc->free<uint8_t>(b);
    db.alloc->free<uint8_t>(c); // Merge down
    db.alloc->free<uint8_t>(a); // Merge up

    // Allocate exactly the freed space and rearrange bytes
    auto abc = db.alloc->malloc<uint8_t>(2); // uint8_t[2]
    abc[0] = 0xBB;
    abc[1] = 0xCC;
    abc[2] = 0xAA;

    EXPECT_EQ(*a, abc[0]);
    EXPECT_EQ(*b, abc[1]);
    EXPECT_EQ(*c, abc[2]);
    EXPECT_EQ(*d, 0xDD);

    db.alloc->free<uint8_t>(abc, 2);
    db.alloc->free<uint8_t>(d); // Merge top

    auto abcd = db.alloc->malloc<uint32_t>();
    *abcd = 0xAABBCCDD;
    EXPECT_EQ(*a, 0xDD);
    EXPECT_EQ(*b, 0xCC);
    EXPECT_EQ(*c, 0xBB);
    EXPECT_EQ(*d, 0xAA);
    EXPECT_EQ(*abcd, 0xAABBCCDD);

    db.commit_transaction();

    remove("test.db");
}

TEST(AllocatorTest, TestResize) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_transaction();

    EXPECT_EQ(db.mem_mapping->shared_state->mmap_size, 4096);

    for (int i = 0; i < 4096/sizeof(uint64_t) - 1; ++i) {
        db.alloc->malloc<uint64_t>();
    }
    EXPECT_EQ(db.mem_mapping->shared_state->mmap_size, 4096);

    db.commit_transaction();

    EXPECT_EQ(db.mem_mapping->shared_state->top, 4096 - sizeof(uint64_t) + 1); // Alloc starts at 1

    db.start_transaction();

    db.alloc->malloc<uint64_t>();
    EXPECT_GT(db.mem_mapping->shared_state->mmap_size, 4096); // Size only increases if db file was resized.

    db.commit_transaction();

    remove("test.db");
}

// Tests if top is correctly reset after rollback.
TEST(AllocatorTest, TestRollbackTop) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);

    db.start_transaction();
    auto a = db.alloc->malloc<int>();
    auto b = db.alloc->malloc<int>();
    *a = 5;
    *b = 10;
    db.rollback_transaction();

    db.start_transaction();
    EXPECT_EQ(*a, 5);
    EXPECT_EQ(*b, 10);
    a = db.alloc->malloc<int>();
    b = db.alloc->malloc<int>();
    *a = 15;
    *b = 20;
    EXPECT_EQ(*a, 15);
    EXPECT_EQ(*b, 20);
    db.commit_transaction();

    remove("test.db");
}