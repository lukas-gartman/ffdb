#include <gtest/gtest.h>
#include "../FixedArray.h"

TEST(FixedArrayTest, Initialization) {
    FixedArray<int> arr(10);
    EXPECT_EQ(arr.size(), 0);
    EXPECT_EQ(arr.capacity(), 10);
}

TEST(FixedArrayTest, AddAndAccess) {
    FixedArray<int> arr(5);
    arr.resize(3);
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;

    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(FixedArrayTest, OutOfRangeAccess) {
    FixedArray<int> arr(5);
    EXPECT_THROW(arr[10], std::out_of_range);
}

TEST(FixedArrayTest, Resize) {
    FixedArray<int> arr(5);
    EXPECT_THROW(arr[5] = 1, std::out_of_range);
    arr.resize(6);
    arr[5] = 1;
    EXPECT_EQ(arr[5], 1);
}

TEST(FixedArrayTest, ResizeUpAndDown) {
    FixedArray<int> arr(5);
    EXPECT_THROW(arr[5], std::out_of_range);
    arr.resize(10);
    arr[5] = 1;
    EXPECT_EQ(arr[5], 1);
    arr.resize(3);
    EXPECT_THROW(arr[6], std::out_of_range);
}

TEST(FixedArrayTest, ShiftingKeys) {
    FixedArray<int> arr(5);
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    arr[4] = 5;

    // Shift keys to the right
    for (int i = 4; i > 0; --i) {
        arr[i] = arr[i - 1];
    }
    arr[0] = 0;

    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 1);
    EXPECT_EQ(arr[2], 2);
    EXPECT_EQ(arr[3], 3);
    EXPECT_EQ(arr[4], 4);
}

TEST(FixedArrayTest, Size) {
    FixedArray<int> arr(5);
    EXPECT_EQ(arr.size(), 0);
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    EXPECT_EQ(arr.size(), 3);
    arr.resize(2);
    EXPECT_EQ(arr.size(), 2);
}

TEST(FixedArrayTest, Erase) {
    FixedArray<int> arr(5);
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;

    int deletedElement = arr.erase(1);
    EXPECT_EQ(deletedElement, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 3);
}

TEST(FixedArrayTest, MoveConstructor) {
    FixedArray<int> arr1(5);
    arr1[0] = 1;
    arr1[1] = 2;
    arr1[2] = 3;

    FixedArray<int> arr2(std::move(arr1));
    EXPECT_EQ(arr2[0], 1);
    EXPECT_EQ(arr2[1], 2);
    EXPECT_EQ(arr2[2], 3);
}

TEST(FixedArrayTest, MoveAssignment) {
    FixedArray<int> arr1(5);
    arr1[0] = 1;
    arr1[1] = 2;
    arr1[2] = 3;

    FixedArray<int> arr2(10);
    arr2 = std::move(arr1);
    EXPECT_EQ(arr2[0], 1);
    EXPECT_EQ(arr2[1], 2);
    EXPECT_EQ(arr2[2], 3);
}
