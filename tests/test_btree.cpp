#include <gtest/gtest.h>
#include "../src/BTree.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <unordered_set>

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

TEST(BTreeTest, InsertTest) {
    MemoryMapState mm_state(7*PAGE_SIZE);

    current_base = (unsigned char *) mm_state.memory_map;

    auto *alloc_state = (Allocator::State *) current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state,
        mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);
    tree.insert(10);
    tree.insert(20);
    tree.insert(5);
    tree.insert(6);
    tree.insert(12);
    tree.insert(30);
    tree.insert(7);
    tree.insert(17);

    EXPECT_TRUE(tree.search(10));
    EXPECT_TRUE(tree.search(20));
    EXPECT_TRUE(tree.search(5));
    EXPECT_TRUE(tree.search(6));
    EXPECT_TRUE(tree.search(12));
    EXPECT_TRUE(tree.search(30));
    EXPECT_TRUE(tree.search(7));
    EXPECT_TRUE(tree.search(17));
    EXPECT_FALSE(tree.search(15));
}

TEST(BTreeTest, SequentialInsertTest) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    constexpr int numKeys = 5000000;
    BTree::BTree<int, 3> tree(&alloc);
    for (int i = 1; i <= numKeys; ++i) {
        tree.insert(i);
    }

    for (int i = 1; i <= numKeys; ++i) {
        EXPECT_TRUE(tree.search(i));
    }
}

TEST(BTreeTest, RandomInsert) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    constexpr int numKeys = 50;
    BTree::BTree<int, 3> tree(&alloc);
    std::srand(std::time(nullptr));
    std::set<int> keys;

    while (keys.size() < numKeys) {
        int key = std::rand() % numKeys * 5 + 1;
        if (keys.insert(key).second) {
            tree.insert(key);
        }
    }

    for (int key : keys) {
        EXPECT_TRUE(tree.search(key));
    }
}

TEST(BTreeTest, DuplicateTest) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    constexpr int numKeys = 1'000'000;
    BTree::BTree<int, 100> tree(&alloc);
    std::srand(std::time(nullptr));
    std::set<int> keys;

    while (keys.size() < numKeys) {
        int key = std::rand() % numKeys * 5 + 1;
        if (keys.insert(key).second) {
            tree.insert(key);
        }
    }

    std::set<int> uniqueKeys;
    bool hasDuplicate = false;

    tree.traverse([&uniqueKeys, &hasDuplicate](const int key) {
        if (uniqueKeys.contains(key)) {
            std::cout << "Duplicate key found: " << key << std::endl;
            hasDuplicate = true;
        } else {
            uniqueKeys.insert(key);
        }
    });

    EXPECT_FALSE(hasDuplicate);
}

TEST(BTreeTest, ManualDuplicateTest) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    const int keys[] = {1, 2, 3, 3, 4};
    BTree::BTree<int, 3> tree(&alloc);
    for (int key : keys) {
        tree.insert(key);
    }

    tree.remove(3);
    EXPECT_FALSE(tree.search(3));
}

TEST(BTreeTest, RemoveTest) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);
    tree.insert(10);
    tree.insert(20);
    tree.insert(5);
    tree.insert(6);
    tree.insert(12);
    tree.insert(30);
    tree.insert(7);
    tree.insert(17);

    tree.remove(6);
    tree.remove(13);
    tree.remove(7);
    tree.remove(4);
    tree.remove(2);
    tree.remove(16);

    EXPECT_FALSE(tree.search(6));
    EXPECT_FALSE(tree.search(13));
    EXPECT_FALSE(tree.search(7));
    EXPECT_FALSE(tree.search(4));
    EXPECT_FALSE(tree.search(2));
    EXPECT_FALSE(tree.search(16));
    EXPECT_TRUE(tree.search(10));
    EXPECT_TRUE(tree.search(20));
    EXPECT_TRUE(tree.search(5));
    EXPECT_TRUE(tree.search(12));
    EXPECT_TRUE(tree.search(30));
    EXPECT_TRUE(tree.search(17));
}

TEST(BTreeTest, SequentialInsertRemove) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);

    constexpr int numKeys = 1000000;
    for (int i = numKeys; i >= numKeys; --i) {
        tree.insert(i);
    }

    for (int i = numKeys; i >= numKeys; --i) {
        tree.remove(i);
        EXPECT_FALSE(tree.search(i)) << "Failed for key: " << i;
    }
}

// NOTE: It seems that this could generate duplicate keys which could
// mess upp some of the tests
std::vector<int> generateRandomKeys(int numKeys) {
    std::srand(std::time(nullptr));
    std::vector<int> keys;
    std::unordered_set<int> keySet;

    while (keySet.size() < numKeys) {
        int key = std::rand() % numKeys;
        if (keySet.insert(key).second) {
            keys.push_back(key);
        }
    }

    return keys;
}

TEST(BTreeTest, RandomInsertRemove) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);

    constexpr int numKeys = 1'000'000;
    std::cout << "Generating random keys..." << std::endl;
    std::vector<int> keys = generateRandomKeys(numKeys);
    for (const int key : keys) {
        tree.insert(key);
        EXPECT_TRUE(tree.search(key)) << "Insert failed for key: " << key;
    }

    std::cout << "Shuffling keys for random removal..." << std::endl;

    // Shuffle the keys for random removal
    auto rng = std::default_random_engine {};
    std::ranges::shuffle(keys, rng);

    std::cout << "Removing keys..." << std::endl;
    for (const int key : keys) {
        tree.remove(key);
        EXPECT_FALSE(tree.search(key)) << "Failed for key: " << key;;
    }

    for (const int key : keys) {
        tree.insert(key);
        EXPECT_TRUE(tree.search(key)) << "Insert failed for key: " << key;
    }
}

TEST(BTreeTest, RootNodeDelete) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);

    // Insertion order
    std::vector insertKeys = {15, 11, 27, 3, 21, 24, 19, 13, 22, 4, 5, 8, 28, 26, 7,
                              6, 23, 29, 16, 2, 25, 0, 17, 12, 14, 1, 9, 18, 20, 10};
    for (int key : insertKeys) {
        tree.insert(key);
        EXPECT_TRUE(tree.search(key));
    }

    tree.print();

    // Deletion order
    tree.remove(9);
    EXPECT_FALSE(tree.search(9));
    tree.remove(20);
    EXPECT_FALSE(tree.search(20));
    tree.remove(6);
    EXPECT_FALSE(tree.search(6));
    tree.remove(19);
    EXPECT_FALSE(tree.search(19));
    tree.remove(8);
    EXPECT_FALSE(tree.search(8));
    tree.remove(15);
    EXPECT_FALSE(tree.search(15));
    std::cout << "BEFORE ----------------" << std::endl;
    tree.print();

    tree.remove(21);

    std::cout << "AFTER ----------------" << std::endl;
    tree.print();
    EXPECT_FALSE(tree.search(21));
}

TEST(BTreeTest, IncrementalInsertDelete) {
    MemoryMapState mm_state(7 * PAGE_SIZE);

    current_base = (unsigned char *)mm_state.memory_map;

    auto *alloc_state = (Allocator::State *)current_base;
    alloc_state->file_size = mm_state.memory_map_size;
    alloc_state->top = 1;
    alloc_state->free_list = 0;
    alloc_state->free_descriptors = 0;

    current_base += sizeof(Allocator::State);

    Allocator alloc(alloc_state, mm_state.memory_map_size - sizeof(Allocator::State));

    BTree::BTree<int, 3> tree(&alloc);

    const int start = 44;
    const int numKeys = 50;
    std::vector<int> keys = generateRandomKeys(numKeys);
    for (int i = start; i < numKeys; ++i) {
        std::cout << "\niteration " << i << std::endl;
        bool testFailed = false;
        int failedKey = -1;
        std::string failedOperation;

        // Insert all keys
        for (int j = 0; j < i; ++j) {
            int key = keys[j];
            std::cout << "insert " << key << ", (j=" << j << ")" << std::endl;
            tree.insert(key);
            if (!tree.search(key)) {
                testFailed = true;
                failedKey = key;
                failedOperation = "insert";
                break;
            }
        }

        if (testFailed) {
            FAIL() << "Test failed at iteration " << i
                   << " during " << failedOperation
                   << " for key: " << failedKey;
            break;
        }

        // Remove all keys
        for (int j = 0; j < i; ++j) {
            int key = keys[j];
            std::cout << "remove " << key << ", (j=" << j << ")" << std::endl;
            tree.remove(key);
            if (tree.search(key)) {
                testFailed = true;
                failedKey = key;
                failedOperation = "delete";

                // tree.remove(key);
                // if (tree.search(key)) {
                //     std::cout << "it failed again" << std::endl;
                // }

                break;
            }
        }

        if (testFailed) {
            FAIL() << "Test failed at iteration " << i
                   << " during " << failedOperation
                   << " for key: " << failedKey;
            break;
        }

        // Re-insert to verify
        for (int j = 0; j < i; ++j) {
            int key = keys[j];
            std::cout << "re-insert " << key << ", (j=" << j << ")" << std::endl;
            tree.insert(key);
            if (!tree.search(key)) {
                testFailed = true;
                failedKey = key;
                failedOperation = "re-insert";
                break;
            }
        }

        if (testFailed) {
            FAIL() << "Test failed at iteration " << i
                   << " during " << failedOperation
                   << " for key: " << failedKey;
            break;
        }
    }
}
