#include <gtest/gtest.h>
#include "../src/FFDBKey.h"
#include "../src/FFDB.h"
#include <string>
#include <array>
#include <cstring>
#include <fcntl.h>

TEST(FFDBKeyTest, TESTETS) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    FFDBKey<char> k1(std::string("abc"));

    std::cout << (k1 == "abc") << std::endl;

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, CpyTestInt) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);
    FFDBKey<int> k(1);
    EXPECT_EQ(k, 1);
    FFDBKey<int> *kptr1 = &k;

    k = 5;
    FFDBKey<int> *kptr2 = &k;
    EXPECT_EQ(kptr1, kptr2);
    EXPECT_FALSE(k == 1);
    EXPECT_TRUE(k == 5);

    int arr1[] = {1, 2, 3, 4, 5};
    k = arr1;
    FFDBKey<int> *kptr3 = &k;
    EXPECT_EQ(kptr2, kptr3);
    EXPECT_FALSE(k == 1);
    EXPECT_FALSE(k == 5);
    EXPECT_TRUE(k == arr1);

    k = FFDBKey<int>(67);
    FFDBKey<int> *kptr4 = &k;
    EXPECT_EQ(kptr3, kptr4);
    EXPECT_FALSE(k == 1);
    EXPECT_FALSE(k == 5);
    EXPECT_FALSE(k == arr1);
    EXPECT_TRUE(k == 67);

    int arr2[] = {3, 5, 6, 7, 8, 9, 0};
    k = FFDBKey<int>(arr2);
    FFDBKey<int> *kptr5 = &k;
    EXPECT_EQ(kptr4, kptr5);
    EXPECT_FALSE(k == 1);
    EXPECT_FALSE(k == 5);
    EXPECT_FALSE(k == arr1);
    EXPECT_FALSE(k == 67);
    EXPECT_TRUE(k == arr2);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, BasicKeyCreationAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);
    int key1 = 11223344;
    int key2 = 11223345;
    FFDBKey<int> k1(key1);
    FFDBKey<int> k2(key2);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    EXPECT_TRUE(k1 < key2);
    EXPECT_TRUE(k2 > key1);
    EXPECT_FALSE(k1 == key2);
    EXPECT_TRUE(k1 != key2);
    EXPECT_FALSE(k2 == key1);
    EXPECT_TRUE(k2 != key1);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ArrayKeyPrefixAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr1[4] = {1,2,3,4};
    int arr2[4] = {1,2,3,5};
    FFDBKey<int> k1(arr1);
    FFDBKey<int> k2(arr2);

    const auto& lk1 = std::get<FFDBKey<int>::LargeKey>(k1.key_storage);
    const auto& lk2 = std::get<FFDBKey<int>::LargeKey>(k2.key_storage);

    EXPECT_EQ(std::memcmp(lk1.key_prefix, arr1, PREFIX_SIZE), 0);
    EXPECT_EQ(std::memcmp(lk2.key_prefix, arr2, PREFIX_SIZE), 0);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    EXPECT_TRUE(k1 < arr2);
    EXPECT_TRUE(k2 > arr1);
    EXPECT_FALSE(k1 == arr2);
    EXPECT_TRUE(k1 != arr2);
    EXPECT_FALSE(k2 == arr1);
    EXPECT_TRUE(k2 != arr1);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, StringKeyPrefixAndComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    char s1[] = "aaaaaaa a";
    char s2[] = "aaaaaaa b";
    FFDBKey<char> k1(s1);
    FFDBKey<char> k2(s2);

    const auto& lk1 = std::get<FFDBKey<char>::LargeKey>(k1.key_storage);
    const auto& lk2 = std::get<FFDBKey<char>::LargeKey>(k2.key_storage);

    EXPECT_EQ(std::memcmp(lk1.key_prefix, s1, PREFIX_SIZE), 0);
    EXPECT_EQ(std::memcmp(lk2.key_prefix, s2, PREFIX_SIZE), 0);

    EXPECT_TRUE(k1 < k2);
    EXPECT_TRUE(k2 > k1);
    EXPECT_FALSE(k1 == k2);
    EXPECT_TRUE(k1 != k2);

    EXPECT_TRUE(k1 < s2);
    EXPECT_TRUE(k2 > s1);
    EXPECT_FALSE(k1 == s2);
    EXPECT_TRUE(k1 != s2);
    EXPECT_FALSE(k2 == s1);
    EXPECT_TRUE(k2 != s1);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, PrefixTruncationForLongKeys) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    const Size size = 32;

    uint8_t long_key[size];
    for (Size i = 0; i < size; ++i) long_key[i] = i;

    FFDBKey<uint8_t> k(long_key);
    const auto& lk = std::get<FFDBKey<uint8_t>::LargeKey>(k.key_storage);

    EXPECT_EQ(std::memcmp(lk.key_prefix, long_key, PREFIX_SIZE), 0);
    EXPECT_EQ(k.size, size);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, ComparisonWithIdenticalKeys) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int arr[4] = {42, 43, 44, 45};
    FFDBKey<int> k1(arr);
    FFDBKey<int> k2(arr);

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

    FFDBKey<int> k1(arr1);
    FFDBKey<int> k2(arr2);
    FFDBKey<int> k3(arr3);
    FFDBKey<int> k4(arr4);

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

    uint8_t key1[256];
    uint8_t key2[256];
    for (size_t i = 0; i < 256; ++i) {
        key1[i] = i;
        key2[i] = i;
    }
    key1[255] = 5; // make key2 slightly different

    FFDBKey<uint8_t> k1(key1);
    FFDBKey<uint8_t> k2(key2);

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

    char s1[512] = {"a"};
    char s2[512] = {"a"};
    s2[511] = 'b'; // last char different

    FFDBKey<char> k1(s1);
    FFDBKey<char> k2(s2);

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

    int arr[128];
    for (size_t i = 0; i < 128; ++i) arr[i] = i * 2;

    FFDBKey<int> k(arr);
    const auto& lk = std::get<FFDBKey<int>::LargeKey>(k.key_storage);

    EXPECT_EQ(std::memcmp(lk.key_prefix, arr, PREFIX_SIZE), 0);
    EXPECT_EQ(k.size / sizeof(int), 128);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, LongKeyComparisonWithShortKey) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    int long_arr[64];
    for (size_t i = 0; i < 64; ++i) long_arr[i] = 1;

    int short_arr[2] = {1, 1};

    FFDBKey<int> long_key(long_arr);
    FFDBKey<int> short_key(short_arr);

    EXPECT_TRUE(short_key < long_key);
    EXPECT_TRUE(long_key > short_key);

    db.commit_txn();
    remove("test.db");
}

TEST(FFDBKeyTest, LongKeyIdenticalComparison) {
    remove("test.db");
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    db.start_txn(DB_WRITE_TXN);

    uint8_t key_data[1024];
    for (uint8_t &i : key_data) i = 4;
    FFDBKey<uint8_t> k1(key_data);
    FFDBKey<uint8_t> k2(key_data);

    EXPECT_TRUE(k1 == k2);
    EXPECT_FALSE(k1 != k2);
    EXPECT_FALSE(k1 < k2);
    EXPECT_FALSE(k1 > k2);

    db.commit_txn();
    remove("test.db");
}
