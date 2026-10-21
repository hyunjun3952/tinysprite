#include "minitest.h"
#include "tinysprite/cel.h"

TEST(create_is_transparent)
{
    Cel *c = cel_create(3, 2);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ_INT(3, cel_width(c));
    ASSERT_EQ_INT(2, cel_height(c));
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 3; x++) {
            Rgba p = cel_get(c, x, y);
            ASSERT_EQ_INT(0, p.r);
            ASSERT_EQ_INT(0, p.g);
            ASSERT_EQ_INT(0, p.b);
            ASSERT_EQ_INT(0, p.a);
        }
    }
    cel_destroy(c);
}

TEST(create_rejects_invalid_size)
{
    ASSERT_TRUE(cel_create(0, 8) == NULL);
    ASSERT_TRUE(cel_create(8, 0) == NULL);
    ASSERT_TRUE(cel_create(-1, 8) == NULL);
    ASSERT_TRUE(cel_create(CEL_MAX_DIMENSION + 1, 8) == NULL);
    ASSERT_TRUE(cel_create(8, CEL_MAX_DIMENSION + 1) == NULL);
}

TEST(destroy_null_is_safe)
{
    cel_destroy(NULL);
}

TEST(set_then_get_roundtrip)
{
    Cel *c = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255};
    ASSERT_EQ_INT(0, cel_set(c, 2, 3, red));
    Rgba got = cel_get(c, 2, 3);
    ASSERT_EQ_INT(255, got.r);
    ASSERT_EQ_INT(255, got.a);
    ASSERT_EQ_INT(0, cel_get(c, 3, 2).a);
    cel_destroy(c);
}

TEST(out_of_bounds_is_rejected)
{
    Cel *c = cel_create(4, 4);
    Rgba red = {255, 0, 0, 255};
    ASSERT_EQ_INT(-1, cel_set(c, 4, 0, red));
    ASSERT_EQ_INT(-1, cel_set(c, 0, 4, red));
    ASSERT_EQ_INT(-1, cel_set(c, -1, 0, red));
    ASSERT_EQ_INT(0, cel_get(c, -1, -1).a);
    ASSERT_EQ_INT(0, cel_get(c, 4, 4).a);
    ASSERT_TRUE(!cel_in_bounds(c, 4, 0));
    ASSERT_TRUE(cel_in_bounds(c, 3, 3));
    cel_destroy(c);
}

TEST(pixels_are_row_major)
{
    Cel *c = cel_create(3, 2);
    Rgba green = {0, 255, 0, 255};
    cel_set(c, 1, 1, green);
    ASSERT_EQ_INT(255, cel_pixels(c)[1 * 3 + 1].g);
    cel_destroy(c);
}

int main(void)
{
    RUN(create_is_transparent);
    RUN(create_rejects_invalid_size);
    RUN(destroy_null_is_safe);
    RUN(set_then_get_roundtrip);
    RUN(out_of_bounds_is_rejected);
    RUN(pixels_are_row_major);
    return TEST_SUMMARY();
}
