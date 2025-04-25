#include "FFDB.h"
#include <fcntl.h>
#include <iostream>

int main() {
    FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);
    db.start_transaction();

    return 0;
}