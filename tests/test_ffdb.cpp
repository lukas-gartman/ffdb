#include <gtest/gtest.h>
#include <random>
#include <fcntl.h>
#include <pthread.h>
#include <vector>
#include "../src/FFDB.h"
#include "../src/BTreeFunctional.h"
#include <sys/stat.h>

using BT = BTreeFunctional<int, int, 3>;
using BTRef = BTreeRef<int, int, 3>;

int reader_val = 0;

FFDB open_db() {
    return FFDB("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
}

void init_db() {
    remove("test.db");
    FFDB db = open_db();

    // 0 as db is created here
    db.start_txn(DB_WRITE_TXN);

    auto tree = BT::empty();
    for (int i = 0; i < 10; i++) {
        tree = BT::insert(tree, i, i);
    }

    db.commit_txn(tree.as_offset());
}

void* writer_fun(void* arg) {
    auto db = open_db();
    BTRef tree = db.start_txn(DB_WRITE_TXN);
    tree = BT::replace_value(tree, 1, 111);
    db.commit_txn(tree.as_offset());
    return nullptr;
}

void* reader_fun(void* arg) {
    auto db = open_db();
    BTRef tree = db.start_txn(DB_READ_TXN);
    sleep(1);
    reader_val = BT::get_value(tree, 1).value_or(0);
    db.commit_txn();
    return nullptr;
}

void print_db() {
    auto db = open_db();
    BTRef tree = db.start_txn(DB_WRITE_TXN);
    BT::debug_print(tree);
    db.commit_txn(tree.as_offset());
}

TEST(FFDBTest, TestTest) {
    struct stat fstat1{};
    struct stat fstat2{};

    init_db();

    pthread_t thr_writer, thr_reader;

    pthread_create(&thr_writer, nullptr, writer_fun, nullptr);
    pthread_create(&thr_reader, nullptr, reader_fun, nullptr);

    pthread_join(thr_writer, nullptr);
    pthread_join(thr_reader, nullptr);

    stat("test.db", &fstat1);

    FFDB db = open_db();
    BTRef tree = db.start_txn(DB_WRITE_TXN);
    int writer_val = BT::get_value(tree, 1).value_or(0);
    for (int i = 0; i < 1000; i++) {
        tree = BT::insert(tree, i + 10, i + 10);
    }
    db.commit_txn(tree.as_offset());

    stat("test.db", &fstat2);

    // Check so that reader value was read from a previous revision
    ASSERT_NE(writer_val, reader_val);

    // Check so reader does not still hold resources by resizing db.
    ASSERT_NE(fstat1.st_size, fstat2.st_size);

    remove("test.db");
}


// Test rollback: Do all kinds of operations and commit, do all kinds of operations and rollback. Verify that
// The first transaction is intact and the second transaction is rolled back.

// Test concurrent transactions: Need to test readonly transactions in different ways.

// Test block descr system in some way

/*
Att göra:
* Implementera läs transaktioner.


*/