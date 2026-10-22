#include "minitest.hpp"
#include "tinysprite/tools.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

TEST(pick_cel_reads_the_pixel)
{
    Cel c(4, 4);
    c.set(2, 1, RED);
    std::optional<Rgba> got = eyedropper_pick_cel(c, 2, 1);
    ASSERT_TRUE(got.has_value());
    ASSERT_EQ_INT(255, got.value_or(Rgba{9, 9, 9, 9}).r);
    ASSERT_EQ_INT(255, got.value_or(Rgba{9, 9, 9, 9}).a);
    got = eyedropper_pick_cel(c, 0, 0);
    ASSERT_TRUE(got.has_value());
    ASSERT_EQ_INT(0, got.value_or(Rgba{9, 9, 9, 9}).a);
}

TEST(pick_cel_out_of_bounds_gives_nothing)
{
    Cel c(4, 4);
    ASSERT_TRUE(!eyedropper_pick_cel(c, 4, 0));
    ASSERT_TRUE(!eyedropper_pick_cel(c, 0, -1));
}

TEST(pick_sprite_reads_the_flattened_image)
{
    Sprite s(4, 4);
    Layer *top = &s.addLayer("top");
    s.layer(0)->cel(0)->set(1, 1, BLUE);
    top->cel(0)->set(1, 1, RED);
    std::optional<Rgba> got = eyedropper_pick_sprite(s, 0, 1, 1);
    ASSERT_TRUE(got.has_value());
    ASSERT_EQ_INT(255, got.value_or(Rgba{9, 9, 9, 9}).r);
    ASSERT_EQ_INT(0, got.value_or(Rgba{9, 9, 9, 9}).b);
    top->setVisible(false);
    got = eyedropper_pick_sprite(s, 0, 1, 1);
    ASSERT_TRUE(got.has_value());
    ASSERT_EQ_INT(255, got.value_or(Rgba{9, 9, 9, 9}).b);
    ASSERT_TRUE(!eyedropper_pick_sprite(s, 0, 9, 9));
    ASSERT_TRUE(!eyedropper_pick_sprite(s, 3, 1, 1));
}

int main()
{
    RUN(pick_cel_reads_the_pixel);
    RUN(pick_cel_out_of_bounds_gives_nothing);
    RUN(pick_sprite_reads_the_flattened_image);
    return TEST_SUMMARY();
}
