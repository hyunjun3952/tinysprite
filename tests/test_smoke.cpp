#include "minitest.h"

TEST(harness_runs)
{
    ASSERT_EQ_INT(4, 2 + 2);
}

int main(void)
{
    RUN(harness_runs);
    return TEST_SUMMARY();
}
