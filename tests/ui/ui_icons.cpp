/* The icon bitmaps are plain data, so check they are well formed: every icon is SIZE rows of SIZE
 * characters made of 'X' and '.', has lit pixels, and no two icons are identical. */
#include <cstring>

#include "icons.hpp"
#include "minitest.hpp"

TEST(every_bitmap_is_well_formed)
{
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); i++) {
        const auto &rows = icons::bitmap(static_cast<icons::Icon>(i));
        int lit = 0;
        for (const char *row : rows) {
            ASSERT_EQ_INT(icons::SIZE, static_cast<int>(std::strlen(row)));
            for (int x = 0; x < icons::SIZE; x++) {
                ASSERT_TRUE(row[x] == 'X' || row[x] == '.');
                if (row[x] == 'X') {
                    lit++;
                }
            }
        }
        ASSERT_TRUE(lit > 0);
    }
}

TEST(icons_differ_from_each_other)
{
    int count = static_cast<int>(icons::Icon::Count);
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            const auto &ra = icons::bitmap(static_cast<icons::Icon>(a));
            const auto &rb = icons::bitmap(static_cast<icons::Icon>(b));
            bool same = true;
            for (size_t y = 0; y < ra.size(); y++) {
                same = same && std::strcmp(ra[y], rb[y]) == 0;
            }
            ASSERT_TRUE(!same);
        }
    }
}

/* Aseprite multiplies every size by an integer `guiscale()`; an icon drawn at a fractional zoom
 * would get uneven pixel runs, so the bitmaps scale by whole pixels only. */
TEST(the_pixel_scale_is_a_whole_number_of_at_least_one)
{
    ASSERT_EQ_INT(1, icons::pixel_scale(1.0f));
    ASSERT_EQ_INT(1, icons::pixel_scale(1.25f));
    ASSERT_EQ_INT(2, icons::pixel_scale(1.5f));
    ASSERT_EQ_INT(2, icons::pixel_scale(2.0f));
    ASSERT_EQ_INT(2, icons::pixel_scale(2.25f));
    ASSERT_EQ_INT(3, icons::pixel_scale(3.0f));
    ASSERT_EQ_INT(1, icons::pixel_scale(0.2f));
}

TEST(the_main_tools_have_icons)
{
    const icons::Icon tools[] = {icons::Icon::Pencil, icons::Icon::Eraser, icons::Icon::Bucket,
                                 icons::Icon::Eyedropper, icons::Icon::Select};
    for (icons::Icon icon : tools) {
        ASSERT_TRUE(icons::bitmap(icon)[0] != nullptr);
    }
}

int main()
{
    RUN(every_bitmap_is_well_formed);
    RUN(icons_differ_from_each_other);
    RUN(the_pixel_scale_is_a_whole_number_of_at_least_one);
    RUN(the_main_tools_have_icons);
    return TEST_SUMMARY();
}
