#include <gtest/gtest.h>
#include "../src/BTreeFunctional.h"
#include "../src/FFDB.h"
#include "../src/FFDBKey.h"
#include <fcntl.h>

#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <unordered_set>

TEST(BTreeFunctionalTest, InsertTest) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    tree = BT::insert(tree, 10, 1.0f);
    tree = BT::insert(tree, 20, 2.0f);
    tree = BT::insert(tree, 5, 3.0f);
    tree = BT::insert(tree, 5, 300.0f);
    tree = BT::insert(tree, 5, 300000.0f);

    EXPECT_TRUE(BT::key_exists(tree, 10));
    EXPECT_TRUE(BT::key_exists(tree, 20));
    EXPECT_TRUE(BT::key_exists(tree, 5));

    db.commit_txn();
    db.start_txn(DB_WRITE_TXN);

    auto new_tree = BT::insert(tree, 6, 4.0f);

    tree = BT::insert(tree, 12, 5.0f);
    tree = BT::insert(tree, 38, 6.0f);
    tree = BT::insert(tree, 7, 7.0f);
    tree = BT::insert(tree, 17, 8.0f);

    EXPECT_TRUE(BT::key_exists(tree, 12));
    EXPECT_TRUE(BT::key_exists(tree, 38));
    EXPECT_TRUE(BT::key_exists(tree, 7));
    EXPECT_TRUE(BT::key_exists(tree, 17));
    EXPECT_FALSE(BT::key_exists(tree, 15));
    EXPECT_FALSE(BT::key_exists(tree, 6));
    EXPECT_TRUE(BT::key_exists(new_tree, 6));

    EXPECT_TRUE(BT::size(new_tree) == 4);
    EXPECT_TRUE(BT::size(tree) == 7);

    db.commit_txn();

    remove("test.db");
}
/*

TEST(BTreeTest, Transactions) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 1024);

    constexpr int num_transactions = 1000;

    db.start_txn(DB_WRITE_TXN);
    BTree::BTree<int, float, 3> trees[num_transactions];
    trees[0] = BTree::BTree<int, float, 3>();
    db.commit_txn();
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
}

TEST(BTreeTest, SequentialInsertTest) {
    remove("test.db");

    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 500000;
    for (int i = 1; i <= num_keys; ++i) {
        tree.insert(i, i + 0.5f);
    }

    for (int i = 1; i <= num_keys; ++i) {
        EXPECT_TRUE(i);
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
            tree.insert(key, key + 0.5f);
        }
    }

    for (int key : keys) {
        EXPECT_TRUE(tree.search(key));
    }

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, DuplicateTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    constexpr int num_keys = 100000;
    std::srand(std::time(nullptr));
    std::set<int> keys;

    while (keys.size() < num_keys) {
        int key = std::rand() % num_keys * 5 + 1;
        if (keys.insert(key).second) {
            tree.insert(key, key + 0.5f);
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
        tree.insert(key, key + 0.5f);
    }

    tree.remove(3);
    EXPECT_FALSE(tree.search(3));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, RemoveTest) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    BTree::BTree<int, float, 3> tree;

    int arr[] = {1,2,3,4,5};
    int arr2[] = {1, 2,3,4,5};

    tree.insert(arr, 1.0f);


    EXPECT_TRUE(tree.search(arr2));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, RemoveTestCoW) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    // First transaction: create initial tree with 3 elements
    db.start_txn(DB_WRITE_TXN);
    BTree::BTree<int, float, 3> tree;
    tree.insert(10, 1.0f);
    tree.insert(20, 2.0f);
    tree.insert(5,  3.0f);
    auto tree_copy1 = tree;  // Save reference to this version
    db.commit_txn();

    // Second transaction: add 3 more elements
    db.start_txn(DB_WRITE_TXN);
    tree.insert(6,  4.0f);
    tree.insert(12, 5.0f);
    tree.insert(30, 6.0f);
    auto tree_copy2 = tree;  // Save reference to this version
    db.commit_txn();

    // Third transaction: add 2 more and remove 3
    db.start_txn(DB_WRITE_TXN);
    tree.insert(7,  7.0f);
    tree.insert(17, 8.0f);
    tree.remove(6);
    tree.remove(7);
    tree.remove(12);
    db.commit_txn();

    // Verify each version retained its correct state
    // tree_copy1 should have the state from first transaction
    EXPECT_TRUE(tree_copy1.search(10));
    EXPECT_TRUE(tree_copy1.search(20));
    EXPECT_TRUE(tree_copy1.search(5));
    EXPECT_FALSE(tree_copy1.search(6));
    EXPECT_EQ(tree_copy1.size(), 3);

    // tree_copy2 should have the state from second transaction
    EXPECT_TRUE(tree_copy2.search(10));
    EXPECT_TRUE(tree_copy2.search(6));
    EXPECT_TRUE(tree_copy2.search(12));
    EXPECT_TRUE(tree_copy2.search(30));
    EXPECT_FALSE(tree_copy2.search(7));
    EXPECT_EQ(tree_copy2.size(), 6);

    // Final tree should have the state after all transactions
    EXPECT_TRUE(tree.search(10));
    EXPECT_TRUE(tree.search(20));
    EXPECT_TRUE(tree.search(5));
    EXPECT_FALSE(tree.search(6));
    EXPECT_FALSE(tree.search(7));
    EXPECT_FALSE(tree.search(12));
    EXPECT_TRUE(tree.search(30));
    EXPECT_TRUE(tree.search(17));
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
        tree.insert(i, i + 0.5f);
    }

    for (int i = num_keys; i >= num_keys; --i) {
        tree.remove(i);
        EXPECT_FALSE(tree.search(i)) << "Failed for key: " << i;
    }

    remove("test.db");
}

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
        tree.insert(key, key + 0.5f);
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

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeTest, getSize) {
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
    EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
}

*/