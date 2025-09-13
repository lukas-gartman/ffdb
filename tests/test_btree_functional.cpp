#include <gtest/gtest.h>
#include "../src/BTreeFunctional.h"
#include "../src/FFDB.h"
#include "../src/FFDBKey.h"
#include <fcntl.h>

#include <cstdlib>
#include <algorithm>
#include <random>
#include <unordered_set>

TEST(BTreeFunctionalTest, TESTILINGUS) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    db.commit_txn();

    remove("test.db");
}

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

TEST(BTreeFunctionalTest, Transactions) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 1024);

    constexpr int num_transactions = 40;

    db.start_txn(DB_WRITE_TXN);
    std::vector<decltype(BT::empty())> trees;

    auto tree = BT::empty();
    tree = BT::insert(tree, 1, 1.5f);
    trees.push_back(tree);
    db.commit_txn();

    for (int i = 2; i <= num_transactions; ++i) {
        db.start_txn(DB_WRITE_TXN);
        trees.push_back(BT::insert(trees.back(), i, i + 0.5f));
        db.commit_txn();
    }

    db.start_txn(DB_WRITE_TXN);
    BT::insert(trees.back(), num_transactions + 1, 1337.0f);
    db.commit_txn();

    for (int t = 0; t < num_transactions; ++t) {
        EXPECT_EQ(BT::size(trees[t]), t + 1);
    }

    remove("test.db");
}

TEST(BTreeFunctionalTest, SequentialInsertTest) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");

    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    constexpr int num_keys = 500000;
    for (int i = 1; i <= num_keys; ++i) {
        tree = BT::insert(tree, i, i + 0.5f);
    }

    for (int i = 1; i <= num_keys; ++i) {
        EXPECT_TRUE(BT::key_exists(tree, i));
    }

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeFunctionalTest, RandomInsert) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    constexpr int num_keys = 50;
    // deterministic seed: do not call srand
    std::set<int> keys;

    while (keys.size() < num_keys) {
        int key = (std::rand() % num_keys) * 5 + 1;
        if (keys.insert(key).second) {
            tree = BT::insert(tree, key, key + 0.5f);
        }
    }

    for (int key : keys) {
        EXPECT_TRUE(BT::key_exists(tree, key));
    }

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeFunctionalTest, DuplicateTest) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16384);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    constexpr int num_keys = 100000;
    // deterministic seed: do not call srand
    std::set<int> keys;

    while (keys.size() < num_keys) {
        int key = (std::rand() % num_keys) * 5 + 1;
        if (keys.insert(key).second) {
            tree = BT::insert(tree, key, key + 0.5f);
        }
    }

    EXPECT_EQ(BT::size(tree), (int)keys.size());
    for (int k : keys) EXPECT_TRUE(BT::key_exists(tree, k));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeFunctionalTest, ManualDuplicateTest) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    const int keys[] = {1, 2, 3, 3, 4};
    for (int key : keys) {
        tree = BT::insert(tree, key, key + 0.5f);
    }

    auto [new_tree, removed] = BT::remove(tree, 3);
    EXPECT_TRUE(removed);
    EXPECT_FALSE(BT::key_exists(new_tree, 3));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeFunctionalTest, RemoveTest) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    for (int i = 1; i <= 5; ++i) tree = BT::insert(tree, i, i + 0.5f);

    auto [t_after_remove, removed] = BT::remove(tree, 3);
    EXPECT_TRUE(removed);
    EXPECT_FALSE(BT::key_exists(t_after_remove, 3));

    db.commit_txn();

    remove("test.db");
}

TEST(BTreeFunctionalTest, RemoveTestCoW) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);
    auto tree = BT::empty();
    tree = BT::insert(tree, 10, 1.0f);
    tree = BT::insert(tree, 20, 2.0f);
    tree = BT::insert(tree, 5,  3.0f);
    auto tree_copy1 = tree;
    db.commit_txn();

    db.start_txn(DB_WRITE_TXN);
    tree = BT::insert(tree, 6,  4.0f);
    tree = BT::insert(tree, 12, 5.0f);
    tree = BT::insert(tree, 30, 6.0f);
    auto tree_copy2 = tree;
    db.commit_txn();

    db.start_txn(DB_WRITE_TXN);
    tree = BT::insert(tree, 7,  7.0f);
    tree = BT::insert(tree, 17, 8.0f);
    auto res1 = BT::remove(tree, 6); tree = res1.first;
    auto res2 = BT::remove(tree, 7); tree = res2.first;
    auto res3 = BT::remove(tree, 12); tree = res3.first;
    db.commit_txn();

    EXPECT_TRUE(BT::key_exists(tree_copy1, 10));
    EXPECT_TRUE(BT::key_exists(tree_copy1, 20));
    EXPECT_TRUE(BT::key_exists(tree_copy1, 5));
    EXPECT_FALSE(BT::key_exists(tree_copy1, 6));
    EXPECT_EQ(BT::size(tree_copy1), 3);

    EXPECT_TRUE(BT::key_exists(tree_copy2, 10));
    EXPECT_TRUE(BT::key_exists(tree_copy2, 6));
    EXPECT_TRUE(BT::key_exists(tree_copy2, 12));
    EXPECT_TRUE(BT::key_exists(tree_copy2, 30));
    EXPECT_FALSE(BT::key_exists(tree_copy2, 7));
    EXPECT_EQ(BT::size(tree_copy2), 6);

    EXPECT_TRUE(BT::key_exists(tree, 10));
    EXPECT_TRUE(BT::key_exists(tree, 20));
    EXPECT_TRUE(BT::key_exists(tree, 5));
    EXPECT_FALSE(BT::key_exists(tree, 6));
    EXPECT_FALSE(BT::key_exists(tree, 7));
    EXPECT_FALSE(BT::key_exists(tree, 12));
    EXPECT_TRUE(BT::key_exists(tree, 30));
    EXPECT_TRUE(BT::key_exists(tree, 17));
    EXPECT_EQ(BT::size(tree), 5);

    remove("test.db");
}

TEST(BTreeFunctionalTest, SequentialInsertRemove) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    constexpr int num_keys = 1000;
    for (int i = num_keys; i >= 1; --i) {
        tree = BT::insert(tree, i, i + 0.5f);
    }

    for (int i = num_keys; i >= 1; --i) {
        auto [new_tree, removed] = BT::remove(tree, i);
        tree = new_tree;
        EXPECT_TRUE(removed) << "Failed to remove: " << i;
        EXPECT_FALSE(BT::key_exists(tree, i)) << "Failed for key: " << i;
    }

    remove("test.db");
}

TEST(BTreeFunctionalTest, RootNodeDelete) {
    using BT = BTreeFunctional<int, float, 3>;

    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();

    std::vector<int> insertKeys = {15, 11, 27, 3, 21, 24, 19, 13, 22, 4, 5, 8, 28, 26, 7,
                                   6, 23, 29, 16, 2, 25, 0, 17, 12, 14, 1, 9, 18, 20, 10};
    for (int key : insertKeys) {
        tree = BT::insert(tree, key, key + 0.5f);
        EXPECT_TRUE(BT::key_exists(tree, key));
    }

    tree = BT::remove(tree, 9).first;
    EXPECT_FALSE(BT::key_exists(tree, 9));
    tree = BT::remove(tree, 20).first;
    EXPECT_FALSE(BT::key_exists(tree, 20));
    tree = BT::remove(tree, 6).first;
    EXPECT_FALSE(BT::key_exists(tree, 6));
    tree = BT::remove(tree, 19).first;
    EXPECT_FALSE(BT::key_exists(tree, 19));
    tree = BT::remove(tree, 8).first;
    EXPECT_FALSE(BT::key_exists(tree, 8));
    tree = BT::remove(tree, 15).first;
    EXPECT_FALSE(BT::key_exists(tree, 15));

    tree = BT::remove(tree, 21).first;
    EXPECT_FALSE(BT::key_exists(tree, 21));

    db.commit_txn();

    remove("test.db");
}

// TEST(BTreeTest, getSize) {
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
//     EXPECT_EQ((sizeof(BTree::Node<int, int, 5>)), (BTree::sizeof_node<int, int>(5)));
// }
