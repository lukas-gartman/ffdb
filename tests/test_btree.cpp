#include <gtest/gtest.h>
#include "../BTree.h"

#include <cstdlib>
#include <ctime>

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
    constexpr int numKeys = 50;
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
    int keys[numKeys];

    for (int i = 0; i < numKeys; ++i) {
        keys[i] = std::rand() % 100000 + 1;
        tree.insert(keys[i]);
    }

    for (int i = 0; i < numKeys; ++i) {
        EXPECT_TRUE(tree.search(keys[i]));
    }
}

TEST(BTreeTest, RemoveTest) {
    BTree<int, 3> tree;
    tree.insert(13); // remove this
    tree.insert(10);
    tree.insert(20);
    tree.insert(5);
    tree.insert(6);
    tree.insert(12);
    // tree.insert(30);
    tree.insert(7);
    // tree.insert(17);

    tree.remove(6);
    tree.remove(13);
    tree.remove(7);
    // tree.remove(4);
    // tree.remove(2);
    // tree.remove(16);

    EXPECT_FALSE(tree.search(6));
    EXPECT_FALSE(tree.search(13));
    EXPECT_FALSE(tree.search(7));
    // EXPECT_FALSE(tree.search(4));
    // EXPECT_FALSE(tree.search(2));
    // EXPECT_FALSE(tree.search(16));
    EXPECT_TRUE(tree.search(10));
    EXPECT_TRUE(tree.search(20));
    EXPECT_TRUE(tree.search(5));
    // EXPECT_TRUE(tree.search(12));
    // EXPECT_TRUE(tree.search(30));
    // EXPECT_TRUE(tree.search(17));
}