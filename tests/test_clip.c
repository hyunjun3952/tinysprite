#include <string.h>

#include "minitest.h"
#include "tinysprite/clip.h"

/* Rows of letters: '.' is transparent, anything else is an opaque pixel whose red channel is the
 * letter, so every distinct letter is a distinct color. */
static void fill(Cel *cel, int x0, int y0, int rows, const char *const *golden)
{
    for (int y = 0; y < rows; y++) {
        for (int x = 0; golden[y][x]; x++) {
            char c = golden[y][x];
            (void)cel_set(cel, x0 + x, y0 + y,
                          c == '.' ? (Rgba){0, 0, 0, 0} : (Rgba){(uint8_t)c, 0, 0, 255});
        }
    }
}

static char letter(Rgba color)
{
    static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyz";
    if (!color.a) {
        return '.';
    }
    return LETTERS[color.r - 'a'];
}

static void expect_cel(const Cel *cel, int x0, int y0, int rows, const char *const *golden)
{
    for (int y = 0; y < rows; y++) {
        size_t len = strlen(golden[y]);
        char row[16] = {0};
        for (size_t x = 0; x < len; x++) {
            row[x] = letter(cel_get(cel, x0 + (int)x, y0 + y));
        }
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static void expect_clip(const Clip *clip, int rows, const char *const *golden)
{
    ASSERT_EQ_INT(rows, clip_height(clip));
    for (int y = 0; y < rows; y++) {
        size_t len = strlen(golden[y]);
        ASSERT_EQ_INT((long long)len, clip_width(clip));
        char row[16] = {0};
        for (size_t x = 0; x < len; x++) {
            row[x] = letter(clip_get(clip, (int)x, y));
        }
        if (strcmp(golden[y], row) != 0) {
            MT_FAIL("row %d: expected %s, got %s", y, golden[y], row);
        }
    }
}

static const char *const SAMPLE[] = {"abc", "d.f"};

static Clip *sample_clip(Cel *cel)
{
    fill(cel, 1, 1, 2, SAMPLE);
    return clip_copy(cel, (Rect){1, 1, 3, 2});
}

TEST(copy_captures_the_region)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    ASSERT_TRUE(clip != NULL);
    expect_clip(clip, 2, SAMPLE);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(copy_is_a_snapshot)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    (void)cel_set(cel, 1, 1, (Rgba){0, 0, 0, 0});
    ASSERT_EQ_INT('a', clip_get(clip, 0, 0).r);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(copy_is_clipped_to_the_cel)
{
    Cel *cel = cel_create(4, 3);
    const char *const rows[] = {"ab", "cd"};
    fill(cel, 2, 1, 2, rows);
    Clip *clip = clip_copy(cel, (Rect){2, 1, 5, 5});
    ASSERT_TRUE(clip != NULL);
    expect_clip(clip, 2, rows);
    clip_destroy(clip);
    ASSERT_TRUE(clip_copy(cel, (Rect){4, 0, 2, 2}) == NULL);
    ASSERT_TRUE(clip_copy(cel, (Rect){0, 0, 0, 2}) == NULL);
    ASSERT_TRUE(clip_copy(NULL, (Rect){0, 0, 1, 1}) == NULL);
    cel_destroy(cel);
}

TEST(flips_mirror_the_pixels)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    clip_flip_horizontal(clip);
    const char *const horizontal[] = {"cba", "f.d"};
    expect_clip(clip, 2, horizontal);
    clip_flip_vertical(clip);
    const char *const both[] = {"f.d", "cba"};
    expect_clip(clip, 2, both);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(rotations_turn_a_non_square_clip)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    ASSERT_EQ_INT(0, clip_rotate(clip, 1));
    const char *const cw[] = {"da", ".b", "fc"};
    expect_clip(clip, 3, cw);
    ASSERT_EQ_INT(0, clip_rotate(clip, 0));
    expect_clip(clip, 2, SAMPLE);
    ASSERT_EQ_INT(0, clip_rotate(clip, 0));
    const char *const ccw[] = {"cf", "b.", "ad"};
    expect_clip(clip, 3, ccw);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(paste_skips_transparent_pixels_and_undoes_in_one_step)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    History *h = history_create();
    (void)cel_set(cel, 3, 4, (Rgba){'z', 0, 0, 255}); /* under the clip's transparent pixel */
    ASSERT_EQ_INT(0, clip_paste(h, cel, clip, 2, 3));
    const char *const after[] = {"abc", "dzf"};
    expect_cel(cel, 2, 3, 2, after);
    ASSERT_EQ_INT(0, history_undo(h));
    ASSERT_EQ_INT('.', letter(cel_get(cel, 2, 3)));
    ASSERT_EQ_INT('z', letter(cel_get(cel, 3, 4)));
    ASSERT_TRUE(!history_can_undo(h));
    history_destroy(h);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(paste_is_clipped_and_negative_offsets_work)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    History *h = history_create();
    ASSERT_EQ_INT(0, clip_paste(h, cel, clip, -1, 4));
    ASSERT_EQ_INT('b', letter(cel_get(cel, 0, 4)));
    ASSERT_EQ_INT('c', letter(cel_get(cel, 1, 4)));
    ASSERT_EQ_INT(0, clip_paste(h, cel, clip, 5, -1));
    ASSERT_EQ_INT('d', letter(cel_get(cel, 5, 0)));
    history_destroy(h);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(paste_fails_during_a_stroke_or_without_arguments)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    History *h = history_create();
    ASSERT_EQ_INT(-1, clip_paste(NULL, cel, clip, 0, 0));
    ASSERT_EQ_INT(-1, clip_paste(h, NULL, clip, 0, 0));
    ASSERT_EQ_INT(-1, clip_paste(h, cel, NULL, 0, 0));
    ASSERT_EQ_INT(0, history_begin_stroke(h, cel));
    ASSERT_EQ_INT(-1, clip_paste(h, cel, clip, 0, 0));
    (void)history_cancel_stroke(h);
    history_destroy(h);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(replace_clears_the_old_rect_and_is_one_undo_step)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    History *h = history_create();
    clip_flip_horizontal(clip);
    ASSERT_EQ_INT(0, clip_replace(h, cel, (Rect){1, 1, 3, 2}, clip, 1, 1));
    const char *const flipped[] = {"cba", "f.d"};
    expect_cel(cel, 1, 1, 2, flipped);

    ASSERT_EQ_INT(0, history_undo(h));
    expect_cel(cel, 1, 1, 2, SAMPLE);
    ASSERT_TRUE(!history_can_undo(h));
    history_destroy(h);
    clip_destroy(clip);
    cel_destroy(cel);
}

TEST(replace_to_another_place_moves_the_pixels)
{
    Cel *cel = cel_create(6, 5);
    Clip *clip = sample_clip(cel);
    History *h = history_create();
    ASSERT_EQ_INT(0, clip_replace(h, cel, (Rect){1, 1, 3, 2}, clip, 2, 2));
    ASSERT_EQ_INT('.', letter(cel_get(cel, 1, 1)));
    const char *const moved[] = {"abc", "d.f"};
    expect_cel(cel, 2, 2, 2, moved);
    history_destroy(h);
    clip_destroy(clip);
    cel_destroy(cel);
}

int main(void)
{
    RUN(copy_captures_the_region);
    RUN(copy_is_a_snapshot);
    RUN(copy_is_clipped_to_the_cel);
    RUN(flips_mirror_the_pixels);
    RUN(rotations_turn_a_non_square_clip);
    RUN(paste_skips_transparent_pixels_and_undoes_in_one_step);
    RUN(paste_is_clipped_and_negative_offsets_work);
    RUN(paste_fails_during_a_stroke_or_without_arguments);
    RUN(replace_clears_the_old_rect_and_is_one_undo_step);
    RUN(replace_to_another_place_moves_the_pixels);
    return TEST_SUMMARY();
}
