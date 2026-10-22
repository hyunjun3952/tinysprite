/* Which layers are selected: the active one plus those picked with Ctrl / Shift, tracked by
 * identity so they survive the stack being reordered. No window is needed. */
#include <vector>

#include "layer_selection.hpp"
#include "minitest.hpp"

static Sprite stack(int layers)
{
    Sprite s(4, 4);
    while (s.layerCount() < layers) {
        s.addLayer("L");
    }
    return s;
}

static bool is(const std::vector<int> &got, const std::vector<int> &want)
{
    return got == want;
}

TEST(a_plain_pick_selects_only_that_layer)
{
    Sprite s = stack(4);
    LayerSelection sel;
    sel.pick(s, 2, LayerPick::Only);
    ASSERT_EQ_INT(2, sel.active());
    ASSERT_TRUE(is(sel.indexes(s), {2}));
    ASSERT_TRUE(sel.contains(s, 2));
    ASSERT_TRUE(!sel.contains(s, 1));
}

TEST(picking_a_missing_layer_changes_nothing)
{
    Sprite s = stack(2);
    LayerSelection sel;
    sel.pick(s, 1, LayerPick::Only);
    sel.pick(s, 5, LayerPick::Only);
    sel.pick(s, -1, LayerPick::Toggle);
    ASSERT_EQ_INT(1, sel.active());
    ASSERT_TRUE(is(sel.indexes(s), {1}));
}

TEST(toggle_adds_a_layer_and_removes_it_again)
{
    Sprite s = stack(4);
    LayerSelection sel;
    sel.pick(s, 1, LayerPick::Only);
    sel.pick(s, 3, LayerPick::Toggle);
    ASSERT_TRUE(is(sel.indexes(s), {1, 3}));
    ASSERT_EQ_INT(3, sel.active());
    sel.pick(s, 1, LayerPick::Toggle);
    ASSERT_TRUE(is(sel.indexes(s), {3}));
}

TEST(toggle_never_removes_the_last_selected_layer)
{
    Sprite s = stack(3);
    LayerSelection sel;
    sel.pick(s, 1, LayerPick::Only);
    sel.pick(s, 1, LayerPick::Toggle);
    ASSERT_TRUE(is(sel.indexes(s), {1}));
}

TEST(toggling_the_active_layer_off_hands_over_to_the_topmost_other)
{
    Sprite s = stack(4);
    LayerSelection sel;
    sel.pick(s, 0, LayerPick::Only);
    sel.pick(s, 1, LayerPick::Toggle);
    sel.pick(s, 2, LayerPick::Toggle); /* active is now 2 */
    sel.pick(s, 2, LayerPick::Toggle);
    ASSERT_TRUE(is(sel.indexes(s), {0, 1}));
    ASSERT_EQ_INT(1, sel.active());
}

TEST(toggling_the_only_selected_layer_changes_nothing)
{
    Sprite s = stack(3);
    LayerSelection sel;
    sel.pick(s, 2, LayerPick::Only);
    sel.pick(s, 2, LayerPick::Toggle);
    ASSERT_EQ_INT(2, sel.active());
    ASSERT_TRUE(is(sel.indexes(s), {2}));
    sel.pick(s, 0, LayerPick::Toggle);
    sel.pick(s, 0, LayerPick::Toggle);
    sel.pick(s, 0, LayerPick::Toggle); /* added, removed and added again: selected once */
    ASSERT_TRUE(is(sel.indexes(s), {0, 2}));
}

TEST(a_range_includes_both_ends_and_the_clicked_layer_stays_active)
{
    Sprite s = stack(5);
    LayerSelection sel;
    sel.pick(s, 1, LayerPick::Only);
    sel.pick(s, 3, LayerPick::Range);
    ASSERT_TRUE(is(sel.indexes(s), {1, 2, 3}));
    ASSERT_EQ_INT(3, sel.active());
    sel.pick(s, 3, LayerPick::Toggle); /* the active one leaves; the topmost other takes over */
    ASSERT_TRUE(is(sel.indexes(s), {1, 2}));
    ASSERT_EQ_INT(2, sel.active());
}

TEST(range_selects_from_the_anchor_to_the_layer)
{
    Sprite s = stack(5);
    LayerSelection sel;
    sel.pick(s, 3, LayerPick::Only);
    sel.pick(s, 1, LayerPick::Range);
    ASSERT_TRUE(is(sel.indexes(s), {1, 2, 3}));
    ASSERT_EQ_INT(1, sel.active());
    sel.pick(s, 4, LayerPick::Range); /* the anchor stays where the plain pick was */
    ASSERT_TRUE(is(sel.indexes(s), {3, 4}));
}

TEST(a_move_keeps_the_active_index_on_the_same_layer)
{
    Sprite s = stack(4);
    LayerSelection sel;
    sel.pick(s, 1, LayerPick::Only);
    sel.moved(1, 3); /* the active layer itself moved */
    ASSERT_EQ_INT(3, sel.active());
    sel.moved(0, 3); /* a layer from below went past it */
    ASSERT_EQ_INT(2, sel.active());
    sel.moved(3, 0); /* a layer from above went below it */
    ASSERT_EQ_INT(3, sel.active());
    sel.moved(3, 3);
    ASSERT_EQ_INT(3, sel.active());
}

TEST(refresh_forgets_layers_that_left_the_sprite)
{
    Sprite s = stack(3);
    LayerSelection sel;
    sel.pick(s, 0, LayerPick::Only);
    sel.pick(s, 2, LayerPick::Range);
    ASSERT_TRUE(is(sel.indexes(s), {0, 1, 2}));
    std::unique_ptr<Layer> gone = s.detachLayer(2);
    sel.refresh(s, nullptr);
    ASSERT_TRUE(is(sel.indexes(s), {0, 1}));
    ASSERT_EQ_INT(1, sel.active()); /* clamped into the shorter stack */
}

TEST(refresh_follows_the_given_layer_to_its_new_index)
{
    Sprite s = stack(3);
    LayerSelection sel;
    sel.pick(s, 0, LayerPick::Only);
    const Layer *layer = s.layer(0);
    s.moveLayer(0, 2);
    sel.refresh(s, layer);
    ASSERT_EQ_INT(2, sel.active());
}

TEST(set_picked_makes_those_layers_part_of_the_selection)
{
    Sprite s = stack(4);
    LayerSelection sel;
    sel.pick(s, 3, LayerPick::Only);
    sel.setPicked({s.layer(0), s.layer(1)});
    ASSERT_TRUE(is(sel.indexes(s), {0, 1, 3}));
}

TEST(reset_goes_back_to_the_first_layer)
{
    Sprite s = stack(3);
    LayerSelection sel;
    sel.pick(s, 2, LayerPick::Only);
    sel.pick(s, 0, LayerPick::Toggle);
    sel.reset();
    ASSERT_EQ_INT(0, sel.active());
    ASSERT_TRUE(is(sel.indexes(s), {0}));
}

int main()
{
    RUN(a_plain_pick_selects_only_that_layer);
    RUN(picking_a_missing_layer_changes_nothing);
    RUN(toggle_adds_a_layer_and_removes_it_again);
    RUN(toggle_never_removes_the_last_selected_layer);
    RUN(toggling_the_active_layer_off_hands_over_to_the_topmost_other);
    RUN(toggling_the_only_selected_layer_changes_nothing);
    RUN(a_range_includes_both_ends_and_the_clicked_layer_stays_active);
    RUN(range_selects_from_the_anchor_to_the_layer);
    RUN(a_move_keeps_the_active_index_on_the_same_layer);
    RUN(refresh_forgets_layers_that_left_the_sprite);
    RUN(refresh_follows_the_given_layer_to_its_new_index);
    RUN(set_picked_makes_those_layers_part_of_the_selection);
    RUN(reset_goes_back_to_the_first_layer);
    return TEST_SUMMARY();
}
