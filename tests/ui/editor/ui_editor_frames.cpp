/* Headless tests for the UI editor state: the current frame, frame operations and playback.
 * Built only when the UI is built; runs under SDL_VIDEODRIVER=dummy. */

#include "editor.hpp"
#include "minitest.hpp"
#include "tinysprite/tag_ops.hpp"
#include "ui_editor_support.hpp"

TEST(a_new_document_shows_frame_zero)
{
    Editor *e = make_editor();
    ASSERT_EQ_INT(0, e->frame());
    ASSERT_TRUE(!e->setFrame(1));
    ASSERT_TRUE(!e->setFrame(-1));
    ASSERT_TRUE(e->setFrame(0));
    delete e;
}

TEST(add_frame_goes_after_the_current_one_and_selects_it)
{
    Editor *e = make_editor();
    e->addFrame();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    ASSERT_EQ_INT(1, e->frame());
    e->setFrame(0);
    e->addFrame();
    ASSERT_EQ_INT(3, e->sprite().frameCount());
    ASSERT_EQ_INT(1, e->frame());
    delete e;
}

TEST(drawing_lands_on_the_current_frame_only)
{
    Editor *e = make_editor();
    e->addFrame();
    dot(e, 2, 2);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    e->setFrame(0);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    e->setFrame(1);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    delete e;
}

TEST(the_canvas_follows_the_frame)
{
    Editor *e = make_editor();
    e->addFrame();
    dot(e, 1, 1);
    e->updateTexture();
    e->setFrame(0);
    ASSERT_TRUE(e->dirty());
    delete e;
}

TEST(undoing_an_added_frame_keeps_the_current_frame_valid)
{
    Editor *e = make_editor();
    e->addFrame();
    ASSERT_EQ_INT(1, e->frame());
    e->undo();
    ASSERT_EQ_INT(1, e->sprite().frameCount());
    ASSERT_EQ_INT(0, e->frame());
    ASSERT_TRUE(e->activeCel() != nullptr);
    e->redo();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    delete e;
}

TEST(duplicate_selects_the_copy)
{
    Editor *e = make_editor();
    dot(e, 3, 3);
    e->duplicateFrame();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    ASSERT_EQ_INT(1, e->frame());
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    delete e;
}

TEST(remove_selects_a_neighbour_and_refuses_the_last_frame)
{
    Editor *e = make_editor();
    e->removeFrame(0);
    ASSERT_EQ_INT(1, e->sprite().frameCount());
    e->addFrame();
    e->addFrame();
    ASSERT_EQ_INT(2, e->frame());
    e->removeFrame(2);
    ASSERT_EQ_INT(1, e->frame());
    e->setFrame(0);
    e->removeFrame(0);
    ASSERT_EQ_INT(0, e->frame());
    ASSERT_EQ_INT(1, e->sprite().frameCount());
    e->undo();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    delete e;
}

TEST(removing_an_earlier_frame_keeps_the_same_frame_selected)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    dot(e, 5, 5); /* on frame 2 */
    e->removeFrame(0);
    ASSERT_EQ_INT(1, e->frame());
    ASSERT_EQ_INT(255, alpha(e, 5, 5));
    delete e;
}

TEST(a_frame_beyond_the_cel_budget_is_refused_and_reported)
{
    Editor *e = make_editor();
    e->sprite().setCelBudget(8ULL * 6 * 4 * 2); /* two cels */
    e->addFrame();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    ASSERT_TRUE(!e->takeMemoryShort());
    e->addFrame();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    ASSERT_EQ_INT(1, e->frame());
    ASSERT_TRUE(e->takeMemoryShort());
    e->duplicateFrame();
    ASSERT_EQ_INT(2, e->sprite().frameCount());
    ASSERT_TRUE(e->takeMemoryShort());
    delete e;
}

TEST(removing_the_current_frame_moves_on_to_the_one_that_followed_it)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    dot(e, 3, 3); /* the marked frame is the last one */
    e->setFrame(1);
    e->removeFrame(1);
    ASSERT_EQ_INT(1, e->frame()); /* the marked frame slid into the removed one's place */
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    delete e;
}

TEST(removing_a_later_frame_leaves_the_current_number_alone)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    e->setFrame(1);
    e->removeFrame(2);
    ASSERT_EQ_INT(1, e->frame());
    delete e;
}

TEST(move_keeps_the_selection_on_the_same_frame)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    dot(e, 0, 0); /* frame 2 is the marked one */
    e->moveFrame(2, 0);
    ASSERT_EQ_INT(0, e->frame());
    ASSERT_EQ_INT(255, alpha(e, 0, 0));
    e->setFrame(2);
    e->moveFrame(0, 2); /* the marked frame goes back; the selected one shifts down */
    ASSERT_EQ_INT(1, e->frame());
    delete e;
}

TEST(moving_a_frame_up_onto_the_current_one_pushes_it_down)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    e->setFrame(0);
    dot(e, 0, 0); /* the marked frame is the current one, at the top of the range */
    e->moveFrame(2, 0);
    ASSERT_EQ_INT(1, e->frame());
    ASSERT_EQ_INT(255, alpha(e, 0, 0));
    e->setFrame(1);
    e->moveFrame(1, 0); /* up from the current frame itself */
    ASSERT_EQ_INT(0, e->frame());
    delete e;
}

TEST(duration_edits_are_undoable)
{
    Editor *e = make_editor();
    e->setFrameDuration(0, 500);
    ASSERT_EQ_INT(500, e->sprite().frameDuration(0));
    e->undo();
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, e->sprite().frameDuration(0));
    delete e;
}

TEST(frame_edits_are_held_off_while_stroking)
{
    Editor *e = make_editor();
    e->selectTool(ToolKind::Pencil);
    e->press(1, 1);
    e->addFrame();
    ASSERT_EQ_INT(1, e->sprite().frameCount());
    e->release();
    delete e;
}

TEST(a_new_document_goes_back_to_frame_zero)
{
    Editor *e = make_editor();
    e->addFrame();
    e->newDocument(4, 4);
    ASSERT_EQ_INT(0, e->frame());
    delete e;
}

TEST(playback_steps_through_the_frames_by_their_durations)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    for (int f = 0; f < 3; f++) {
        e->setFrameDuration(f, 100);
    }
    e->setFrame(0);
    ASSERT_TRUE(!e->playing());
    e->togglePlayback();
    ASSERT_TRUE(e->playing());
    e->tick(100);
    ASSERT_EQ_INT(1, e->frame());
    e->tick(250);
    ASSERT_EQ_INT(0, e->frame()); /* 2, then 0 again, 50 ms into it */
    e->togglePlayback();
    ASSERT_TRUE(!e->playing());
    e->tick(1000);
    ASSERT_EQ_INT(0, e->frame());
    delete e;
}

TEST(a_press_while_playing_stops_playback_and_draws_nothing)
{
    Editor *e = make_editor();
    e->addFrame();
    e->setFrame(0);
    e->togglePlayback();
    dot(e, 2, 2);
    ASSERT_TRUE(!e->playing());
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    dot(e, 2, 2);
    ASSERT_EQ_INT(255, alpha(e, 2, 2));
    delete e;
}

TEST(frame_edits_and_undo_stop_playback)
{
    Editor *e = make_editor();
    e->togglePlayback();
    e->addFrame();
    ASSERT_TRUE(!e->playing());
    e->togglePlayback();
    e->undo();
    ASSERT_TRUE(!e->playing());
    delete e;
}

TEST(playback_with_one_frame_stays_on_it)
{
    Editor *e = make_editor();
    e->togglePlayback();
    e->tick(5000);
    ASSERT_EQ_INT(0, e->frame());
    delete e;
}

TEST(onion_skin_shows_the_previous_frame_under_the_current_one)
{
    Editor *e = make_editor();
    dot(e, 2, 2); /* frame 0 */
    e->addFrame();
    e->updateTexture();
    ASSERT_EQ_INT(0, e->shownPixel(2, 2).a);
    e->setOnion(true, OnionSkin{1, 1, 100});
    e->updateTexture();
    ASSERT_TRUE(e->onionEnabled());
    ASSERT_TRUE(e->shownPixel(2, 2).a > 0);
    e->setOnion(false, OnionSkin{1, 1, 100});
    e->updateTexture();
    ASSERT_EQ_INT(0, e->shownPixel(2, 2).a);
    delete e;
}

TEST(onion_skin_follows_edits_of_the_current_frame)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    e->addFrame();
    e->setOnion(true, OnionSkin{1, 0, 100});
    e->updateTexture();
    dot(e, 2, 2); /* the current frame now covers its own ghost */
    e->updateTexture();
    ASSERT_EQ_INT(255, e->shownPixel(2, 2).a);
    delete e;
}

TEST(onion_skin_is_hidden_while_playing)
{
    Editor *e = make_editor();
    dot(e, 2, 2);
    e->addFrame();
    e->setOnion(true, OnionSkin{1, 0, 100});
    e->updateTexture();
    ASSERT_TRUE(e->shownPixel(2, 2).a > 0);
    e->togglePlayback();
    e->updateTexture();
    ASSERT_EQ_INT(0, e->shownPixel(2, 2).a);
    e->togglePlayback();
    e->updateTexture();
    ASSERT_TRUE(e->shownPixel(2, 2).a > 0);
    delete e;
}

TEST(onion_skin_settings_survive_a_settings_round_trip)
{
    Editor *e = make_editor();
    e->setOnion(true, OnionSkin{3, 2, 40});
    SettingsMap settings;
    e->store(settings);
    Editor *f = make_editor();
    f->load(settings);
    ASSERT_TRUE(f->onionEnabled());
    ASSERT_EQ_INT(3, f->onionSkin().before);
    ASSERT_EQ_INT(2, f->onionSkin().after);
    ASSERT_EQ_INT(40, f->onionSkin().opacity);
    delete e;
    delete f;
}

TEST(onion_skin_settings_are_limited)
{
    Editor *e = make_editor();
    e->setOnion(true, OnionSkin{-4, 999, 255});
    ASSERT_EQ_INT(0, e->onionSkin().before);
    ASSERT_EQ_INT(ONION_MAX_FRAMES, e->onionSkin().after);
    delete e;
}

TEST(frames_stop_at_the_limit_and_say_so)
{
    Editor *e = make_editor();
    while (e->sprite().frameCount() < SPRITE_MAX_FRAMES) {
        e->addFrame();
    }
    ASSERT_TRUE(!e->takeFramesFull());
    e->addFrame();
    ASSERT_EQ_INT(SPRITE_MAX_FRAMES, e->sprite().frameCount());
    ASSERT_TRUE(e->takeFramesFull());
    ASSERT_TRUE(!e->takeMemoryShort());
    e->duplicateFrame();
    ASSERT_TRUE(e->takeFramesFull());
    delete e;
}

static void tag_named(Editor *e, const char *name, int from, int to)
{
    Sprite::Tag t;
    t.name = name;
    t.from = from;
    t.to = to;
    tag_op_add(e->history(), e->sprite(), t);
}

static std::string selected_tag_name(Editor *e)
{
    int i = e->selectedTag();
    return i >= 0 && i < static_cast<int>(e->sprite().tags().size())
               ? e->sprite().tags()[static_cast<size_t>(i)].name
               : "";
}

TEST(the_selected_tag_stays_the_same_tag_when_a_frame_removes_an_earlier_one)
{
    Editor *e = make_editor();
    for (int i = 0; i < 5; i++) {
        e->addFrame();
    }
    tag_named(e, "A", 3, 3);
    tag_named(e, "B", 0, 1);
    tag_named(e, "C", 5, 5);
    e->selectTag(1);
    e->removeFrame(3);
    ASSERT_TRUE(selected_tag_name(e) == "B");
    e->undo();
    ASSERT_TRUE(selected_tag_name(e) == "B");
    e->redo();
    ASSERT_TRUE(selected_tag_name(e) == "B");
    e->selectTag(1); /* C */
    e->removeFrame(0);
    ASSERT_TRUE(selected_tag_name(e) == "C");
    delete e;
}

TEST(the_selected_tag_stays_the_same_tag_across_undo_of_a_removal)
{
    Editor *e = make_editor();
    for (int i = 0; i < 3; i++) {
        e->addFrame();
    }
    tag_named(e, "A", 0, 0);
    tag_named(e, "B", 1, 1);
    e->selectTag(1);
    e->removeTag(0);
    ASSERT_EQ_INT(0, e->selectedTag());
    e->undo();
    ASSERT_TRUE(selected_tag_name(e) == "B");
    e->redo();
    ASSERT_TRUE(selected_tag_name(e) == "B");
    delete e;
}

TEST(a_selected_tag_that_vanishes_with_its_frame_selects_nothing)
{
    Editor *e = make_editor();
    e->addFrame();
    tag_named(e, "A", 1, 1);
    e->selectTag(0);
    e->removeFrame(1);
    ASSERT_EQ_INT(-1, e->selectedTag());
    delete e;
}

TEST(adding_a_tag_covers_the_current_frame_and_selects_it)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    e->setFrame(1);
    e->addTag();
    ASSERT_EQ_INT(1, static_cast<int>(e->sprite().tags().size()));
    ASSERT_EQ_INT(1, e->sprite().tags()[0].from);
    ASSERT_EQ_INT(1, e->sprite().tags()[0].to);
    ASSERT_EQ_INT(0, e->selectedTag());
    e->undo();
    ASSERT_EQ_INT(0, static_cast<int>(e->sprite().tags().size()));
    ASSERT_EQ_INT(-1, e->selectedTag());
    delete e;
}

TEST(tags_can_be_edited_removed_and_undone)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addTag();
    Sprite::Tag t = e->sprite().tags()[0];
    t.name = "run";
    t.from = 0;
    t.direction = PlayMode::Reverse;
    e->editTag(0, t);
    ASSERT_TRUE(e->sprite().tags()[0].name == "run");
    ASSERT_EQ_INT(0, e->sprite().tags()[0].from);
    e->removeTag(0);
    ASSERT_EQ_INT(-1, e->selectedTag());
    e->undo();
    ASSERT_TRUE(e->sprite().tags()[0].name == "run");
    delete e;
}

TEST(removing_an_earlier_tag_keeps_the_same_tag_selected)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    e->setFrame(0);
    e->addTag();
    e->setFrame(2);
    e->addTag();
    ASSERT_EQ_INT(1, e->selectedTag());
    e->removeTag(0);
    ASSERT_EQ_INT(0, e->selectedTag());
    ASSERT_EQ_INT(2, e->sprite().tags()[e->selectedTag()].from);
    delete e;
}

TEST(a_selected_tag_limits_playback_to_its_range)
{
    Editor *e = make_editor();
    for (int i = 0; i < 3; i++) {
        e->addFrame();
    }
    for (int f = 0; f < 4; f++) {
        e->setFrameDuration(f, 100);
    }
    e->setFrame(1);
    e->addTag();
    Sprite::Tag t = e->sprite().tags()[0];
    t.to = 2;
    e->editTag(0, t); /* frames 1..2 */
    e->setFrame(0);
    e->togglePlayback();
    ASSERT_EQ_INT(1, e->frame()); /* playback starts inside the tag */
    e->tick(100);
    ASSERT_EQ_INT(2, e->frame());
    e->tick(100);
    ASSERT_EQ_INT(1, e->frame());
    delete e;
}

TEST(without_a_selected_tag_the_whole_animation_plays)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addTag();
    e->selectTag(-1);
    e->setFrame(0);
    e->setFrameDuration(0, 100);
    e->setFrameDuration(1, 100);
    e->togglePlayback();
    e->tick(100);
    ASSERT_EQ_INT(1, e->frame());
    e->tick(100);
    ASSERT_EQ_INT(0, e->frame());
    delete e;
}

TEST(the_tag_direction_sets_the_playback_order)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    for (int f = 0; f < 3; f++) {
        e->setFrameDuration(f, 100);
    }
    e->setFrame(0);
    e->addTag();
    Sprite::Tag t = e->sprite().tags()[0];
    t.to = 2;
    t.direction = PlayMode::Reverse;
    e->editTag(0, t);
    e->setFrame(2);
    e->togglePlayback();
    e->tick(100);
    ASSERT_EQ_INT(1, e->frame());
    delete e;
}

TEST(selecting_a_tag_out_of_range_selects_nothing)
{
    Editor *e = make_editor();
    e->addTag();
    e->selectTag(5);
    ASSERT_EQ_INT(-1, e->selectedTag());
    e->selectTag(0);
    ASSERT_EQ_INT(0, e->selectedTag());
    e->newDocument(4, 4);
    ASSERT_EQ_INT(-1, e->selectedTag());
    delete e;
}

TEST(linked_cels_share_what_is_drawn)
{
    Editor *e = make_editor();
    e->addFrame();
    e->setFrame(0);
    e->linkCel(0, 0, 1);
    ASSERT_TRUE(e->sprite().layer(0)->celLinked(1));
    e->setFrame(1);
    dot(e, 3, 3);
    e->setFrame(0);
    ASSERT_EQ_INT(255, alpha(e, 3, 3));
    e->undo(); /* the dot */
    ASSERT_EQ_INT(0, alpha(e, 3, 3));
    e->undo(); /* the link */
    ASSERT_TRUE(!e->sprite().layer(0)->celLinked(0));
    delete e;
}

TEST(linking_redraws_the_canvas_and_unlinking_separates_the_frames)
{
    Editor *e = make_editor();
    dot(e, 1, 1);
    e->addFrame();
    e->updateTexture();
    e->linkCel(0, 0, 1);
    ASSERT_TRUE(e->dirty());
    ASSERT_EQ_INT(255, alpha(e, 1, 1));
    e->unlinkCel(0, 1);
    ASSERT_TRUE(!e->sprite().layer(0)->celLinked(0));
    dot(e, 2, 2);
    e->setFrame(0);
    ASSERT_EQ_INT(0, alpha(e, 2, 2));
    delete e;
}

TEST(link_to_previous_uses_the_active_layer_and_frame)
{
    Editor *e = make_editor();
    e->addFrame();
    e->addFrame();
    e->linkCelToPrevious();
    ASSERT_TRUE(e->sprite().layer(0)->cel(2) == e->sprite().layer(0)->cel(1));
    e->setFrame(0);
    e->linkCelToPrevious(); /* nothing before the first frame */
    ASSERT_TRUE(!e->sprite().layer(0)->celLinked(0));
    delete e;
}

TEST(a_locked_layer_keeps_its_cels_unlinked)
{
    Editor *e = make_editor();
    e->addFrame();
    e->setLayerLocked(0, true);
    e->linkCel(0, 0, 1);
    ASSERT_TRUE(!e->sprite().layer(0)->celLinked(0));
    delete e;
}

TEST(linking_is_held_off_while_stroking_and_stops_playback)
{
    Editor *e = make_editor();
    e->addFrame();
    e->selectTool(ToolKind::Pencil);
    e->press(0, 0);
    e->linkCel(0, 0, 1);
    ASSERT_TRUE(!e->sprite().layer(0)->celLinked(0));
    e->release();
    e->togglePlayback();
    e->linkCel(0, 0, 1);
    ASSERT_TRUE(!e->playing());
    ASSERT_TRUE(e->sprite().layer(0)->celLinked(0));
    delete e;
}

TEST(removing_the_playing_tag_makes_playback_cover_all_frames)
{
    Editor *e = make_editor();
    for (int i = 0; i < 3; i++) {
        e->addFrame();
    }
    e->setFrame(0);
    Sprite::Tag tag;
    tag.name = "a";
    tag.from = 0;
    tag.to = 1;
    ASSERT_TRUE(e->sprite().addTag(tag));
    e->selectTag(0);
    e->togglePlayback();
    e->removeTag(0);
    bool sawLast = false;
    for (int i = 0; i < 80; i++) {
        e->tick(100);
        sawLast = sawLast || e->frame() >= 2;
    }
    ASSERT_TRUE(sawLast);
    delete e;
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(a_new_document_shows_frame_zero);
    RUN(add_frame_goes_after_the_current_one_and_selects_it);
    RUN(drawing_lands_on_the_current_frame_only);
    RUN(the_canvas_follows_the_frame);
    RUN(undoing_an_added_frame_keeps_the_current_frame_valid);
    RUN(duplicate_selects_the_copy);
    RUN(remove_selects_a_neighbour_and_refuses_the_last_frame);
    RUN(removing_an_earlier_frame_keeps_the_same_frame_selected);
    RUN(a_frame_beyond_the_cel_budget_is_refused_and_reported);
    RUN(removing_the_current_frame_moves_on_to_the_one_that_followed_it);
    RUN(removing_a_later_frame_leaves_the_current_number_alone);
    RUN(move_keeps_the_selection_on_the_same_frame);
    RUN(moving_a_frame_up_onto_the_current_one_pushes_it_down);
    RUN(duration_edits_are_undoable);
    RUN(frame_edits_are_held_off_while_stroking);
    RUN(a_new_document_goes_back_to_frame_zero);
    RUN(playback_steps_through_the_frames_by_their_durations);
    RUN(a_press_while_playing_stops_playback_and_draws_nothing);
    RUN(frame_edits_and_undo_stop_playback);
    RUN(playback_with_one_frame_stays_on_it);
    RUN(onion_skin_shows_the_previous_frame_under_the_current_one);
    RUN(onion_skin_follows_edits_of_the_current_frame);
    RUN(onion_skin_is_hidden_while_playing);
    RUN(onion_skin_settings_survive_a_settings_round_trip);
    RUN(onion_skin_settings_are_limited);
    RUN(frames_stop_at_the_limit_and_say_so);
    RUN(the_selected_tag_stays_the_same_tag_when_a_frame_removes_an_earlier_one);
    RUN(the_selected_tag_stays_the_same_tag_across_undo_of_a_removal);
    RUN(a_selected_tag_that_vanishes_with_its_frame_selects_nothing);
    RUN(adding_a_tag_covers_the_current_frame_and_selects_it);
    RUN(tags_can_be_edited_removed_and_undone);
    RUN(removing_an_earlier_tag_keeps_the_same_tag_selected);
    RUN(a_selected_tag_limits_playback_to_its_range);
    RUN(without_a_selected_tag_the_whole_animation_plays);
    RUN(the_tag_direction_sets_the_playback_order);
    RUN(selecting_a_tag_out_of_range_selects_nothing);
    RUN(removing_the_playing_tag_makes_playback_cover_all_frames);
    RUN(linked_cels_share_what_is_drawn);
    RUN(linking_redraws_the_canvas_and_unlinking_separates_the_frames);
    RUN(link_to_previous_uses_the_active_layer_and_frame);
    RUN(a_locked_layer_keeps_its_cels_unlinked);
    RUN(linking_is_held_off_while_stroking_and_stops_playback);
    ui_quit();
    return TEST_SUMMARY();
}
