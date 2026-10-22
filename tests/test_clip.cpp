#include <cstdlib>
#include <cstring>

#include "minitest.hpp"
#include "tinysprite/selection.hpp"

/* Rows of letters: '.' is transparent, anything else is an opaque pixel whose red channel is the
 * letter, so every distinct letter is a distinct color. */
static void fill(Cel &cel, int x0, int y0, int rows, const char *const *golden)
{
    for (int y = 0; y < rows; y++) {
        for (int x = 0; golden[y][x] != '\0'; x++) {
            char c = golden[y][x];
            (void)cel.set(x0 + x, y0 + y,
                          c == '.' ? Rgba{0, 0, 0, 0} : Rgba{(uint8_t)c, 0, 0, 255});
        }
    }
}

static char letter(Rgba color)
{
    static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyz";
    if (color.a == 0) {
        return '.';
    }
    return LETTERS[color.r - 'a'];
}

static void expect_cel(const Cel &cel, int x0, int y0, int rows, const char *const *golden)
{
    for (int y = 0; y < rows; y++) {
        size_t len = strlen(golden[y]);
        char row[16] = {0};
        for (size_t x = 0; x < len; x++) {
            row[x] = letter(cel.get(x0 + (int)x, y0 + y));
        }
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static void expect_clip(const Clip &clip, int rows, const char *const *golden)
{
    ASSERT_EQ_INT(rows, clip.height());
    for (int y = 0; y < rows; y++) {
        size_t len = strlen(golden[y]);
        ASSERT_EQ_INT((long long)len, clip.width());
        char row[16] = {0};
        for (size_t x = 0; x < len; x++) {
            row[x] = letter(clip.get((int)x, y));
        }
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static const char *const SAMPLE[] = {"abc", "d.f"};

/* A copy that must succeed. */
static Clip copy_of(const Cel &cel, Rect area)
{
    std::optional<Clip> clip = Clip::copy(cel, area);
    if (!clip) {
        MT_FAIL("clip is empty");
        std::abort();
    }
    return *clip;
}

static Clip sample_clip(Cel &cel)
{
    fill(cel, 1, 1, 2, SAMPLE);
    return copy_of(cel, Rect{1, 1, 3, 2});
}

TEST(copy_captures_the_region)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    expect_clip(clip, 2, SAMPLE);
}

TEST(copy_is_a_snapshot)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    (void)cel.set(1, 1, Rgba{0, 0, 0, 0});
    ASSERT_EQ_CHAR('a', clip.get(0, 0).r);
}

TEST(copy_is_clipped_to_the_cel)
{
    Cel cel(4, 3);
    const char *const rows[] = {"ab", "cd"};
    fill(cel, 2, 1, 2, rows);
    Clip clip = copy_of(cel, Rect{2, 1, 5, 5});
    expect_clip(clip, 2, rows);
    ASSERT_TRUE(!Clip::copy(cel, Rect{4, 0, 2, 2}));
    ASSERT_TRUE(!Clip::copy(cel, Rect{0, 0, 0, 2}));
}

TEST(flips_mirror_the_pixels)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    clip.flipHorizontal();
    const char *const horizontal[] = {"cba", "f.d"};
    expect_clip(clip, 2, horizontal);
    clip.flipVertical();
    const char *const both[] = {"f.d", "cba"};
    expect_clip(clip, 2, both);
}

TEST(rotations_turn_a_non_square_clip)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    clip.rotate(true);
    const char *const cw[] = {"da", ".b", "fc"};
    expect_clip(clip, 3, cw);
    clip.rotate(false);
    expect_clip(clip, 2, SAMPLE);
    clip.rotate(false);
    const char *const ccw[] = {"cf", "b.", "ad"};
    expect_clip(clip, 3, ccw);
}

TEST(paste_skips_transparent_pixels_and_undoes_in_one_step)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    History h;
    (void)cel.set(3, 4, Rgba{'z', 0, 0, 255}); /* under the clip's transparent pixel */
    ASSERT_TRUE(clip_paste(h, cel, clip, 2, 3));
    const char *const after[] = {"abc", "dzf"};
    expect_cel(cel, 2, 3, 2, after);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_CHAR('.', letter(cel.get(2, 3)));
    ASSERT_EQ_CHAR('z', letter(cel.get(3, 4)));
    ASSERT_TRUE(!h.canUndo());
}

TEST(paste_is_clipped_and_negative_offsets_work)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    History h;
    ASSERT_TRUE(clip_paste(h, cel, clip, -1, 4));
    ASSERT_EQ_CHAR('b', letter(cel.get(0, 4)));
    ASSERT_EQ_CHAR('c', letter(cel.get(1, 4)));
    ASSERT_TRUE(clip_paste(h, cel, clip, 5, -1));
    ASSERT_EQ_CHAR('d', letter(cel.get(5, 0)));
}

TEST(paste_fails_during_a_stroke)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    History h;
    ASSERT_TRUE(h.beginStroke(cel));
    ASSERT_TRUE(!clip_paste(h, cel, clip, 0, 0));
    (void)h.cancelStroke();
}

TEST(replace_clears_the_old_rect_and_is_one_undo_step)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    History h;
    clip.flipHorizontal();
    ASSERT_TRUE(clip_replace(h, cel, Rect{1, 1, 3, 2}, clip, 1, 1));
    const char *const flipped[] = {"cba", "f.d"};
    expect_cel(cel, 1, 1, 2, flipped);

    ASSERT_TRUE(h.undo());
    expect_cel(cel, 1, 1, 2, SAMPLE);
    ASSERT_TRUE(!h.canUndo());
}

TEST(replace_to_another_place_moves_the_pixels)
{
    Cel cel(6, 5);
    Clip clip = sample_clip(cel);
    History h;
    ASSERT_TRUE(clip_replace(h, cel, Rect{1, 1, 3, 2}, clip, 2, 2));
    ASSERT_EQ_CHAR('.', letter(cel.get(1, 1)));
    const char *const moved[] = {"abc", "d.f"};
    expect_cel(cel, 2, 2, 2, moved);
}

int main()
{
    RUN(copy_captures_the_region);
    RUN(copy_is_a_snapshot);
    RUN(copy_is_clipped_to_the_cel);
    RUN(flips_mirror_the_pixels);
    RUN(rotations_turn_a_non_square_clip);
    RUN(paste_skips_transparent_pixels_and_undoes_in_one_step);
    RUN(paste_is_clipped_and_negative_offsets_work);
    RUN(paste_fails_during_a_stroke);
    RUN(replace_clears_the_old_rect_and_is_one_undo_step);
    RUN(replace_to_another_place_moves_the_pixels);
    return TEST_SUMMARY();
}
