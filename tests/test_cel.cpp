#include <stdexcept>

#include "minitest.h"
#include "tinysprite/cel.h"

TEST(create_is_transparent)
{
    Cel c(3, 2);
    ASSERT_EQ_INT(3, c.width());
    ASSERT_EQ_INT(2, c.height());
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 3; x++) {
            Rgba p = c.get(x, y);
            ASSERT_EQ_INT(0, p.r);
            ASSERT_EQ_INT(0, p.g);
            ASSERT_EQ_INT(0, p.b);
            ASSERT_EQ_INT(0, p.a);
        }
    }
}

TEST(create_rejects_invalid_size)
{
    ASSERT_THROWS(Cel(0, 8), std::invalid_argument);
    ASSERT_THROWS(Cel(8, 0), std::invalid_argument);
    ASSERT_THROWS(Cel(-1, 8), std::invalid_argument);
    ASSERT_THROWS(Cel(CEL_MAX_DIMENSION + 1, 8), std::invalid_argument);
    ASSERT_THROWS(Cel(8, CEL_MAX_DIMENSION + 1), std::invalid_argument);
}

TEST(set_then_get_roundtrip)
{
    Cel c(4, 4);
    Rgba red = {255, 0, 0, 255};
    ASSERT_TRUE(c.set(2, 3, red));
    Rgba got = c.get(2, 3);
    ASSERT_EQ_INT(255, got.r);
    ASSERT_EQ_INT(255, got.a);
    ASSERT_EQ_INT(0, c.get(3, 2).a);
}

TEST(out_of_bounds_is_rejected)
{
    Cel c(4, 4);
    Rgba red = {255, 0, 0, 255};
    ASSERT_TRUE(!c.set(4, 0, red));
    ASSERT_TRUE(!c.set(0, 4, red));
    ASSERT_TRUE(!c.set(-1, 0, red));
    ASSERT_EQ_INT(0, c.get(-1, -1).a);
    ASSERT_EQ_INT(0, c.get(4, 4).a);
    ASSERT_TRUE(!c.inBounds(4, 0));
    ASSERT_TRUE(c.inBounds(3, 3));
}

TEST(pixels_are_row_major)
{
    Cel c(3, 2);
    Rgba green = {0, 255, 0, 255};
    c.set(1, 1, green);
    ASSERT_EQ_INT(255, c.pixels()[1 * 3 + 1].g);
}

TEST(index_of_is_row_major)
{
    Cel c(3, 2);
    ASSERT_EQ_INT(0, static_cast<int>(c.indexOf(0, 0)));
    ASSERT_EQ_INT(2, static_cast<int>(c.indexOf(2, 0)));
    ASSERT_EQ_INT(4, static_cast<int>(c.indexOf(1, 1)));
    Rgba green = {0, 255, 0, 255};
    c.set(2, 1, green);
    ASSERT_EQ_INT(255, c.pixels()[c.indexOf(2, 1)].g);
}

int main(void)
{
    RUN(create_is_transparent);
    RUN(create_rejects_invalid_size);
    RUN(set_then_get_roundtrip);
    RUN(out_of_bounds_is_rejected);
    RUN(pixels_are_row_major);
    RUN(index_of_is_row_major);
    return TEST_SUMMARY();
}
