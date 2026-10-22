#include "minitest.h"
#include "tinysprite/cel.h"
#include "tinysprite/thumbnail.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

TEST(size_scales_a_small_image_up_by_a_whole_factor)
{
    int w = 0;
    int h = 0;
    thumbnail_size(8, 8, 36, 24, w, h);
    ASSERT_EQ_INT(24, w); /* 3x: the limit is the height */
    ASSERT_EQ_INT(24, h);
    thumbnail_size(10, 4, 36, 24, w, h);
    ASSERT_EQ_INT(30, w); /* 3x again, so every pixel stays a clean block */
    ASSERT_EQ_INT(12, h);
}

TEST(size_scales_a_large_image_down_keeping_the_aspect_ratio)
{
    int w = 0;
    int h = 0;
    thumbnail_size(40, 32, 36, 24, w, h);
    ASSERT_EQ_INT(30, w);
    ASSERT_EQ_INT(24, h);
    thumbnail_size(1000, 10, 36, 24, w, h);
    ASSERT_EQ_INT(36, w);
    ASSERT_EQ_INT(1, h); /* never below one pixel */
}

TEST(size_never_exceeds_the_box)
{
    for (int sw = 1; sw < 200; sw += 7) {
        for (int sh = 1; sh < 200; sh += 11) {
            int w = 0;
            int h = 0;
            thumbnail_size(sw, sh, 36, 24, w, h);
            ASSERT_TRUE(w >= 1 && w <= 36);
            ASSERT_TRUE(h >= 1 && h <= 24);
        }
    }
}

TEST(pixels_of_a_whole_factor_upscale_are_exact_blocks)
{
    Cel cel(2, 1);
    cel.set(0, 0, RED);
    cel.set(1, 0, BLUE);
    std::vector<Rgba> pixels = thumbnail_pixels(cel, 4, 2);
    ASSERT_EQ_INT(8, static_cast<int>(pixels.size()));
    for (int y = 0; y < 2; y++) {
        ASSERT_TRUE(pixels[static_cast<size_t>(y) * 4 + 0] == RED);
        ASSERT_TRUE(pixels[static_cast<size_t>(y) * 4 + 1] == RED);
        ASSERT_TRUE(pixels[static_cast<size_t>(y) * 4 + 2] == BLUE);
        ASSERT_TRUE(pixels[static_cast<size_t>(y) * 4 + 3] == BLUE);
    }
}

TEST(pixels_of_a_downscale_sample_the_cell_centers)
{
    Cel cel(4, 4);
    cel.set(1, 1, RED);
    cel.set(3, 1, BLUE);
    std::vector<Rgba> pixels = thumbnail_pixels(cel, 2, 2);
    ASSERT_TRUE(pixels[0] == RED);  /* the cell covering columns 0-1 samples column 1 */
    ASSERT_TRUE(pixels[1] == BLUE); /* the cell covering columns 2-3 samples column 3 */
    ASSERT_EQ_INT(0, pixels[2].a);
}

TEST(pixels_keep_transparency)
{
    Cel cel(3, 3);
    std::vector<Rgba> pixels = thumbnail_pixels(cel, 3, 3);
    for (const Rgba &p : pixels) {
        ASSERT_EQ_INT(0, p.a);
    }
}

int main()
{
    RUN(size_scales_a_small_image_up_by_a_whole_factor);
    RUN(size_scales_a_large_image_down_keeping_the_aspect_ratio);
    RUN(size_never_exceeds_the_box);
    RUN(pixels_of_a_whole_factor_upscale_are_exact_blocks);
    RUN(pixels_of_a_downscale_sample_the_cell_centers);
    RUN(pixels_keep_transparency);
    return TEST_SUMMARY();
}
