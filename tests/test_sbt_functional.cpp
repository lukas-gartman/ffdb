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

    EXPECT_TRUE(SBT::valid(tree));

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
    auto [tree_after_remove, removed] = SBT::remove(tree, 10);
    tree = tree_after_remove;

    EXPECT_FALSE(SBT::get_value(tree, 10) == 1.0f);
    EXPECT_TRUE(SBT::get_value(tree, 20) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree, 5) == 3.0f);

    EXPECT_TRUE(SBT::valid(tree));

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

    EXPECT_TRUE(SBT::valid(tree));

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTestFunctional, CopyOnWrite) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    using SBT = SBTFunctional<int, float>;

    // create initial tree
    db.start_txn(DB_WRITE_TXN);
    auto tree1 = SBT::empty();
    tree1 = SBT::insert(tree1, 1, 1.0f);
    tree1 = SBT::insert(tree1, 2, 2.0f);
    tree1 = SBT::insert(tree1, 3, 3.0f);
    db.commit_txn();

    // modify tree
    db.start_txn(DB_WRITE_TXN);
    auto tree2 = SBT::insert(tree1, 4, 4.0f);
    auto [tree2_after_remove, r2] = SBT::remove(tree2, 2);
    tree2 = tree2_after_remove;
    db.commit_txn();

    // further modify
    db.start_txn(DB_WRITE_TXN);
    auto tree3 = SBT::insert(tree2, 5, 5.0f);
    auto [tree3_after_remove, r3] = SBT::remove(tree3, 1);
    tree3 = tree3_after_remove;
    db.commit_txn();

    db.start_txn(DB_WRITE_TXN);
    auto tree4 = SBT::update_value(tree3, 3, [](float v){ return ++v; });
    tree4 = SBT::update_value(tree4, 4, [](float v){ return v - 4; });
    tree4 = SBT::replace_value(tree4, 5, 500);
    SBT::print(tree4); std::cout << std::endl;
    db.commit_txn();

    // Check snapshots
    float value = 0;
    // tree_copy1: 1,2,3
    EXPECT_TRUE(SBT::get_value(tree1, 1) == 1.0f);
    EXPECT_TRUE(SBT::get_value(tree1, 2) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree1, 3) == 3.0f);
    EXPECT_FALSE(SBT::get_value(tree1, 4) == 4.0f);
    EXPECT_FALSE(SBT::get_value(tree1, 5) == 5.0f);
    EXPECT_EQ(SBT::get_size(tree1), 3);
    EXPECT_TRUE(SBT::valid(tree1));

    // tree_copy2: 1,3,4
    EXPECT_TRUE(SBT::get_value(tree2, 1) == 1.0f);
    EXPECT_FALSE(SBT::get_value(tree2, 2) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree2, 3) == 3.0f);
    EXPECT_TRUE(SBT::get_value(tree2, 4) == 4.0f);
    EXPECT_FALSE(SBT::get_value(tree2, 5) == 5.0f);
    EXPECT_EQ(SBT::get_size(tree2), 3);
    EXPECT_TRUE(SBT::valid(tree2));

    // tree: 3,4,5
    EXPECT_FALSE(SBT::get_value(tree3, 1) == 1.0f);
    EXPECT_FALSE(SBT::get_value(tree3, 2) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree3, 3) == 3.0f);
    EXPECT_TRUE(SBT::get_value(tree3, 4) == 4.0f);
    EXPECT_TRUE(SBT::get_value(tree3, 5) == 5.0f);
    EXPECT_EQ(SBT::get_size(tree3), 3);
    EXPECT_TRUE(SBT::valid(tree3));

    EXPECT_FALSE(SBT::get_value(tree4, 1) == 1.0f);
    EXPECT_FALSE(SBT::get_value(tree4, 2) == 2.0f);
    EXPECT_TRUE(SBT::get_value(tree4, 3) == 4.0f);
    EXPECT_TRUE(SBT::get_value(tree4, 4) == 0.0f);
    EXPECT_TRUE(SBT::get_value(tree4, 5) == 500.0f);
    EXPECT_EQ(SBT::get_size(tree4), 3);
    EXPECT_TRUE(SBT::valid(tree4));

    remove("test.db");
}

TEST(SBTTestFunctional, Update) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    using SBT = SBTFunctional<int, float>;

    db.start_txn(DB_WRITE_TXN);
    auto tree = SBT::empty();

    // Insert initial values
    tree = SBT::insert(tree, 10, 1.0f);
    tree = SBT::insert(tree, 20, 2.0f);
    tree = SBT::insert(tree, 5, 3.0f);
    tree = SBT::replace_value(tree, 10, 10.0f);
    tree = SBT::replace_value(tree, 20, 20.0f);
    tree = SBT::replace_value(tree, 5, 5.0f);

    // Verify updated values
    EXPECT_EQ(SBT::get_value(tree, 10), 10.0f);
    EXPECT_EQ(SBT::get_value(tree, 20), 20.0f);
    EXPECT_EQ(SBT::get_value(tree, 5), 5.0f);
    EXPECT_TRUE(SBT::valid(tree));

    db.commit_txn();

    remove("test.db");
}