#include "FFDB2.h"
#include <fcntl.h>

int main() {
    FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);
    db.start_txn(DB_WRITE_TXN);

    return 0;
}