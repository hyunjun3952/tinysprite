#include "minitest.hpp"
#include "tinysprite/guide_ops.hpp"

static Guides guides(std::vector<int> vertical, std::vector<int> horizontal)
{
    Guides g;
    g.vertical = std::move(vertical);
    g.horizontal = std::move(horizontal);
    return g;
}

TEST(setting_the_guides_is_one_undo_step)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(guide_op_set(h, s, guides({2, 5}, {3})));
    ASSERT_EQ_INT(2, static_cast<int>(s.guides().vertical.size()));
    ASSERT_EQ_INT(1, static_cast<int>(s.guides().horizontal.size()));
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.guides().empty());
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(5, s.guides().vertical[1]);
    ASSERT_EQ_INT(3, s.guides().horizontal[0]);
}

TEST(clearing_the_guides_can_be_undone)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(guide_op_set(h, s, guides({1}, {2})));
    ASSERT_TRUE(guide_op_set(h, s, Guides{}));
    ASSERT_TRUE(s.guides().empty());
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.guides().vertical[0]);
    ASSERT_EQ_INT(2, s.guides().horizontal[0]);
}

TEST(setting_the_guides_to_what_they_are_records_nothing)
{
    Sprite s(8, 8);
    History h;
    ASSERT_TRUE(guide_op_set(h, s, guides({4}, {})));
    ASSERT_TRUE(guide_op_set(h, s, guides({4}, {})));
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.guides().empty());
    ASSERT_TRUE(!h.undo());
}

TEST(guide_ops_are_refused_while_a_stroke_is_active)
{
    Sprite s(8, 8);
    History h;
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!guide_op_set(h, s, guides({1}, {})));
    ASSERT_TRUE(!guide_op_set(h, s, Guides{}));
    ASSERT_TRUE(s.guides().empty());
    h.cancelStroke();
}

int main()
{
    RUN(setting_the_guides_is_one_undo_step);
    RUN(clearing_the_guides_can_be_undone);
    RUN(setting_the_guides_to_what_they_are_records_nothing);
    RUN(guide_ops_are_refused_while_a_stroke_is_active);
    return TEST_SUMMARY();
}
