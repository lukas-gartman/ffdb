#include <gtest/gtest.h>
#include "../src/SBT.h"
#include "../src/FFDB2.h"
#include "../src/FFDBKey.h"

#include <algorithm>
#include <fcntl.h>
#include <random>

TEST(SBTTest, InsertAndSearch) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    EXPECT_TRUE(tree.insert(FFDBKey<int>(10), 1.0f));
    EXPECT_TRUE(tree.insert(FFDBKey<int>(20), 2.0f));
    EXPECT_TRUE(tree.insert(FFDBKey<int>(5), 3.0f));
    EXPECT_FALSE(tree.insert(FFDBKey<int>(10), 100.0f)); // duplicate

    float value = 0;
    EXPECT_TRUE(tree.search(FFDBKey<int>(10), value));
    EXPECT_EQ(value, 1.0f);
    EXPECT_TRUE(tree.search(FFDBKey<int>(20), value));
    EXPECT_EQ(value, 2.0f);
    EXPECT_TRUE(tree.search(FFDBKey<int>(5), value));
    EXPECT_EQ(value, 3.0f);
    EXPECT_FALSE(tree.search(FFDBKey<int>(99), value));

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTest, Remove) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    tree.insert(FFDBKey<int>(10), 1.0f);
    tree.insert(FFDBKey<int>(20), 2.0f);
    tree.insert(FFDBKey<int>(5), 3.0f);

    EXPECT_TRUE(tree.remove(FFDBKey<int>(10)));
    float value = 0;
    EXPECT_FALSE(tree.search(FFDBKey<int>(10), value));
    EXPECT_TRUE(tree.search(FFDBKey<int>(20), value));
    EXPECT_TRUE(tree.search(FFDBKey<int>(5), value));
    EXPECT_FALSE(tree.remove(FFDBKey<int>(42))); // not present

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTest, InOrderTraversal) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    std::vector<int> keys = {10, 5, 20, 15, 25};
    for (int k : keys) tree.insert(FFDBKey<int>(k), k + 0.5f);

    std::vector<int> traversed;
    tree.in_order([&](const FFDBKey<int>& k, float v) { traversed.push_back(std::get<int>(k.key_storage)); });

    std::vector<int> expected = {5, 10, 15, 20, 25};
    EXPECT_EQ(traversed, expected);

    db.commit_txn();
    remove("test.db");
}

TEST(SBTTest, CopyOnWrite) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    // create initial tree
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;
    tree.insert(FFDBKey<int>(1), 1.0f);
    tree.insert(FFDBKey<int>(2), 2.0f);
    tree.insert(FFDBKey<int>(3), 3.0f);
    auto tree_copy1 = tree; // snapshot
    db.commit_txn();

    // modify tree
    db.start_txn(DB_WRITE_TXN);
    tree.insert(FFDBKey<int>(4), 4.0f);
    tree.remove(FFDBKey<int>(2));
    auto tree_copy2 = tree; // snapshot
    db.commit_txn();

    // further modify
    db.start_txn(DB_WRITE_TXN);
    tree.insert(FFDBKey<int>(5), 5.0f);
    tree.remove(FFDBKey<int>(1));
    db.commit_txn();

    // Check snapshots
    float value = 0;
    // tree_copy1: 1,2,3
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(1), value));
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(2), value));
    EXPECT_TRUE(tree_copy1.search(FFDBKey<int>(3), value));
    EXPECT_FALSE(tree_copy1.search(FFDBKey<int>(4), value));
    EXPECT_FALSE(tree_copy1.search(FFDBKey<int>(5), value));
    EXPECT_EQ(tree_copy1.size(), 3);

    // tree_copy2: 1,3,4
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(1), value));
    EXPECT_FALSE(tree_copy2.search(FFDBKey<int>(2), value));
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(3), value));
    EXPECT_TRUE(tree_copy2.search(FFDBKey<int>(4), value));
    EXPECT_FALSE(tree_copy2.search(FFDBKey<int>(5), value));
    EXPECT_EQ(tree_copy2.size(), 3);

    // tree: 3,4,5
    EXPECT_FALSE(tree.search(FFDBKey<int>(1), value));
    EXPECT_FALSE(tree.search(FFDBKey<int>(2), value));
    EXPECT_TRUE(tree.search(FFDBKey<int>(3), value));
    EXPECT_TRUE(tree.search(FFDBKey<int>(4), value));
    EXPECT_TRUE(tree.search(FFDBKey<int>(5), value));
    EXPECT_EQ(tree.size(), 3);

    remove("test.db");
}

TEST(SBTTest, Update) {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);
    db.start_txn(DB_WRITE_TXN);
    SBT::SBT<int, float> tree;

    // Insert initial values
    tree.insert(FFDBKey<int>(10), 1.0f);
    tree.insert(FFDBKey<int>(20), 2.0f);
    tree.insert(FFDBKey<int>(5), 3.0f);

    // Test successful updates
    EXPECT_TRUE(tree.update(FFDBKey<int>(10), 10.0f));
    EXPECT_TRUE(tree.update(FFDBKey<int>(20), 20.0f));
    EXPECT_TRUE(tree.update(FFDBKey<int>(5), 5.0f));

    // Verify updated values
    float value = 0;
    EXPECT_TRUE(tree.search(FFDBKey<int>(10), value));
    EXPECT_EQ(value, 10.0f);
    EXPECT_TRUE(tree.search(FFDBKey<int>(20), value));
    EXPECT_EQ(value, 20.0f);
    EXPECT_TRUE(tree.search(FFDBKey<int>(5), value));
    EXPECT_EQ(value, 5.0f);

    // Test update on non-existent key
    EXPECT_FALSE(tree.update(FFDBKey<int>(99), 99.0f));

    db.commit_txn();

    remove("test.db");
}

bool check_sbt_balance(const SBT::SBT<int, float> &tree) {
    using Node = SBT::Node<int, float>;
    std::function<bool(Ref<Node>)> check = [&](const Ref<Node> &node) -> bool {
        if (!node) return true;
        int lsize = node->left ? node->left->size : 0;
        int rsize = node->right ? node->right->size : 0;

        // SBT property: no child subtree is larger than its sibling by more than allowed (usually 1)
        if (node->left && node->left->left && node->left->left->size > rsize)      return false;
        if (node->left && node->left->right && node->left->right->size > rsize)    return false;
        if (node->right && node->right->right && node->right->right->size > lsize) return false;
        if (node->right && node->right->left && node->right->left->size > lsize)   return false;

        return check(node->left) && check(node->right);
    };

    return check(tree.get_root());
}

TEST(SBTTest, SBTBalancedPrint) {
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
        tree.insert(FFDBKey<int>(key), float(key));
    }

    std::cout << "\nIn-order traversal: ";
    tree.in_order([](const FFDBKey<int>& k, float v) { std::cout << std::get<int>(k.key_storage) << " "; });

    EXPECT_TRUE(check_sbt_balance(tree));

    db.commit_txn();
    remove("test.db");
}