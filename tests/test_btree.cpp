#include <gtest/gtest.h>
#include "../BTree.h"

TEST(BTreeTest, InsertTest) {
    BTree<int> tree(3);
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
