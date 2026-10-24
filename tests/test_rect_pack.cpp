#include <vector>

#include "minitest.hpp"
#include "tinysprite/rect_pack.hpp"

static Rect size(int w, int h)
{
    return Rect{0, 0, w, h};
}

/* Whether the placed rectangles (each grown by `padding` right and down) are inside the sheet and
 * do not overlap each other. */
static bool valid(const std::vector<Rect> &sizes, const PackedRects &packed, int padding)
{
    for (size_t i = 0; i < sizes.size(); i++) {
        const Rect &r = packed.rects[i];
        if (r.w != sizes[i].w || r.h != sizes[i].h || r.x < 0 || r.y < 0 ||
            r.x + r.w > packed.width || r.y + r.h > packed.height) {
            return false;
        }
        for (size_t j = i + 1; j < sizes.size(); j++) {
            const Rect &o = packed.rects[j];
            bool apart = r.x + r.w + padding <= o.x || o.x + o.w + padding <= r.x ||
                         r.y + r.h + padding <= o.y || o.y + o.h + padding <= r.y;
            if (!apart) {
                return false;
            }
        }
    }
    return true;
}

TEST(one_rectangle_makes_a_sheet_of_its_size)
{
    std::optional<PackedRects> packed = pack_rects({size(5, 3)}, 0, 1024);
    ASSERT_TRUE(packed.has_value());
    if (!packed) {
        return;
    }
    ASSERT_EQ_INT(5, packed->width);
    ASSERT_EQ_INT(3, packed->height);
    ASSERT_EQ_INT(0, packed->rects[0].x);
    ASSERT_EQ_INT(0, packed->rects[0].y);
}

TEST(four_equal_squares_fill_a_square_exactly)
{
    std::vector<Rect> sizes(4, size(8, 8));
    std::optional<PackedRects> packed = pack_rects(sizes, 0, 1024);
    ASSERT_TRUE(packed.has_value());
    if (!packed) {
        return;
    }
    ASSERT_EQ_INT(16, packed->width);
    ASSERT_EQ_INT(16, packed->height);
    ASSERT_TRUE(valid(sizes, *packed, 0));
}

TEST(rectangles_of_many_sizes_never_overlap_and_stay_inside_the_sheet)
{
    std::vector<Rect> sizes;
    unsigned seed = 12345;
    for (int i = 0; i < 60; i++) {
        seed = seed * 1103515245u + 12345u;
        int w = 3 + static_cast<int>((seed >> 16) % 29);
        seed = seed * 1103515245u + 12345u;
        int h = 2 + static_cast<int>((seed >> 16) % 31);
        sizes.push_back(size(w, h));
    }
    for (int padding : {0, 1, 3}) {
        std::optional<PackedRects> packed = pack_rects(sizes, padding, 4096);
        ASSERT_TRUE(packed.has_value());
        if (!packed) {
            return;
        }
        const PackedRects &sheet = *packed;
        ASSERT_TRUE(valid(sizes, sheet, padding));
        long long area = 0;
        for (const Rect &r : sizes) {
            area += static_cast<long long>(r.w) * r.h;
        }
        /* a decent packing: the sheet is no more than about twice the area of what it holds */
        ASSERT_TRUE(static_cast<long long>(sheet.width) * sheet.height <= area * 2 + 4000);
    }
}

TEST(the_result_is_in_the_order_of_the_input_and_does_not_change_between_runs)
{
    std::vector<Rect> sizes = {size(10, 4), size(3, 9), size(7, 7), size(2, 2)};
    std::optional<PackedRects> a = pack_rects(sizes, 1, 256);
    std::optional<PackedRects> b = pack_rects(sizes, 1, 256);
    ASSERT_TRUE(a.has_value() && b.has_value());
    if (!a || !b) {
        return;
    }
    ASSERT_EQ_INT(a->width, b->width);
    for (size_t i = 0; i < sizes.size(); i++) {
        ASSERT_EQ_INT(sizes[i].w, a->rects[i].w);
        ASSERT_EQ_INT(a->rects[i].x, b->rects[i].x);
        ASSERT_EQ_INT(a->rects[i].y, b->rects[i].y);
    }
}

TEST(padding_separates_neighbours_but_not_the_sheet_edges)
{
    std::vector<Rect> sizes(2, size(4, 4));
    std::optional<PackedRects> packed = pack_rects(sizes, 2, 1024);
    ASSERT_TRUE(packed.has_value());
    if (!packed) {
        return;
    }
    ASSERT_TRUE(valid(sizes, *packed, 2));
    /* two 4 x 4 side by side with a gap of 2, or one over the other: 10 x 4 / 4 x 10 */
    ASSERT_EQ_INT(40, packed->width * packed->height);
}

TEST(nothing_is_packed_when_it_cannot_fit_or_there_is_nothing)
{
    ASSERT_TRUE(!pack_rects({}, 0, 100).has_value());
    ASSERT_TRUE(!pack_rects({size(101, 4)}, 0, 100).has_value());
    ASSERT_TRUE(!pack_rects({size(0, 4)}, 0, 100).has_value());
    ASSERT_TRUE(!pack_rects({size(4, 4)}, -1, 100).has_value());
    ASSERT_TRUE(!pack_rects({size(4, 4)}, 101, 100).has_value()); /* more padding than sheet */
    ASSERT_TRUE(!pack_rects({size(4, 4)}, 2147483647, 100).has_value()); /* no overflow either */
    /* four 60 x 60 cannot share a 100 x 100 sheet */
    ASSERT_TRUE(!pack_rects(std::vector<Rect>(4, size(60, 60)), 0, 100).has_value());
    ASSERT_TRUE(pack_rects(std::vector<Rect>(4, size(60, 60)), 0, 120).has_value());
}

int main()
{
    RUN(one_rectangle_makes_a_sheet_of_its_size);
    RUN(four_equal_squares_fill_a_square_exactly);
    RUN(rectangles_of_many_sizes_never_overlap_and_stay_inside_the_sheet);
    RUN(the_result_is_in_the_order_of_the_input_and_does_not_change_between_runs);
    RUN(padding_separates_neighbours_but_not_the_sheet_edges);
    RUN(nothing_is_packed_when_it_cannot_fit_or_there_is_nothing);
    return TEST_SUMMARY();
}
