#include <gtest/gtest.h>
#include "../src/BTree.h"
#include "../src/FFDB2.h"
#include "../src/FFDBKey.h"
#include <fcntl.h>

#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <unordered_set>

TEST(BTreeTest, InsertTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);
    BTree::BTree<int, float, 3> tree;
    tree.insert(FFDBKey<int>(10), 1.0f);
    tree.insert(FFDBKey<int>(20), 2.0f);
    tree.insert(FFDBKey<int>(5),  3.0f);
    tree.insert(FFDBKey<int>(5),  300.0f);
    tree.insert(FFDBKey<int>(5),  300000.0f);
    auto new_tree = tree.insert(FFDBKey<int>(6), 4.0f);
    EXPECT_TRUE(tree.search(FFDBKey<int>(10)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(20)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(5)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(6)));

    db.commit_txn();
    db.start_txn(DB_WRITE_TXN);

    tree.insert(FFDBKey<int>(12), 5.0f);
    tree.insert(FFDBKey<int>(30), 6.0f);
    tree.insert(FFDBKey<int>(7),  7.0f);
    tree.insert(FFDBKey<int>(17), 8.0f);

    EXPECT_TRUE(tree.search(FFDBKey<int>(12)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(30)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(7)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(17)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(15)));

    EXPECT_TRUE(new_tree.size() == 4);
    EXPECT_TRUE(tree.size() == 8);

    tree.print();

    db.commit_txn();

    remove("test.db");
}

/*TEST(BTreeTest, Transactions) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 1024);

    constexpr int num_transactions = 1000;
    std::cout << "print" << std::endl;
    BTree::BTree<int, float, 3> trees[num_transactions];

    //db.start_txn(DB_WRITE_TXN);
    //trees[0] = BTree::BTree<int, float, 3>();
    //db.commit_txn();
    for (int i = 1; i < num_transactions; i++) {
        db.start_txn(DB_WRITE_TXN);
        trees[i] = trees[i-1].insert(i, .5f + i);
        db.commit_txn();
    }

    db.start_txn(DB_WRITE_TXN);
    trees[num_transactions-1].insert(num_transactions, 1337.0f);
    db.commit_txn();

    for (int t = 0; t < num_transactions; t++) {
        EXPECT_EQ(trees[t].size(), t + 1);
    }

    remove("test.db");
}*/

TEST(BTreeTest, SequentialInsertTest) {
    remove("test.db");

    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 500000;
    for (int i = 1; i <= num_keys; ++i) {
        tree.insert(FFDBKey<int>(i), i + 0.5f);
    }

    for (int i = 1; i <= num_keys; ++i) {
        EXPECT_TRUE(tree.search(FFDBKey<int>(i)));
    }

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, RandomInsert) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 50;
    std::srand(std::time(nullptr));
    std::set<int> keys;

    while (keys.size() < num_keys) {
        int key = std::rand() % num_keys * 5 + 1;
        if (keys.insert(key).second) {
            tree.insert(FFDBKey<int>(key), key + 0.5f);
        }
    }

    for (int key : keys) {
        EXPECT_TRUE(tree.search(FFDBKey<int>(key)));
    }

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, DuplicateTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 1'000'000;
    std::srand(std::time(nullptr));
    std::set<int> keys;

    while (keys.size() < num_keys) {
        int key = std::rand() % num_keys * 5 + 1;
        if (keys.insert(key).second) {
            tree.insert(FFDBKey<int>(key), key + 0.5f);
        }
    }

    std::set<int> unique_keys;
    bool has_duplicate = false;

    tree.traverse([&unique_keys, &has_duplicate](const FFDBKey<int>& key, auto value) {
        // If you want to compare with int, add a get_value() method to FFDBKey
        int k = std::get<int>(key.key_storage);
        if (unique_keys.contains(k)) {
            std::cout << "Duplicate key found: " << k << ": " << value << std::endl;
            has_duplicate = true;
        } else {
            unique_keys.insert(k);
        }
    });

    EXPECT_FALSE(has_duplicate);

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, ManualDuplicateTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    const int keys[] = {1, 2, 3, 3, 4};
    for (int key : keys) {
        tree.insert(FFDBKey<int>(key), key + 0.5f);
    }

    tree.remove(FFDBKey<int>(3));
    EXPECT_FALSE(tree.search(FFDBKey<int>(3)));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, RemoveTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    tree.insert(FFDBKey<int>(10), 1.0f);
    tree.insert(FFDBKey<int>(20), 2.0f);
    tree.insert(FFDBKey<int>(5),  3.0f);
    tree.insert(FFDBKey<int>(6),  4.0f);
    tree.insert(FFDBKey<int>(12), 5.0f);
    tree.insert(FFDBKey<int>(30), 6.0f);
    tree.insert(FFDBKey<int>(7),  7.0f);
    tree.insert(FFDBKey<int>(17), 8.0f);

    tree.remove(FFDBKey<int>(6));
    tree.remove(FFDBKey<int>(13));
    tree.remove(FFDBKey<int>(7));
    tree.remove(FFDBKey<int>(4));
    tree.remove(FFDBKey<int>(2));
    tree.remove(FFDBKey<int>(16));

    EXPECT_FALSE(tree.search(FFDBKey<int>(6)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(13)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(7)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(4)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(2)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(16)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(10)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(20)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(5)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(12)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(30)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(17)));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, RemoveTestCoW) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    // First transaction: create initial tree with 3 elements
    db.start_txn(DB_WRITE_TXN);
    BTree::BTree<int, float, 3> tree;
    tree.insert(FFDBKey<int>(10), 1.0f);
    tree.insert(FFDBKey<int>(20), 2.0f);
    tree.insert(FFDBKey<int>(5),  3.0f);
    auto tree_copy1 = tree;  // Save reference to this version
    db.commit_txn();

    // Second transaction: add 3 more elements
    db.start_txn(DB_WRITE_TXN);
    tree.insert(FFDBKey<int>(6),  4.0f);
    tree.insert(FFDBKey<int>(12), 5.0f);
    tree.insert(FFDBKey<int>(30), 6.0f);
    auto tree_copy2 = tree;  // Save reference to this version
    db.commit_txn();

    // Third transaction: add 2 more and remove 3
    db.start_txn(DB_WRITE_TXN);
    tree.insert(FFDBKey<int>(7),  7.0f);
    tree.insert(FFDBKey<int>(17), 8.0f);
    tree.remove(FFDBKey<int>(6));
    tree.remove(FFDBKey<int>(7));
    tree.remove(FFDBKey<int>(12));
    db.commit_txn();

    // Verify each version retained its correct state
    // tree_copy1 should have the state from first transaction
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(10)));
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(20)));
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(5)));
    EXPECT_FALSE(tree_copy1.search(FFDBKey<int>(6)));
    EXPECT_EQ(tree_copy1.size(), 3);

    // tree_copy2 should have the state from second transaction
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(10)));
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(6)));
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(12)));
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(30)));
    EXPECT_FALSE(tree_copy2.search(FFDBKey<int>(7)));
    EXPECT_EQ(tree_copy2.size(), 6);

    // Final tree should have the state after all transactions
    EXPECT_TRUE(tree.search(FFDBKey<int>(10)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(20)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(5)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(6)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(7)));
    EXPECT_FALSE(tree.search(FFDBKey<int>(12)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(30)));
    EXPECT_TRUE(tree.search(FFDBKey<int>(17)));
    EXPECT_EQ(tree.size(), 5);

    remove("test.db");
}

TEST(BTreeTest, SequentialInsertRemove) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 1000000;
    for (int i = num_keys; i >= num_keys; --i) {
        tree.insert(FFDBKey<int>(i), i + 0.5f);
    }

    for (int i = num_keys; i >= num_keys; --i) {
        tree.remove(FFDBKey<int>(i));
        EXPECT_FALSE(tree.search(FFDBKey<int>(i))) << "Failed for key: " << i;
    }

    remove("test.db");
}

// // NOTE: It seems that this could generate duplicate keys which could
// // mess upp some of the tests
// std::vector<int> generateRandomKeys(int num_keys) {
//     std::srand(std::time(nullptr));
//     std::vector<int> keys;
//     std::unordered_set<int> keySet;
//
//     while (keySet.size() < num_keys) {
//         int key = std::rand() % num_keys;
//         if (keySet.insert(key).second) {
//             keys.push_back(key);
//         }
//     }
//
//     return keys;
// }
//
// TEST(BTreeTest, RandomInsertRemove) {
//     FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
//     db.start_transaction();
//
//     BTree::BTree<int, float, 3> tree(db.alloc);
//
//     constexpr int num_keys = 1'000'000;
//     std::cout << "Generating random keys..." << std::endl;
//     std::vector<int> keys = generateRandomKeys(num_keys);
//     for (const int key : keys) {
//         tree.insert(key);
//         EXPECT_TRUE(tree.search(key)) << "Insert failed for key: " << key;
//     }
//
//     std::cout << "Shuffling keys for random removal..." << std::endl;
//
//     // Shuffle the keys for random removal
//     auto rng = std::default_random_engine {};
//     std::ranges::shuffle(keys, rng);
//
//     std::cout << "Removing keys..." << std::endl;
//     for (const int key : keys) {
//         tree.remove(key);
//         EXPECT_FALSE(tree.search(key)) << "Failed for key: " << key;;
//     }
//
//     for (const int key : keys) {
//         tree.insert(key);
//         EXPECT_TRUE(tree.search(key)) << "Insert failed for key: " << key;
//     }
//
//     db.commit_transaction();
//
//     remove("test.db");
// }

TEST(BTreeTest, RootNodeDelete) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    // Insertion order
    std::vector insertKeys = {15, 11, 27, 3, 21, 24, 19, 13, 22, 4, 5, 8, 28, 26, 7,
                              6, 23, 29, 16, 2, 25, 0, 17, 12, 14, 1, 9, 18, 20, 10};
    for (int key : insertKeys) {
        std::cout << "inserting key " << key << std::endl;
        tree.insert(FFDBKey<int>(key), key + 0.5f);
        EXPECT_TRUE(tree.search(FFDBKey<int>(key)));
    }

    tree.print();

    // Deletion order
    tree.remove(FFDBKey<int>(9));
    EXPECT_FALSE(tree.search(FFDBKey<int>(9)));
    tree.remove(FFDBKey<int>(20));
    EXPECT_FALSE(tree.search(FFDBKey<int>(20)));
    tree.remove(FFDBKey<int>(6));
    EXPECT_FALSE(tree.search(FFDBKey<int>(6)));
    tree.remove(FFDBKey<int>(19));
    EXPECT_FALSE(tree.search(FFDBKey<int>(19)));
    tree.remove(FFDBKey<int>(8));
    EXPECT_FALSE(tree.search(FFDBKey<int>(8)));
    tree.remove(FFDBKey<int>(15));
    EXPECT_FALSE(tree.search(FFDBKey<int>(15)));
    std::cout << "BEFORE ----------------" << std::endl;
    tree.print();

    tree.remove(FFDBKey<int>(21));

    std::cout << "AFTER ----------------" << std::endl;
    tree.print();
    EXPECT_FALSE(tree.search(FFDBKey<int>(21)));

    db.commit_txn();

    remove("test.db");
}

// TEST(BTreeTest, IncrementalInsertDelete) {
//     FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
//     db.start_transaction();
//
//     BTree::BTree<int, float, 3> tree(db.alloc);
//
//     const int start = 44;
//     const int num_keys = 50;
//     std::vector<int> keys = generateRandomKeys(num_keys);
//     for (int i = start; i < num_keys; ++i) {
//         std::cout << "\niteration " << i << std::endl;
//         bool testFailed = false;
//         int failedKey = -1;
//         std::string failedOperation;
//
//         // Insert all keys
//         for (int j = 0; j < i; ++j) {
//             int key = keys[j];
//             std::cout << "insert " << key << ", (j=" << j << ")" << std::endl;
//             tree.insert(key);
//             if (!tree.search(key)) {
//                 testFailed = true;
//                 failedKey = key;
//                 failedOperation = "insert";
//                 break;
//             }
//         }
//
//         if (testFailed) {
//             FAIL() << "Test failed at iteration " << i
//                    << " during " << failedOperation
//                    << " for key: " << failedKey;
//             break;
//         }
//
//         // Remove all keys
//         for (int j = 0; j < i; ++j) {
//             int key = keys[j];
//             std::cout << "remove " << key << ", (j=" << j << ")" << std::endl;
//             tree.remove(key);
//             if (tree.search(key)) {
//                 testFailed = true;
//                 failedKey = key;
//                 failedOperation = "delete";
//
//                 // tree.remove(key);
//                 // if (tree.search(key)) {
//                 //     std::cout << "it failed again" << std::endl;
//                 // }
//
//                 break;
//             }
//         }
//
//         if (testFailed) {
//             FAIL() << "Test failed at iteration " << i
//                    << " during " << failedOperation
//                    << " for key: " << failedKey;
//             break;
//         }
//
//         // Re-insert to verify
//         for (int j = 0; j < i; ++j) {
//             int key = keys[j];
//             std::cout << "re-insert " << key << ", (j=" << j << ")" << std::endl;
//             tree.insert(key);
//             if (!tree.search(key)) {
//                 testFailed = true;
//                 failedKey = key;
//                 failedOperation = "re-insert";
//                 break;
//             }
//         }
//
//         if (testFailed) {
//             FAIL() << "Test failed at iteration " << i
//                    << " during " << failedOperation
//                    << " for key: " << failedKey;
//             break;
//         }
//     }
//
//     db.commit_transaction();
//
//     remove("test.db");
// }