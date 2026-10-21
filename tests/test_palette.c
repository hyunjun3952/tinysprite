#include "minitest.h"
#include "tinysprite/palette.h"

TEST(create_is_empty)
{
    Palette *p = palette_create();
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ_INT(0, palette_count(p));
    palette_destroy(p);
}

TEST(destroy_null_is_safe)
{
    palette_destroy(NULL);
}

TEST(add_returns_sequential_indices)
{
    Palette *p = palette_create();
    Rgba red = {255, 0, 0, 255};
    Rgba blue = {0, 0, 255, 255};
    ASSERT_EQ_INT(0, palette_add(p, red));
    ASSERT_EQ_INT(1, palette_add(p, blue));
    ASSERT_EQ_INT(2, palette_count(p));
    ASSERT_EQ_INT(255, palette_get(p, 0).r);
    ASSERT_EQ_INT(255, palette_get(p, 1).b);
    palette_destroy(p);
}

TEST(get_out_of_range_is_transparent)
{
    Palette *p = palette_create();
    Rgba c = palette_get(p, 0);
    ASSERT_EQ_INT(0, c.a);
    c = palette_get(p, -1);
    ASSERT_EQ_INT(0, c.a);
    palette_destroy(p);
}

TEST(set_replaces_color)
{
    Palette *p = palette_create();
    Rgba red = {255, 0, 0, 255};
    Rgba green = {0, 255, 0, 255};
    palette_add(p, red);
    ASSERT_EQ_INT(0, palette_set(p, 0, green));
    ASSERT_EQ_INT(255, palette_get(p, 0).g);
    ASSERT_EQ_INT(0, palette_get(p, 0).r);
    ASSERT_EQ_INT(-1, palette_set(p, 1, green));
    ASSERT_EQ_INT(-1, palette_set(p, -1, green));
    palette_destroy(p);
}

TEST(add_fails_when_full)
{
    Palette *p = palette_create();
    Rgba c = {1, 2, 3, 255};
    for (int i = 0; i < PALETTE_MAX_COLORS; i++) {
        ASSERT_EQ_INT(i, palette_add(p, c));
    }
    ASSERT_EQ_INT(-1, palette_add(p, c));
    ASSERT_EQ_INT(PALETTE_MAX_COLORS, palette_count(p));
    palette_destroy(p);
}

int main(void)
{
    RUN(create_is_empty);
    RUN(destroy_null_is_safe);
    RUN(add_returns_sequential_indices);
    RUN(get_out_of_range_is_transparent);
    RUN(set_replaces_color);
    RUN(add_fails_when_full);
    return TEST_SUMMARY();
}
