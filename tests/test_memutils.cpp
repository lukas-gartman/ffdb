#include <gtest/gtest.h>
#include "../src/Typedefs.h"
#include "../src/Memutils.h"

TEST(MemutilsTest, TestBytesLeft1) {
    for (int i = 0; i < 4096 * 4; i++) {
        EXPECT_EQ(bytes_left_in_page(i, 4096), (4096 - i % 4096));
    }
}
