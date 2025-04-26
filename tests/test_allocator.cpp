#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/mman.h>

#include "../src/Allocator.h" // Should include typedefs.h
#include "../src/FFDB.h"

/* MallocTestSeq
 * When allocating memory when no segmentation is present, the memory should be
 * allocated at the top of the memory map.
 */

TEST(AllocatorTest, MallocTestSeq) {
    // Interact with DB
    {
        FFDB db(std::string("test.db"), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1024);

        db.start_transaction();

        db.commit_transaction();
    }

    // Check that db file is correctly

    remove("test.db");
}