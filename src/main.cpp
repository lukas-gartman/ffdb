#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>

#include "BTreeFunctional.h"

using BT = BTreeFunctional<char, int, 3>;
using BTRef = BTreeRef<char, int, 3>;

int increment(int count) {
    return count + 1;
}

int main() {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 4096 * 16);

    BTRef tree = db.start_txn(DB_WRITE_TXN);
    if (!tree) {
        tree = BT::empty();
        tree = BT::insert(tree, "count", 1);
    } else if (!BT::key_exists(tree, "count")) {
        tree = BT::insert(tree, "count", 1);
    } else {
        tree = BT::update_value(tree, "count", increment);
    }

    int count = BT::get_value(tree, "count").value_or(0);
    db.commit_txn(tree.as_offset());

    std::cout << count << '\n';

    return 0;
}
