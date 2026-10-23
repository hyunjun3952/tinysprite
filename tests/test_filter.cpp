#include "minitest.hpp"
#include "tinysprite/filter_ops.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba GREEN = {0, 255, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};
static const Rgba BLACK = {0, 0, 0, 255};
static const Rgba WHITE = {255, 255, 255, 255};

static int opaque_count(const Cel &cel)
{
    int n = 0;
    for (int y = 0; y < cel.height(); y++) {
        for (int x = 0; x < cel.width(); x++) {
            n += cel.get(x, y).a != 0;
        }
    }
    return n;
}

TEST(outline_surrounds_a_pixel_on_its_four_sides)
{
    Cel cel(5, 5);
    cel.set(2, 2, RED);
    History h;
    ASSERT_TRUE(filter_outline(h, cel, BLACK, false));
    ASSERT_TRUE(cel.get(2, 2) == RED);
    ASSERT_TRUE(cel.get(1, 2) == BLACK);
    ASSERT_TRUE(cel.get(3, 2) == BLACK);
    ASSERT_TRUE(cel.get(2, 1) == BLACK);
    ASSERT_TRUE(cel.get(2, 3) == BLACK);
    ASSERT_TRUE(cel.get(1, 1) == Rgba{});
    ASSERT_EQ_INT(5, opaque_count(cel));
}

TEST(outline_with_diagonals_adds_the_corners)
{
    Cel cel(5, 5);
    cel.set(2, 2, RED);
    History h;
    ASSERT_TRUE(filter_outline(h, cel, BLACK, true));
    ASSERT_TRUE(cel.get(1, 1) == BLACK);
    ASSERT_TRUE(cel.get(3, 3) == BLACK);
    ASSERT_EQ_INT(9, opaque_count(cel));
}

TEST(outline_at_the_edge_is_clipped_to_the_cel)
{
    Cel cel(3, 3);
    cel.set(0, 0, RED);
    History h;
    ASSERT_TRUE(filter_outline(h, cel, BLACK, false));
    ASSERT_TRUE(cel.get(1, 0) == BLACK);
    ASSERT_TRUE(cel.get(0, 1) == BLACK);
    ASSERT_EQ_INT(3, opaque_count(cel));
}

TEST(outline_of_an_empty_cel_records_nothing)
{
    Cel cel(3, 3);
    History h;
    ASSERT_TRUE(filter_outline(h, cel, BLACK, true));
    ASSERT_TRUE(!h.canUndo());
}

TEST(outline_only_reads_the_original_pixels)
{
    // The new outline pixels must not grow a second ring.
    Cel cel(7, 7);
    cel.set(3, 3, RED);
    History h;
    filter_outline(h, cel, BLACK, false);
    ASSERT_TRUE(cel.get(1, 3) == Rgba{});
}

TEST(outline_is_one_undo_step)
{
    Cel cel(5, 5);
    cel.set(2, 2, RED);
    History h;
    filter_outline(h, cel, BLACK, false);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, opaque_count(cel));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(5, opaque_count(cel));
}

TEST(outline_stays_inside_the_area)
{
    Cel cel(5, 5);
    cel.set(2, 2, RED);
    History h;
    ASSERT_TRUE(filter_outline(h, cel, BLACK, false, Rect{0, 0, 2, 5}));
    ASSERT_TRUE(cel.get(1, 2) == BLACK);
    ASSERT_TRUE(cel.get(3, 2) == Rgba{});
    ASSERT_EQ_INT(2, opaque_count(cel));
}

TEST(a_filter_is_refused_while_a_stroke_is_active)
{
    Cel cel(3, 3);
    cel.set(1, 1, RED);
    History h;
    h.beginStroke(cel);
    ASSERT_TRUE(!filter_outline(h, cel, BLACK, false));
    ASSERT_TRUE(!filter_replace_color(h, cel, RED, BLUE));
    ASSERT_TRUE(!filter_hue_brightness(h, cel, 30, 0));
    h.cancelStroke();
    ASSERT_EQ_INT(1, opaque_count(cel));
}

TEST(replace_color_swaps_every_exact_match)
{
    Cel cel(3, 1);
    cel.set(0, 0, RED);
    cel.set(1, 0, GREEN);
    cel.set(2, 0, RED);
    History h;
    ASSERT_TRUE(filter_replace_color(h, cel, RED, BLUE));
    ASSERT_TRUE(cel.get(0, 0) == BLUE);
    ASSERT_TRUE(cel.get(1, 0) == GREEN);
    ASSERT_TRUE(cel.get(2, 0) == BLUE);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(cel.get(0, 0) == RED);
    ASSERT_TRUE(cel.get(2, 0) == RED);
}

TEST(replace_color_does_not_match_a_different_alpha)
{
    Cel cel(2, 1);
    cel.set(0, 0, Rgba{255, 0, 0, 128});
    cel.set(1, 0, RED);
    History h;
    filter_replace_color(h, cel, RED, BLUE);
    ASSERT_TRUE(cel.get(0, 0) == Rgba({255, 0, 0, 128}));
    ASSERT_TRUE(cel.get(1, 0) == BLUE);
}

TEST(replace_color_with_itself_records_nothing)
{
    Cel cel(2, 1);
    cel.set(0, 0, RED);
    History h;
    ASSERT_TRUE(filter_replace_color(h, cel, RED, RED));
    ASSERT_TRUE(!h.canUndo());
}

TEST(replace_color_stays_inside_the_area)
{
    Cel cel(3, 1);
    cel.set(0, 0, RED);
    cel.set(2, 0, RED);
    History h;
    filter_replace_color(h, cel, RED, BLUE, Rect{2, 0, 1, 1});
    ASSERT_TRUE(cel.get(0, 0) == RED);
    ASSERT_TRUE(cel.get(2, 0) == BLUE);
}

TEST(hue_shift_turns_red_toward_green)
{
    Cel cel(1, 1);
    cel.set(0, 0, RED);
    History h;
    ASSERT_TRUE(filter_hue_brightness(h, cel, 120, 0));
    ASSERT_TRUE(cel.get(0, 0) == GREEN);
    ASSERT_TRUE(filter_hue_brightness(h, cel, 120, 0));
    ASSERT_TRUE(cel.get(0, 0) == BLUE);
}

TEST(hue_shift_wraps_around)
{
    Cel cel(1, 1);
    cel.set(0, 0, RED);
    History h;
    filter_hue_brightness(h, cel, -120, 0);
    ASSERT_TRUE(cel.get(0, 0) == BLUE);
}

TEST(brightness_extremes_give_black_and_white)
{
    Cel cel(1, 2);
    cel.set(0, 0, RED);
    cel.set(0, 1, RED);
    History h;
    filter_hue_brightness(h, cel, 0, 100);
    ASSERT_TRUE(cel.get(0, 0) == WHITE);
    h.undo();
    filter_hue_brightness(h, cel, 0, -100);
    ASSERT_TRUE(cel.get(0, 0) == BLACK);
}

TEST(brightness_keeps_alpha_and_leaves_transparent_pixels)
{
    Cel cel(2, 1);
    cel.set(0, 0, Rgba{100, 100, 100, 64});
    History h;
    filter_hue_brightness(h, cel, 0, 50);
    ASSERT_EQ_INT(64, cel.get(0, 0).a);
    ASSERT_TRUE(cel.get(0, 0).r > 100);
    ASSERT_TRUE(cel.get(1, 0) == Rgba{});
}

TEST(a_neutral_adjustment_records_nothing)
{
    Cel cel(2, 1);
    cel.set(0, 0, Rgba{12, 200, 99, 255});
    History h;
    ASSERT_TRUE(filter_hue_brightness(h, cel, 0, 0));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(filter_hue_brightness(h, cel, 360, 0));
    ASSERT_TRUE(cel.get(0, 0) == Rgba({12, 200, 99, 255}));
}

TEST(adjustment_is_clamped_to_its_range)
{
    Cel cel(1, 1);
    cel.set(0, 0, RED);
    History h;
    filter_hue_brightness(h, cel, 0, 5000);
    ASSERT_TRUE(cel.get(0, 0) == WHITE);
}

TEST(filter_apply_runs_the_chosen_filter)
{
    Cel cel(3, 1);
    cel.set(1, 0, RED);
    History h;
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    ASSERT_TRUE(filter_apply(h, cel, p));
    ASSERT_TRUE(cel.get(1, 0) == BLUE);
    p.kind = FilterParams::Kind::Outline;
    p.color = GREEN;
    ASSERT_TRUE(filter_apply(h, cel, p));
    ASSERT_TRUE(cel.get(0, 0) == GREEN);
    ASSERT_TRUE(cel.get(2, 0) == GREEN);
    p.kind = FilterParams::Kind::HueBrightness;
    p.hue = 120;
    ASSERT_TRUE(filter_apply(h, cel, p));
    ASSERT_TRUE(cel.get(1, 0) == RED);
}

TEST(filtering_several_cels_is_one_undo_step)
{
    Cel a(2, 1), b(2, 1);
    a.set(0, 0, RED);
    b.set(1, 0, RED);
    History h;
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    ASSERT_TRUE(filter_apply_cels(h, {&a, &b}, p));
    ASSERT_TRUE(a.get(0, 0) == BLUE);
    ASSERT_TRUE(b.get(1, 0) == BLUE);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(a.get(0, 0) == RED);
    ASSERT_TRUE(b.get(1, 0) == RED);
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(a.get(0, 0) == BLUE);
    ASSERT_TRUE(b.get(1, 0) == BLUE);
}

TEST(filtering_cels_that_do_not_change_records_nothing)
{
    Cel a(2, 1);
    History h;
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    ASSERT_TRUE(filter_apply_cels(h, {&a}, p));
    ASSERT_TRUE(!h.canUndo());
}

TEST(filtering_cels_is_refused_while_a_stroke_is_active)
{
    Cel a(2, 1);
    a.set(0, 0, RED);
    History h;
    h.beginStroke(a);
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    ASSERT_TRUE(!filter_apply_cels(h, {&a}, p));
    h.cancelStroke();
}

TEST(filtering_cels_pixel_steps_interleave)
{
    Cel a(2, 1);
    a.set(0, 0, RED);
    History h;
    FilterParams p;
    p.kind = FilterParams::Kind::ReplaceColor;
    p.from = RED;
    p.to = BLUE;
    filter_apply_cels(h, {&a}, p);
    h.beginStroke(a);
    h.setPixel(1, 0, GREEN);
    h.endStroke();
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(a.get(0, 0) == BLUE);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(a.get(0, 0) == RED);
}

int main()
{
    RUN(outline_surrounds_a_pixel_on_its_four_sides);
    RUN(outline_with_diagonals_adds_the_corners);
    RUN(outline_at_the_edge_is_clipped_to_the_cel);
    RUN(outline_of_an_empty_cel_records_nothing);
    RUN(outline_only_reads_the_original_pixels);
    RUN(outline_is_one_undo_step);
    RUN(outline_stays_inside_the_area);
    RUN(a_filter_is_refused_while_a_stroke_is_active);
    RUN(replace_color_swaps_every_exact_match);
    RUN(replace_color_does_not_match_a_different_alpha);
    RUN(replace_color_with_itself_records_nothing);
    RUN(replace_color_stays_inside_the_area);
    RUN(hue_shift_turns_red_toward_green);
    RUN(hue_shift_wraps_around);
    RUN(brightness_extremes_give_black_and_white);
    RUN(brightness_keeps_alpha_and_leaves_transparent_pixels);
    RUN(a_neutral_adjustment_records_nothing);
    RUN(adjustment_is_clamped_to_its_range);
    RUN(filter_apply_runs_the_chosen_filter);
    RUN(filtering_several_cels_is_one_undo_step);
    RUN(filtering_cels_that_do_not_change_records_nothing);
    RUN(filtering_cels_is_refused_while_a_stroke_is_active);
    RUN(filtering_cels_pixel_steps_interleave);
    return TEST_SUMMARY();
}
