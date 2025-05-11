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