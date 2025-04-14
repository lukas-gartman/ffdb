#include "FFDB.h"
#include <fcntl.h>

int main() {
    FFDB db("test.db", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);

    return 0;
}