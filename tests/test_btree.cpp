#include <gtest/gtest.h>
#include "../BTree.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <unordered_set>

TEST(BTreeTest, InsertTest) {
    BTree<int, 3> tree;
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
    constexpr int numKeys = 5000000;
    BTree<int, 3> tree;
    for (int i = 1; i <= numKeys; ++i) {
        tree.insert(i);
    }

    for (int i = 1; i <= numKeys; ++i) {
        EXPECT_TRUE(tree.search(i));
    }
}

TEST(BTreeTest, RandomInsert) {
    constexpr int numKeys = 50;
    BTree<int, 3> tree;
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
    constexpr int numKeys = 1000000;
    BTree<int, 100> tree;
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

TEST(BTreeTest, RemoveTest) {
    BTree<int, 3> tree;
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
    constexpr int numKeys = 1000000;
    BTree<int, 3> tree;

    for (int i = numKeys; i >= numKeys; --i) {
        tree.insert(i);
    }

    for (int i = numKeys; i >= numKeys; --i) {
        tree.remove(i);
        EXPECT_FALSE(tree.search(i)) << "Failed for key: " << i;
    }
}

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
    constexpr int numKeys = 10000000;
    BTree<int, 3> tree;

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
    BTree<int, 3> tree;

    // Insertion order
    tree.insert(15);
    EXPECT_TRUE(tree.search(15));
    tree.insert(11);
    EXPECT_TRUE(tree.search(11));
    tree.insert(27);
    EXPECT_TRUE(tree.search(27));
    tree.insert(3);
    EXPECT_TRUE(tree.search(3));
    tree.insert(21);
    EXPECT_TRUE(tree.search(21));
    tree.insert(24);
    EXPECT_TRUE(tree.search(24));
    tree.insert(19);
    EXPECT_TRUE(tree.search(19));
    tree.insert(13);
    EXPECT_TRUE(tree.search(13));
    tree.insert(22);
    EXPECT_TRUE(tree.search(22));
    tree.insert(4);
    EXPECT_TRUE(tree.search(4));
    tree.insert(5);
    EXPECT_TRUE(tree.search(5));
    tree.insert(8);
    EXPECT_TRUE(tree.search(8));
    tree.insert(28);
    EXPECT_TRUE(tree.search(28));
    tree.insert(26);
    EXPECT_TRUE(tree.search(26));
    tree.insert(7);
    EXPECT_TRUE(tree.search(7));
    tree.insert(6);
    EXPECT_TRUE(tree.search(6));
    tree.insert(23);
    EXPECT_TRUE(tree.search(23));
    tree.insert(29);
    EXPECT_TRUE(tree.search(29));
    tree.insert(16);
    EXPECT_TRUE(tree.search(16));
    tree.insert(2);
    EXPECT_TRUE(tree.search(2));
    tree.insert(25);
    EXPECT_TRUE(tree.search(25));
    tree.insert(0);
    EXPECT_TRUE(tree.search(0));
    tree.insert(17);
    EXPECT_TRUE(tree.search(17));
    tree.insert(12);
    EXPECT_TRUE(tree.search(12));
    tree.insert(14);
    EXPECT_TRUE(tree.search(14));
    tree.insert(1);
    EXPECT_TRUE(tree.search(1));
    tree.insert(9);
    EXPECT_TRUE(tree.search(9));
    tree.insert(18);
    EXPECT_TRUE(tree.search(18));
    tree.insert(20);
    EXPECT_TRUE(tree.search(20));
    tree.insert(10);
    EXPECT_TRUE(tree.search(10));

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
    const int start = 44;
    const int numKeys = 50;
    std::vector<int> keys = generateRandomKeys(numKeys);
    for (int i = start; i < numKeys; ++i) {
        std::cout << "\niteration " << i << std::endl;
        BTree<int, 3> tree;
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
