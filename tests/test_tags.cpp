#include "minitest.hpp"
#include "tinysprite/frame_ops.hpp"
#include "tinysprite/playback.hpp"
#include "tinysprite/tag_ops.hpp"

static Sprite frames(int n)
{
    Sprite s(2, 2);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    return s;
}

static Sprite::Tag tag(const char *name, int from, int to, PlayMode dir = PlayMode::Forward)
{
    return Sprite::Tag{name, from, to, dir};
}

TEST(a_tag_is_a_named_range_with_a_direction)
{
    Sprite s = frames(4);
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(s.addTag(tag("walk", 1, 3, PlayMode::PingPong)));
    ASSERT_EQ_INT(1, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(s.tags()[0].name == "walk");
    ASSERT_EQ_INT(1, s.tags()[0].from);
    ASSERT_EQ_INT(3, s.tags()[0].to);
    ASSERT_TRUE(s.tags()[0].direction == PlayMode::PingPong);
}

TEST(bad_tags_are_refused)
{
    Sprite s = frames(3);
    ASSERT_TRUE(!s.addTag(tag("a", -1, 1)));
    ASSERT_TRUE(!s.addTag(tag("a", 0, 3)));
    ASSERT_TRUE(!s.addTag(tag("a", 2, 1)));
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(s.addTag(tag("ok", 0, 0)));
    ASSERT_TRUE(!s.setTag(1, tag("x", 0, 0)));
    ASSERT_TRUE(!s.setTag(0, tag("x", 0, 9)));
    ASSERT_TRUE(!s.removeTag(1));
    ASSERT_TRUE(s.tags()[0].name == "ok");
}

TEST(names_are_truncated_without_splitting_a_character)
{
    Sprite s = frames(1);
    std::string long_name(TAG_NAME_MAX - 1, 'a');
    long_name += "\xc3\xa9\xc3\xa9"; /* two two-byte characters straddle the limit */
    ASSERT_TRUE(s.addTag(tag(long_name.c_str(), 0, 0)));
    ASSERT_TRUE(s.tags()[0].name.size() <= static_cast<size_t>(TAG_NAME_MAX));
    ASSERT_EQ_INT(TAG_NAME_MAX - 1, static_cast<int>(s.tags()[0].name.size()));
}

TEST(set_and_remove_edit_the_list)
{
    Sprite s = frames(4);
    s.addTag(tag("a", 0, 1));
    s.addTag(tag("b", 2, 3));
    ASSERT_TRUE(s.setTag(1, tag("c", 1, 2, PlayMode::Reverse)));
    ASSERT_TRUE(s.tags()[1].name == "c");
    ASSERT_TRUE(s.removeTag(0));
    ASSERT_EQ_INT(1, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(s.tags()[0].name == "c");
}

TEST(inserting_a_frame_shifts_or_stretches_tags)
{
    Sprite s = frames(4);
    s.addTag(tag("after", 2, 3));
    s.addTag(tag("around", 1, 2));
    s.addTag(tag("before", 0, 0));
    s.insertFrame(2, s.makeFrame()); /* inside "around", at the start of "after" */
    ASSERT_EQ_INT(3, s.tags()[0].from);
    ASSERT_EQ_INT(4, s.tags()[0].to);
    ASSERT_EQ_INT(1, s.tags()[1].from);
    ASSERT_EQ_INT(3, s.tags()[1].to);
    ASSERT_EQ_INT(0, s.tags()[2].from);
    ASSERT_EQ_INT(0, s.tags()[2].to);
}

TEST(inserting_after_the_last_frame_leaves_tags_alone)
{
    Sprite s = frames(2);
    s.addTag(tag("all", 0, 1));
    s.insertFrame(2, s.makeFrame());
    ASSERT_EQ_INT(1, s.tags()[0].to);
}

TEST(removing_a_frame_shrinks_shifts_or_drops_tags)
{
    Sprite s = frames(5);
    s.addTag(tag("late", 3, 4));
    s.addTag(tag("mid", 1, 2));
    s.addTag(tag("one", 0, 0));
    s.detachFrame(0); /* "one" loses its only frame, the others shift down */
    ASSERT_EQ_INT(2, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(s.tags()[0].name == "late");
    ASSERT_EQ_INT(2, s.tags()[0].from);
    ASSERT_EQ_INT(3, s.tags()[0].to);
    ASSERT_EQ_INT(0, s.tags()[1].from);
    ASSERT_EQ_INT(1, s.tags()[1].to);
    s.detachFrame(1); /* inside "mid" */
    ASSERT_EQ_INT(0, s.tags()[1].from);
    ASSERT_EQ_INT(0, s.tags()[1].to);
}

TEST(tag_edits_are_undoable)
{
    Sprite s = frames(3);
    History h;
    ASSERT_TRUE(tag_op_add(h, s, tag("a", 0, 1)));
    ASSERT_EQ_INT(1, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(tag_op_set(h, s, 0, tag("b", 1, 2, PlayMode::Reverse)));
    ASSERT_TRUE(s.tags()[0].name == "b");
    ASSERT_TRUE(tag_op_remove(h, s, 0));
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.tags()[0].name == "b");
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.tags()[0].name == "a");
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(s.tags()[0].name == "a");
}

TEST(invalid_tag_ops_record_nothing)
{
    Sprite s = frames(2);
    History h;
    ASSERT_TRUE(!tag_op_add(h, s, tag("a", 0, 5)));
    ASSERT_TRUE(!tag_op_set(h, s, 0, tag("a", 0, 1)));
    ASSERT_TRUE(!tag_op_remove(h, s, 0));
    ASSERT_TRUE(!h.canUndo());
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!tag_op_add(h, s, tag("a", 0, 1)));
    h.cancelStroke();
}

TEST(setting_a_tag_to_what_it_is_records_nothing)
{
    Sprite s = frames(2);
    History h;
    tag_op_add(h, s, tag("a", 0, 1));
    ASSERT_TRUE(tag_op_set(h, s, 0, tag("a", 0, 1)));
    h.undo();
    ASSERT_TRUE(!h.canUndo());
}

TEST(frame_ops_keep_tags_in_step_and_undo_restores_them)
{
    Sprite s = frames(3);
    History h;
    tag_op_add(h, s, tag("only", 1, 1));
    ASSERT_TRUE(frame_op_remove(h, s, 1));
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, static_cast<int>(s.tags().size()));
    ASSERT_EQ_INT(1, s.tags()[0].from);
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(0, static_cast<int>(s.tags().size()));
    ASSERT_TRUE(h.undo());

    ASSERT_TRUE(frame_op_add(h, s, 1)); /* before "only" */
    ASSERT_EQ_INT(2, s.tags()[0].from);
    ASSERT_TRUE(h.undo());
    ASSERT_EQ_INT(1, s.tags()[0].from);
    ASSERT_TRUE(frame_op_duplicate(h, s, 1)); /* the copy goes after the source: inside nothing */
    ASSERT_EQ_INT(1, s.tags()[0].from);
}

int main()
{
    RUN(a_tag_is_a_named_range_with_a_direction);
    RUN(bad_tags_are_refused);
    RUN(names_are_truncated_without_splitting_a_character);
    RUN(set_and_remove_edit_the_list);
    RUN(inserting_a_frame_shifts_or_stretches_tags);
    RUN(inserting_after_the_last_frame_leaves_tags_alone);
    RUN(removing_a_frame_shrinks_shifts_or_drops_tags);
    RUN(tag_edits_are_undoable);
    RUN(invalid_tag_ops_record_nothing);
    RUN(setting_a_tag_to_what_it_is_records_nothing);
    RUN(frame_ops_keep_tags_in_step_and_undo_restores_them);
    return TEST_SUMMARY();
}
