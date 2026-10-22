#include <cstdio>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

static const Rgba RED = {255, 0, 0, 255};

/* A 4x2 sprite with n frames; frame i has a pixel at (0, 0) of red level 40 * (i + 1). */
static Sprite frames(int n)
{
    Sprite s(4, 2);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    for (int i = 0; i < n; i++) {
        s.layer(0)->cel(i)->set(0, 0, Rgba{static_cast<uint8_t>(40 * (i + 1)), 0, 0, 255});
    }
    return s;
}

static Rgba at(const SpriteSheet &sheet, int x, int y)
{
    return sheet
        .pixels[static_cast<size_t>(y) * static_cast<size_t>(sheet.width) + static_cast<size_t>(x)];
}

TEST(horizontal_puts_the_frames_side_by_side)
{
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(3), SheetOptions{});
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(12, sheet->width);
    ASSERT_EQ_INT(2, sheet->height);
    ASSERT_EQ_INT(3, static_cast<int>(sheet->frames.size()));
    ASSERT_EQ_INT(4, sheet->frames[1].x);
    ASSERT_EQ_INT(0, sheet->frames[1].y);
    ASSERT_EQ_INT(4, sheet->frames[1].w);
    ASSERT_EQ_INT(2, sheet->frames[1].h);
    ASSERT_EQ_INT(40, at(*sheet, 0, 0).r);
    ASSERT_EQ_INT(80, at(*sheet, 4, 0).r);
    ASSERT_EQ_INT(120, at(*sheet, 8, 0).r);
    ASSERT_EQ_INT(0, at(*sheet, 5, 0).a);
}

TEST(vertical_stacks_them)
{
    SheetOptions o;
    o.layout = SheetLayout::Vertical;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(3), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(4, sheet->width);
    ASSERT_EQ_INT(6, sheet->height);
    ASSERT_EQ_INT(2, sheet->frames[1].y);
    ASSERT_EQ_INT(80, at(*sheet, 0, 2).r);
}

TEST(a_grid_wraps_at_the_column_count)
{
    SheetOptions o;
    o.layout = SheetLayout::Grid;
    o.columns = 2;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(3), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(8, sheet->width);
    ASSERT_EQ_INT(4, sheet->height);
    ASSERT_EQ_INT(0, sheet->frames[2].x);
    ASSERT_EQ_INT(2, sheet->frames[2].y);
    ASSERT_EQ_INT(120, at(*sheet, 0, 2).r);
    ASSERT_EQ_INT(0, at(*sheet, 4, 2).a); /* the empty fourth slot */
}

TEST(a_grid_without_a_column_count_is_roughly_square)
{
    SheetOptions o;
    o.layout = SheetLayout::Grid;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(5), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(12, sheet->width); /* 3 columns */
    ASSERT_EQ_INT(4, sheet->height); /* 2 rows */
}

TEST(spacing_goes_between_frames_only)
{
    SheetOptions o;
    o.spacing = 3;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(2), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(11, sheet->width);
    ASSERT_EQ_INT(7, sheet->frames[1].x);
    ASSERT_EQ_INT(0, at(*sheet, 5, 0).a);
    ASSERT_EQ_INT(80, at(*sheet, 7, 0).r);
}

TEST(scale_enlarges_every_frame)
{
    SheetOptions o;
    o.scale = 2;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(2), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(16, sheet->width);
    ASSERT_EQ_INT(4, sheet->height);
    ASSERT_EQ_INT(8, sheet->frames[1].x);
    ASSERT_EQ_INT(8, sheet->frames[1].w);
    ASSERT_EQ_INT(40, at(*sheet, 1, 1).r);
    ASSERT_EQ_INT(80, at(*sheet, 9, 1).r);
}

TEST(a_range_picks_the_frames)
{
    SheetOptions o;
    o.first = 1;
    o.last = 2;
    std::optional<SpriteSheet> sheet = spritesheet_render(frames(4), o);
    ASSERT_TRUE(sheet.has_value());
    if (!sheet) {
        return;
    }
    ASSERT_EQ_INT(2, static_cast<int>(sheet->frames.size()));
    ASSERT_EQ_INT(80, at(*sheet, 0, 0).r);
    o = SheetOptions{};
    o.first = 2;
    sheet = spritesheet_render(frames(4), o);
    ASSERT_TRUE(sheet.has_value());
    if (sheet) {
        ASSERT_EQ_INT(2, static_cast<int>(sheet->frames.size()));
    }
}

TEST(layers_are_flattened_like_the_canvas_shows_them)
{
    Sprite s = frames(1);
    Layer &top = s.addLayer("Top");
    top.cel(0)->set(1, 0, RED);
    top.setVisible(false);
    std::optional<SpriteSheet> sheet = spritesheet_render(s, SheetOptions{});
    ASSERT_TRUE(sheet.has_value());
    if (sheet) {
        ASSERT_EQ_INT(0, at(*sheet, 1, 0).a);
    }
}

TEST(bad_options_render_nothing)
{
    Sprite s = frames(2);
    SheetOptions o;
    o.first = 3;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
    o = SheetOptions{};
    o.last = 2;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
    o = SheetOptions{};
    o.first = 1;
    o.last = 0;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
    o = SheetOptions{};
    o.scale = 0;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
    o = SheetOptions{};
    o.spacing = -1;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
    o = SheetOptions{};
    o.layout = SheetLayout::Grid;
    o.columns = -2;
    ASSERT_TRUE(!spritesheet_render(s, o).has_value());
}

TEST(a_sheet_beyond_the_size_limit_is_refused)
{
    Sprite s(4000, 4);
    for (int i = 1; i < 6; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    ASSERT_TRUE(!spritesheet_render(s, SheetOptions{}).has_value()); /* 24000 px wide */
    SheetOptions o;
    o.layout = SheetLayout::Vertical;
    ASSERT_TRUE(spritesheet_render(s, o).has_value());
}

TEST(export_writes_an_image_that_reads_back)
{
    const char *path = "test_spritesheet.png";
    ASSERT_TRUE(spritesheet_export(frames(3), path, SheetOptions{}));
    std::optional<Sprite> back = image_import(path);
    ASSERT_TRUE(back.has_value());
    if (back) {
        ASSERT_EQ_INT(12, back->width());
        ASSERT_EQ_INT(2, back->height());
        ASSERT_EQ_INT(80, back->layer(0)->cel(0)->get(4, 0).r);
    }
    std::remove(path);
    ASSERT_TRUE(!spritesheet_export(frames(3), "test_spritesheet.gif", SheetOptions{}));
    ASSERT_TRUE(!spritesheet_export(frames(3), "no_such_dir/x.png", SheetOptions{}));
}

int main()
{
    RUN(horizontal_puts_the_frames_side_by_side);
    RUN(vertical_stacks_them);
    RUN(a_grid_wraps_at_the_column_count);
    RUN(a_grid_without_a_column_count_is_roughly_square);
    RUN(spacing_goes_between_frames_only);
    RUN(scale_enlarges_every_frame);
    RUN(a_range_picks_the_frames);
    RUN(layers_are_flattened_like_the_canvas_shows_them);
    RUN(bad_options_render_nothing);
    RUN(a_sheet_beyond_the_size_limit_is_refused);
    RUN(export_writes_an_image_that_reads_back);
    return TEST_SUMMARY();
}
