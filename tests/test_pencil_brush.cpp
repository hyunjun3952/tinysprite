#include <cstring>

#include <vector>

#include "minitest.h"
#include "tinysprite/raster.h"
#include "tinysprite/tools.h"

#define W 8
#define H 5

static const Rgba RED = {255, 0, 0, 255};

static void expect_rows(const Cel &c, const char *const golden[H])
{
    for (int y = 0; y < H; y++) {
        char row[W + 1];
        for (int x = 0; x < W; x++) {
            row[x] = c.get(x, y).a != 0 ? '#' : '.';
        }
        row[W] = '\0';
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

/* A one-pixel click with the brush at (x, y). */
static Cel dab(PencilOptions options, int x, int y)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, options, x, y);
    s->end();
    return c;
}

TEST(size_zero_and_one_are_a_single_pixel)
{
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    Cel c = dab(PencilOptions{}, 3, 2);
    expect_rows(c, golden);
    c = dab(PencilOptions{false, Symmetry{}, 1}, 3, 2);
    expect_rows(c, golden);
}

TEST(even_square_brush_grows_right_and_down)
{
    Cel c = dab(PencilOptions{false, Symmetry{}, 2}, 3, 2);
    const char *golden[H] = {"........", "........", "...##...", "...##...", "........"};
    expect_rows(c, golden);
}

TEST(odd_square_brush_is_centered)
{
    Cel c = dab(PencilOptions{false, Symmetry{}, 3}, 3, 2);
    const char *golden[H] = {"........", "..###...", "..###...", "..###...", "........"};
    expect_rows(c, golden);
}

TEST(round_brush_rounds_the_corners)
{
    Cel c = dab(PencilOptions{false, Symmetry{}, 3, true}, 3, 2);
    const char *three[H] = {"........", "...#....", "..###...", "...#....", "........"};
    expect_rows(c, three);

    c = dab(PencilOptions{false, Symmetry{}, 4, true}, 3, 2);
    const char *four[H] = {"........", "...##...", "..####..", "..####..", "...##..."};
    expect_rows(c, four);

    c = dab(PencilOptions{false, Symmetry{}, 2, true}, 3, 2);
    const char *two[H] = {"........", "........", "...##...", "...##...", "........"};
    expect_rows(c, two);
}

TEST(brush_is_clipped_at_the_cel_edge)
{
    Cel c = dab(PencilOptions{false, Symmetry{}, 3}, 0, 0);
    const char *golden[H] = {"##......", "##......", "........", "........", "........"};
    expect_rows(c, golden);
}

TEST(dragging_sweeps_the_brush_along_the_line)
{
    History h;
    Cel c(W, H);
    auto s = PencilStroke::begin(h, c, RED, PencilOptions{false, Symmetry{}, 2}, 1, 2);
    s->extend(5, 2);
    s->end();
    const char *golden[H] = {"........", "........", ".######.", ".######.", "........"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    const char *blank[H] = {"........", "........", "........", "........", "........"};
    expect_rows(c, blank);
}

TEST(mirrored_brush_reflects_every_pixel_of_the_stamp)
{
    Cel c = dab(PencilOptions{false, Symmetry{true}, 2}, 1, 1);
    const char *golden[H] = {"........", ".##..##.", ".##..##.", "........", "........"};
    expect_rows(c, golden);

    c = dab(PencilOptions{false, Symmetry{false, true}, 2}, 1, 0);
    const char *vertical[H] = {".##.....", ".##.....", "........", ".##.....", ".##....."};
    expect_rows(c, vertical);
}

TEST(eraser_uses_the_brush_too)
{
    History h;
    Cel c(W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            c.set(x, y, RED);
        }
    }
    auto s = PencilStroke::begin(h, c, Rgba{}, PencilOptions{false, Symmetry{}, 3}, 3, 2);
    s->end();
    const char *golden[H] = {"########", "##...###", "##...###", "##...###", "########"};
    expect_rows(c, golden);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(255, c.get(3, 2).a);
}

TEST(pixel_perfect_is_ignored_for_a_big_brush)
{
    std::vector<Cel> plain;
    for (int perfect = 0; perfect < 2; perfect++) {
        History h;
        plain.emplace_back(W, H);
        auto s = PencilStroke::begin(h, plain.back(), RED,
                                     PencilOptions{perfect != 0, Symmetry{}, 2}, 0, 0);
        s->extend(1, 0);
        s->extend(1, 1);
        s->extend(2, 1);
        s->end();
    }
    ASSERT_EQ_INT(0, memcmp(plain[0].pixels(), plain[1].pixels(),
                            static_cast<size_t>(W) * static_cast<size_t>(H) * sizeof(Rgba)));
}

TEST(oversized_brushes_are_capped)
{
    Cel c = dab(PencilOptions{false, Symmetry{}, 100000}, 3, 2);
    ASSERT_EQ_INT(255, c.get(0, 0).a);
    ASSERT_EQ_INT(255, c.get(W - 1, H - 1).a);
    c = dab(PencilOptions{false, Symmetry{}, -4}, 3, 2);
    const char *golden[H] = {"........", "........", "...#....", "........", "........"};
    expect_rows(c, golden);
}

TEST(brush_helpers_describe_the_footprint)
{
    ASSERT_EQ_INT(1, (PencilOptions{}).brushSize());
    ASSERT_EQ_INT(5, (PencilOptions{false, Symmetry{}, 5}).brushSize());
    ASSERT_EQ_INT(PENCIL_BRUSH_MAX, (PencilOptions{false, Symmetry{}, 9999}).brushSize());
    auto round = PencilOptions{false, Symmetry{}, 3, true};
    ASSERT_TRUE(!round.brushContains(0, 0));
    ASSERT_TRUE(round.brushContains(1, 0));
    ASSERT_TRUE(round.brushContains(1, 1));
    ASSERT_TRUE(!round.brushContains(3, 1)); /* outside the brush box */
    ASSERT_TRUE(!round.brushContains(-1, 1));
    ASSERT_TRUE((PencilOptions{false, Symmetry{}, 3}).brushContains(0, 0));
    ASSERT_EQ_INT(0, (PencilOptions{false, Symmetry{}, 2}).brushOrigin());
    ASSERT_EQ_INT(1, (PencilOptions{false, Symmetry{}, 3}).brushOrigin());
    ASSERT_EQ_INT(1, (PencilOptions{false, Symmetry{}, 4}).brushOrigin());
}

/* The stroke only paints the cels a step newly uncovers; the result must equal stamping the whole
 * brush at every point of the path. */
TEST(big_brush_stroke_equals_stamping_every_point)
{
    for (bool round : {false, true}) {
        for (int size : {2, 5, 6}) {
            PencilOptions options{false, Symmetry{}, size, round};
            const ToolPoint path[] = {{3, 3}, {20, 9}, {8, 18}, {8, 18}, {25, 25}};

            Cel actual(32, 32);
            History h;
            auto stroke = PencilStroke::begin(h, actual, RED, options, path[0].x, path[0].y);
            for (const ToolPoint &p : path) {
                stroke->extend(p.x, p.y);
            }
            stroke->end();

            Cel expected(32, 32);
            auto stamp = [&](int x, int y) {
                for (int j = 0; j < size; j++) {
                    for (int i = 0; i < size; i++) {
                        if (options.brushContains(i, j)) {
                            expected.set(x - options.brushOrigin() + i,
                                         y - options.brushOrigin() + j, RED);
                        }
                    }
                }
            };
            stamp(path[0].x, path[0].y);
            for (size_t k = 1; k < sizeof(path) / sizeof(path[0]); k++) {
                raster_line(path[k - 1].x, path[k - 1].y, path[k].x, path[k].y, stamp);
            }
            ASSERT_EQ_INT(0, memcmp(actual.pixels(), expected.pixels(),
                                    static_cast<size_t>(32 * 32) * sizeof(Rgba)));
        }
    }
}

int main()
{
    RUN(brush_helpers_describe_the_footprint);
    RUN(size_zero_and_one_are_a_single_pixel);
    RUN(even_square_brush_grows_right_and_down);
    RUN(odd_square_brush_is_centered);
    RUN(round_brush_rounds_the_corners);
    RUN(brush_is_clipped_at_the_cel_edge);
    RUN(dragging_sweeps_the_brush_along_the_line);
    RUN(mirrored_brush_reflects_every_pixel_of_the_stamp);
    RUN(eraser_uses_the_brush_too);
    RUN(pixel_perfect_is_ignored_for_a_big_brush);
    RUN(big_brush_stroke_equals_stamping_every_point);
    RUN(oversized_brushes_are_capped);
    return TEST_SUMMARY();
}
