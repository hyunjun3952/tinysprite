#include "minitest.hpp"
#include "tinysprite/cel_ops.hpp"
#include "tinysprite/frame_ops.hpp"
#include "tinysprite/io.hpp"
#include "tinysprite/layer_ops.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

static Sprite frames(int n)
{
    Sprite s(4, 4);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    return s;
}

TEST(cels_start_unlinked)
{
    Sprite s = frames(3);
    for (int f = 0; f < 3; f++) {
        ASSERT_TRUE(!s.layer(0)->celLinked(f));
    }
}

TEST(linking_makes_two_frames_share_one_cel)
{
    Sprite s = frames(3);
    History h;
    s.layer(0)->cel(0)->set(1, 1, RED);
    ASSERT_TRUE(cel_op_link(h, *s.layer(0), 0, 2));
    ASSERT_TRUE(s.layer(0)->cel(0) == s.layer(0)->cel(2));
    ASSERT_TRUE(s.layer(0)->celLinked(0));
    ASSERT_TRUE(s.layer(0)->celLinked(2));
    ASSERT_TRUE(!s.layer(0)->celLinked(1));
    ASSERT_EQ_INT(255, s.layer(0)->cel(2)->get(1, 1).r);
}

TEST(a_stroke_on_one_linked_frame_shows_on_the_other)
{
    Sprite s = frames(2);
    History h;
    cel_op_link(h, *s.layer(0), 0, 1);
    h.beginStroke(*s.layer(0)->cel(1));
    h.setPixel(2, 2, BLUE);
    h.endStroke();
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(2, 2).b);
    h.undo();
    ASSERT_EQ_INT(0, s.layer(0)->cel(0)->get(2, 2).a);
}

TEST(linking_and_unlinking_are_refused_while_a_stroke_is_open)
{
    Sprite s = frames(3);
    History h;
    ASSERT_TRUE(cel_op_link(h, *s.layer(0), 0, 1));
    h.beginStroke(*s.layer(0)->cel(2));
    ASSERT_TRUE(!cel_op_unlink(h, *s.layer(0), 1));
    ASSERT_TRUE(
        !cel_op_link(h, *s.layer(0), 0, 1)); /* already linked: still refused, not a no-op */
    ASSERT_TRUE(!cel_op_link(h, *s.layer(0), 0, 2));
    ASSERT_TRUE(s.layer(0)->celLinked(1));
    ASSERT_TRUE(!s.layer(0)->celLinked(2));
    h.endStroke();
}

TEST(link_is_undoable_and_brings_back_the_old_cel)
{
    Sprite s = frames(2);
    History h;
    s.layer(0)->cel(1)->set(3, 3, BLUE);
    Cel *old = s.layer(0)->cel(1);
    ASSERT_TRUE(cel_op_link(h, *s.layer(0), 0, 1));
    ASSERT_EQ_INT(0, s.layer(0)->cel(1)->get(3, 3).a);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.layer(0)->cel(1) == old);
    ASSERT_EQ_INT(255, old->get(3, 3).b);
    ASSERT_TRUE(!s.layer(0)->celLinked(0));
    ASSERT_TRUE(h.redo());
    ASSERT_TRUE(s.layer(0)->cel(1) == s.layer(0)->cel(0));
}

TEST(linking_what_is_already_linked_records_nothing)
{
    Sprite s = frames(2);
    History h;
    cel_op_link(h, *s.layer(0), 0, 1);
    ASSERT_TRUE(cel_op_link(h, *s.layer(0), 0, 1));
    ASSERT_TRUE(cel_op_link(h, *s.layer(0), 1, 0));
    h.undo();
    ASSERT_TRUE(!h.canUndo());
}

TEST(bad_links_are_refused)
{
    Sprite s = frames(2);
    History h;
    ASSERT_TRUE(!cel_op_link(h, *s.layer(0), 0, 2));
    ASSERT_TRUE(!cel_op_link(h, *s.layer(0), -1, 1));
    ASSERT_TRUE(!cel_op_link(h, *s.layer(0), 1, 5));
    h.beginStroke(*s.layer(0)->cel(0));
    ASSERT_TRUE(!cel_op_link(h, *s.layer(0), 0, 1));
    h.cancelStroke();
    ASSERT_TRUE(!h.canUndo());
}

TEST(unlink_gives_the_frame_its_own_copy)
{
    Sprite s = frames(2);
    History h;
    s.layer(0)->cel(0)->set(0, 0, RED);
    cel_op_link(h, *s.layer(0), 0, 1);
    ASSERT_TRUE(cel_op_unlink(h, *s.layer(0), 1));
    ASSERT_TRUE(s.layer(0)->cel(0) != s.layer(0)->cel(1));
    ASSERT_TRUE(!s.layer(0)->celLinked(0));
    ASSERT_EQ_INT(255, s.layer(0)->cel(1)->get(0, 0).r);
    s.layer(0)->cel(1)->set(0, 0, BLUE);
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(0, 0).r);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.layer(0)->cel(0) == s.layer(0)->cel(1));
}

TEST(unlinking_an_unlinked_cel_records_nothing)
{
    Sprite s = frames(2);
    History h;
    ASSERT_TRUE(cel_op_unlink(h, *s.layer(0), 0));
    ASSERT_TRUE(!h.canUndo());
    ASSERT_TRUE(!cel_op_unlink(h, *s.layer(0), 4));
}

TEST(stroke_steps_stay_valid_across_link_and_unlink)
{
    Sprite s = frames(2);
    History h;
    h.beginStroke(*s.layer(0)->cel(1));
    h.setPixel(0, 0, RED);
    h.endStroke();
    cel_op_link(h, *s.layer(0), 0, 1); /* frame 1's painted cel goes away, but is kept */
    h.undo();
    h.undo();
    ASSERT_EQ_INT(0, s.layer(0)->cel(1)->get(0, 0).a);
    h.redo();
    h.redo();
    ASSERT_TRUE(s.layer(0)->cel(1) == s.layer(0)->cel(0));
}

TEST(duplicating_a_frame_copies_instead_of_linking)
{
    Sprite s = frames(1);
    History h;
    frame_op_duplicate(h, s, 0);
    ASSERT_TRUE(s.layer(0)->cel(0) != s.layer(0)->cel(1));
}

TEST(removing_a_linked_frame_keeps_the_other_one_and_undo_relinks)
{
    Sprite s = frames(3);
    History h;
    cel_op_link(h, *s.layer(0), 0, 2);
    ASSERT_TRUE(frame_op_remove(h, s, 2));
    ASSERT_TRUE(!s.layer(0)->celLinked(0));
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.layer(0)->celLinked(0));
    ASSERT_TRUE(s.layer(0)->cel(0) == s.layer(0)->cel(2));
}

TEST(duplicating_a_layer_keeps_its_links)
{
    Sprite s = frames(3);
    History h;
    cel_op_link(h, *s.layer(0), 0, 2);
    ASSERT_TRUE(layer_op_duplicate(h, s, 0) != nullptr);
    Layer &copy = *s.layer(1);
    ASSERT_TRUE(copy.cel(0) == copy.cel(2));
    ASSERT_TRUE(copy.cel(0) != copy.cel(1));
    ASSERT_TRUE(copy.cel(0) != s.layer(0)->cel(0));
}

TEST(merging_onto_a_linked_layer_unlinks_it_and_undo_relinks)
{
    Sprite s = frames(2);
    History h;
    s.layer(0)->cel(0)->set(0, 0, RED);
    cel_op_link(h, *s.layer(0), 0, 1);
    s.addLayer("Top");
    s.layer(1)->cel(1)->set(1, 1, BLUE); /* only on frame 1 */
    ASSERT_TRUE(layer_op_merge_down(h, s, 1));
    ASSERT_EQ_INT(1, s.layerCount());
    Layer &low = *s.layer(0);
    ASSERT_TRUE(low.cel(0) != low.cel(1));
    ASSERT_EQ_INT(0, low.cel(0)->get(1, 1).a);
    ASSERT_EQ_INT(255, low.cel(1)->get(1, 1).b);
    ASSERT_EQ_INT(255, low.cel(0)->get(0, 0).r);
    ASSERT_EQ_INT(255, low.cel(1)->get(0, 0).r);
    ASSERT_TRUE(h.undo());
    ASSERT_TRUE(s.layer(0)->cel(0) == s.layer(0)->cel(1));
    ASSERT_EQ_INT(0, s.layer(0)->cel(1)->get(1, 1).a);
    ASSERT_TRUE(h.redo());
    ASSERT_EQ_INT(255, s.layer(0)->cel(1)->get(1, 1).b);
}

TEST(links_round_trip_through_the_file)
{
    Sprite s = frames(4);
    History h;
    s.layer(0)->cel(0)->set(0, 0, RED);
    cel_op_link(h, *s.layer(0), 0, 2);
    cel_op_link(h, *s.layer(0), 0, 3);
    s.addLayer("Top");
    cel_op_link(h, *s.layer(1), 1, 2);
    std::optional<Sprite> back = pxa_decode(pxa_encode(s));
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    Layer &a = *back->layer(0);
    ASSERT_TRUE(a.cel(0) == a.cel(2));
    ASSERT_TRUE(a.cel(0) == a.cel(3));
    ASSERT_TRUE(a.cel(0) != a.cel(1));
    ASSERT_EQ_INT(255, a.cel(3)->get(0, 0).r);
    Layer &b = *back->layer(1);
    ASSERT_TRUE(b.cel(1) == b.cel(2));
    ASSERT_TRUE(b.cel(0) != b.cel(1));
}

TEST(a_sprite_without_links_writes_no_link_chunk)
{
    std::vector<uint8_t> data = pxa_encode(frames(3));
    for (size_t i = 0; i + 4 <= data.size(); i++) {
        ASSERT_TRUE(
            !(data[i] == 'L' && data[i + 1] == 'I' && data[i + 2] == 'N' && data[i + 3] == 'K'));
    }
}

TEST(a_link_to_a_later_frame_is_rejected)
{
    Sprite s = frames(3);
    History h;
    cel_op_link(h, *s.layer(0), 0, 2);
    std::vector<uint8_t> data = pxa_encode(s);
    /* LINK is the last chunk: layer u32, count u32, then (frame u32, source u32) */
    size_t pair = data.size() - 8;
    data[pair] = 0;     /* frame 0 ... */
    data[pair + 4] = 2; /* ... shares with frame 2, which comes later */
    ASSERT_TRUE(!pxa_decode(data));
}

TEST(a_link_for_a_missing_layer_is_rejected)
{
    Sprite s = frames(2);
    History h;
    cel_op_link(h, *s.layer(0), 0, 1);
    std::vector<uint8_t> data = pxa_encode(s);
    size_t layer = data.size() - 8 - 8;
    data[layer] = 9;
    ASSERT_TRUE(!pxa_decode(data));
}

int main()
{
    RUN(cels_start_unlinked);
    RUN(linking_makes_two_frames_share_one_cel);
    RUN(a_stroke_on_one_linked_frame_shows_on_the_other);
    RUN(linking_and_unlinking_are_refused_while_a_stroke_is_open);
    RUN(link_is_undoable_and_brings_back_the_old_cel);
    RUN(linking_what_is_already_linked_records_nothing);
    RUN(bad_links_are_refused);
    RUN(unlink_gives_the_frame_its_own_copy);
    RUN(unlinking_an_unlinked_cel_records_nothing);
    RUN(stroke_steps_stay_valid_across_link_and_unlink);
    RUN(duplicating_a_frame_copies_instead_of_linking);
    RUN(removing_a_linked_frame_keeps_the_other_one_and_undo_relinks);
    RUN(duplicating_a_layer_keeps_its_links);
    RUN(merging_onto_a_linked_layer_unlinks_it_and_undo_relinks);
    RUN(links_round_trip_through_the_file);
    RUN(a_sprite_without_links_writes_no_link_chunk);
    RUN(a_link_to_a_later_frame_is_rejected);
    RUN(a_link_for_a_missing_layer_is_rejected);
    return TEST_SUMMARY();
}
