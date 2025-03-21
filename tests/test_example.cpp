#include <gtest/gtest.h>
#include "../typedefs.h"

TEST(TypedefsTest, ExampleTest) {
    Object o = 0;
    Ref<int> ref = o;
    ASSERT_EQ(ref.as_object(), o);
}