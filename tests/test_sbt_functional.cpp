#include <gtest/gtest.h>
#include "../src/SBTFunctional.h"
#include "../src/FFDB.h"
#include "../src/FFDBKey.h"

#include <algorithm>
#include <fcntl.h>
#include <random>

TEST(SBTTestFunctional, InsertAndSearch) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    using SBT = SBTFunctional<int, float>;

    db.start_txn(DB_WRITE_TXN);
    auto tree = SBT::empty();
    tree = SBT::insert(tree, 10, 1.0f);
    tree = SBT::insert(tree, 20, 2.0f);
    tree = SBT::insert(tree, 5, 3.0f);
    tree = SBT::insert(tree, 10, 100.0f); // Nothing inserted

    EXPECT_TRUE(SBT::key_exists(tree, 10));
    EXPECT_TRUE(SBT::key_exists(tree, 20));
    EXPECT_TRUE(SBT::key_exists(tree, 5));

    EXPECT_TRUE(SBT::get_value(tree, 10) == 1.0f);
    EXPECT_TRUE(SBT::get_value(tree, 20) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree, 5) == 3.0f);
    EXPECT_FALSE(SBT::get_value(tree, 99) != std::nullopt);

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTestFunctional, Remove) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    using SBT = SBTFunctional<int, float>;

    db.start_txn(DB_WRITE_TXN);
    auto tree = SBT::empty();

    tree = SBT::insert(tree, 10, 1.0f);
    tree = SBT::insert(tree, 20, 2.0f);
    tree = SBT::insert(tree, 5, 3.0f);
    tree = SBT::remove(tree, 10);

    EXPECT_FALSE(SBT::get_value(tree, 10) == 1.0f);
    EXPECT_TRUE(SBT::get_value(tree, 20) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree, 5) == 3.0f);

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTestFunctional, InOrderTraversal) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    using SBT = SBTFunctional<int, float>;

    db.start_txn(DB_WRITE_TXN);
    auto tree = SBT::empty();

    std::vector<int> keys = {10, 5, 20, 15, 25};
    for (int i = 0; i < keys.size(); i++) tree = SBT::insert(tree, keys[i], i);

    std::vector<float> vals = SBT::values(tree);

    std::vector<float> expected = {1, 0, 3, 2, 4};
    EXPECT_EQ(vals, expected);

    db.commit_txn();
    remove("test.db");
}
/*
TEST(SBTTestFunctional, CopyOnWrite) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    // create initial tree
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;
    tree.insert(1, 1.0f);
    tree.insert(2, 2.0f);
    tree.insert(3, 3.0f);
    auto tree_copy1 = tree; // snapshot
    db.commit_txn();

    // modify tree
    db.start_txn(DB_WRITE_TXN);
    tree.insert(4, 4.0f);
    tree.remove(2);
    auto tree_copy2 = tree; // snapshot
    db.commit_txn();

    // further modify
    db.start_txn(DB_WRITE_TXN);
    tree.insert(5, 5.0f);
    tree.remove(1);
    db.commit_txn();

    // Check snapshots
    float value = 0;
    // tree_copy1: 1,2,3
    EXPECT_TRUE(tree_copy1.search(1, value));
    EXPECT_TRUE(tree_copy1.search(2, value));
    EXPECT_TRUE(tree_copy1.search(3, value));
    EXPECT_FALSE(tree_copy1.search(4, value));
    EXPECT_FALSE(tree_copy1.search(5, value));
    EXPECT_EQ(tree_copy1.size(), 3);

    // tree_copy2: 1,3,4
    EXPECT_TRUE(tree_copy2.search(1, value));
    EXPECT_FALSE(tree_copy2.search(2, value));
    EXPECT_TRUE(tree_copy2.search(3, value));
    EXPECT_TRUE(tree_copy2.search(4, value));
    EXPECT_FALSE(tree_copy2.search(5, value));
    EXPECT_EQ(tree_copy2.size(), 3);

    // tree: 3,4,5
    EXPECT_FALSE(tree.search(1, value));
    EXPECT_FALSE(tree.search(2, value));
    EXPECT_TRUE(tree.search(3, value));
    EXPECT_TRUE(tree.search(4, value));
    EXPECT_TRUE(tree.search(5, value));
    EXPECT_EQ(tree.size(), 3);

    remove("test.db");
}

TEST(SBTTestFunctional, Update) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    // Insert initial values
    tree.insert(10, 1.0f);
    tree.insert(20, 2.0f);
    tree.insert(5, 3.0f);

    // Test successful updates
    EXPECT_TRUE(tree.update(10, 10.0f));
    EXPECT_TRUE(tree.update(20, 20.0f));
    EXPECT_TRUE(tree.update(5, 5.0f));

    // Verify updated values
    float value = 0;
    EXPECT_TRUE(tree.search(10, value));
    EXPECT_EQ(value, 10.0f);
    EXPECT_TRUE(tree.search(20, value));
    EXPECT_EQ(value, 20.0f);
    EXPECT_TRUE(tree.search(5, value));
    EXPECT_EQ(value, 5.0f);

    // Test update on non-existent key
    EXPECT_FALSE(tree.update(99, 99.0f));

    db.commit_txn();

    remove("test.db");
}

TEST(SBTTestFunctional, SBTBalancedPrint) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    int num_keys = 100;
    int keys[num_keys];
    for (int i = 0; i < num_keys; ++i) {
        keys[i] = i+1;
    }

    std::shuffle(keys, keys + num_keys, std::mt19937());

    std::cout << "\nGenerated keys: ";
    for (int key : keys) {
        std::cout << key << " ";
        tree.insert(key, float(key));
    }

    std::cout << "\nIn-order traversal: ";
    tree.in_order([](const FFDBKey<int>& k, float v) { std::cout << std::get<int>(k.key_storage) << " "; });

    EXPECT_TRUE(check_sbt_balance(tree));

    db.commit_txn();
    remove("test.db");
}*/