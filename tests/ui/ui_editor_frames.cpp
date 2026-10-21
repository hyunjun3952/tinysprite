/* Headless tests for the UI editor state: the current frame, frame operations and playback.
 * Built only when the UI is built; runs under SDL_VIDEODRIVER=dummy. */

#include "editor.h"
#include "minitest.h"
#include "ui_editor_support.h"

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

int main(void)
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
    RUN(move_keeps_the_selection_on_the_same_frame);
    RUN(duration_edits_are_undoable);
    RUN(frame_edits_are_held_off_while_stroking);
    RUN(a_new_document_goes_back_to_frame_zero);
    RUN(playback_steps_through_the_frames_by_their_durations);
    RUN(a_press_while_playing_stops_playback_and_draws_nothing);
    RUN(frame_edits_and_undo_stop_playback);
    RUN(playback_with_one_frame_stays_on_it);
    ui_quit();
    return TEST_SUMMARY();
}
