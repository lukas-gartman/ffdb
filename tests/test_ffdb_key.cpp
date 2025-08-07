#include <gtest/gtest.h>
#include "../src/FFDBKey.h"
#include "../src/FFDB2.h"
#include <string>
#include <array>
#include <cstring>
#include <fcntl.h>

TEST(FFDBKeyTest, BasicKeyCreationAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);
    int key1 = 11223344;
    int key2 = 11223345;
    FFDBKey<int> k1(key1);
    FFDBKey<int> k2(key2);

    EXPECT_TRUE(std::get<int>(k1.key_storage) == key1);
    EXPECT_TRUE(std::get<int>(k2.key_storage) == key2);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ArrayKeyPrefixAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr1[4] = {1,2,3,4};
    int arr2[4] = {1,2,3,5};
    FFDBKey<int> k1(arr1, 4);
    FFDBKey<int> k2(arr2, 4);

    const auto& lk1 = std::get<FFDBKey<int>::LargeKey>(k1.key_storage);
    const auto& lk2 = std::get<FFDBKey<int>::LargeKey>(k2.key_storage);

    EXPECT_EQ(std::memcmp(lk1.key_prefix.data(), arr1, PREFIX_SIZE), 0);
    EXPECT_EQ(std::memcmp(lk2.key_prefix.data(), arr2, PREFIX_SIZE), 0);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, StringKeyPrefixAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    char s1[] = "aaaaaaa a";
    char s2[] = "aaaaaaa b";
    FFDBKey<char> k1(s1, strlen(s1));
    FFDBKey<char> k2(s2, strlen(s2));

    const auto& lk1 = std::get<FFDBKey<char>::LargeKey>(k1.key_storage);
    const auto& lk2 = std::get<FFDBKey<char>::LargeKey>(k2.key_storage);

    EXPECT_EQ(std::memcmp(lk1.key_prefix.data(), s1, PREFIX_SIZE), 0);
    EXPECT_EQ(std::memcmp(lk2.key_prefix.data(), s2, PREFIX_SIZE), 0);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, PrefixTruncationForLongKeys) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::array<uint8_t, 32> long_key;
    for (size_t i = 0; i < long_key.size(); ++i) long_key[i] = static_cast<uint8_t>(i);

    FFDBKey<uint8_t> k(long_key.data(), long_key.size());
    const auto& lk = std::get<FFDBKey<uint8_t>::LargeKey>(k.key_storage);

    EXPECT_EQ(std::memcmp(lk.key_prefix.data(), long_key.data(), PREFIX_SIZE), 0);
    EXPECT_EQ(lk.size, 32);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, EmptyArrayKey) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr[0] = {};
    FFDBKey<int> k(arr, 0);

    EXPECT_TRUE(std::get<int>(k.key_storage) == int{});

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ComparisonWithIdenticalKeys) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr[4] = {42, 43, 44, 45};
    FFDBKey<int> k1(arr, 4);
    FFDBKey<int> k2(arr, 4);

    EXPECT_TRUE(k1 == k2);
    EXPECT_FALSE(k1 != k2);
    EXPECT_FALSE(k1 < k2);
    EXPECT_FALSE(k1 > k2);
    EXPECT_TRUE(k1 <= k2);
    EXPECT_TRUE(k1 >= k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ComparisonWithDifferentSizes) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr1[2] = {1,2};
    int arr2[4] = {1,2,3,4};
    int arr3[4] = {5,6,7,8};
    int arr4[2] = {5,6};

    FFDBKey<int> k1(arr1, 2);
    FFDBKey<int> k2(arr2, 4);
    FFDBKey<int> k3(arr3, 4);
    FFDBKey<int> k4(arr4, 2);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_TRUE(k3 > k4);
    EXPECT_TRUE(k4 < k3);
    EXPECT_TRUE(k3 > k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ScalarDoubleKey) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    double d1 = 3.14159;
    double d2 = 2.71828;
    FFDBKey<double> k1(d1);
    FFDBKey<double> k2(d2);

    EXPECT_TRUE(std::get<double>(k1.key_storage) == d1);
    EXPECT_TRUE(std::get<double>(k2.key_storage) == d2);

    EXPECT_TRUE(k2 < k1);
    EXPECT_TRUE(k1 > k2);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, VeryLongArrayKeyComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::array<uint8_t, 256> key1;
    std::array<uint8_t, 256> key2;
    for (size_t i = 0; i < 256; ++i) {
        key1[i] = static_cast<uint8_t>(i);
        key2[i] = static_cast<uint8_t>(i);
    }
    key1[255] = 5; // make key2 slightly different

    FFDBKey<uint8_t> k1(key1.data(), key1.size());
    FFDBKey<uint8_t> k2(key2.data(), key2.size());

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, VeryLongStringKeyComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::string s1(512, 'a');
    std::string s2(512, 'a');
    s2[511] = 'b'; // last char different

    FFDBKey<char> k1(s1.data(), s1.size());
    FFDBKey<char> k2(s2.data(), s2.size());

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, LongKeyPrefixCorrectness) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::array<int, 128> arr;
    for (size_t i = 0; i < arr.size(); ++i) arr[i] = static_cast<int>(i * 2);

    FFDBKey<int> k(arr.data(), arr.size());
    const auto& lk = std::get<FFDBKey<int>::LargeKey>(k.key_storage);

    EXPECT_EQ(std::memcmp(lk.key_prefix.data(), arr.data(), PREFIX_SIZE), 0);
    EXPECT_EQ(lk.size / sizeof(int), arr.size());

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, LongKeyComparisonWithShortKey) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::array<int, 64> long_arr;
    for (size_t i = 0; i < long_arr.size(); ++i) long_arr[i] = 1;

    int short_arr[2] = {1, 1};

    FFDBKey<int> long_key(long_arr.data(), long_arr.size());
    FFDBKey<int> short_key(short_arr, 2);

    EXPECT_TRUE(short_key < long_key);
    EXPECT_TRUE(long_key > short_key);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, LongKeyIdenticalComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    std::vector<uint8_t> key_data(1024, 42);
    FFDBKey<uint8_t> k1(key_data.data(), key_data.size());
    FFDBKey<uint8_t> k2(key_data.data(), key_data.size());

    EXPECT_TRUE(k1 == k2);
    EXPECT_FALSE(k1 != k2);
    EXPECT_FALSE(k1 < k2);
    EXPECT_FALSE(k1 > k2);

    db.commit_txn();
    remove("test.db");
}
