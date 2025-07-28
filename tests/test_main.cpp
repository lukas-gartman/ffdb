#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    remove("test.db"); // Clean up any previous failed test database
    return RUN_ALL_TESTS();
}
