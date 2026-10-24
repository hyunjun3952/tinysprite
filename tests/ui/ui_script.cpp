/* The input script of a headless run (`--script`): the commands expand into one input per frame.
 * Plain functions of their inputs, no ImGui frame is needed. */
#include <string>
#include <vector>

#include "imgui.h"
#include "minitest.hpp"
#include "script.hpp"
#include "ui_harness.hpp"

/* All the frames a script expands to. */
static std::vector<ScriptInput> expand(const std::string &text, std::string &error)
{
    Script script;
    std::vector<ScriptInput> frames;
    if (!script.parse(text, error)) {
        return frames;
    }
    while (!script.done()) {
        frames.push_back(script.next());
    }
    return frames;
}

TEST(a_click_moves_presses_and_releases_on_three_frames)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("click 40 100", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() == 3);
    ASSERT_TRUE(frames[0].moved && frames[0].x == 40.0f && frames[0].y == 100.0f);
    ASSERT_TRUE(frames[0].button == -1);
    ASSERT_TRUE(frames[1].button == 0 && frames[1].buttonDown);
    ASSERT_TRUE(frames[2].button == 0 && !frames[2].buttonDown);
    ASSERT_TRUE(frames[2].x == 40.0f && frames[2].y == 100.0f);
}

TEST(a_right_click_uses_the_right_button)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("rclick 5 6", error);
    ASSERT_TRUE(frames.size() == 3);
    ASSERT_TRUE(frames[1].button == 1 && frames[1].buttonDown);
    ASSERT_TRUE(frames[2].button == 1 && !frames[2].buttonDown);
}

TEST(a_drag_presses_at_the_start_and_releases_at_the_end)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("drag 10 20 90 60", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() > 4);
    ASSERT_TRUE(frames.front().x == 10.0f && frames.front().y == 20.0f);
    ASSERT_TRUE(frames[1].button == 0 && frames[1].buttonDown);
    ASSERT_TRUE(frames[1].x == 10.0f);
    ASSERT_TRUE(frames.back().button == 0 && !frames.back().buttonDown);
    ASSERT_TRUE(frames.back().x == 90.0f && frames.back().y == 60.0f);
    /* it moves in steps, so a panel sees a drag and not a jump */
    ASSERT_TRUE(frames[3].x > 10.0f && frames[3].x < 90.0f);
}

/* ImGui spreads the input over frames, so a modifier is held a frame before the key goes down and
 * a frame after it comes up: the key never sees a modifier change in its own frame. */
TEST(a_key_is_pressed_between_frames_that_hold_its_modifiers)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("key ctrl+shift+Z", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() == 4);
    ASSERT_TRUE(frames[0].key.empty() && frames[0].ctrl && frames[0].shift && !frames[0].alt);
    ASSERT_TRUE(frames[1].key == "Z" && frames[1].keyDown && frames[1].ctrl && frames[1].shift);
    ASSERT_TRUE(frames[2].key == "Z" && !frames[2].keyDown && frames[2].ctrl && frames[2].shift);
    ASSERT_TRUE(frames[3].key.empty() && !frames[3].ctrl && !frames[3].shift);
}

TEST(a_key_without_modifiers_takes_two_frames)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("key Escape", error);
    ASSERT_TRUE(frames.size() == 2);
    ASSERT_TRUE(frames[0].key == "Escape" && frames[0].keyDown && !frames[0].ctrl);
    ASSERT_TRUE(frames[1].key == "Escape" && !frames[1].keyDown);
}

TEST(a_path_presses_at_the_first_point_goes_through_the_others_and_releases_at_the_last)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("path 10 10 50 10 50 40", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() > 6);
    ASSERT_TRUE(frames.front().moved && frames.front().x == 10.0f && frames.front().y == 10.0f);
    ASSERT_TRUE(frames[1].button == 0 && frames[1].buttonDown && frames[1].x == 10.0f);
    bool corner = false;
    for (const ScriptInput &frame : frames) {
        corner = corner || (frame.x == 50.0f && frame.y == 10.0f && frame.buttonDown == false &&
                            frame.button == -1 && frame.moved);
    }
    ASSERT_TRUE(corner); /* the mouse reaches the middle point with the button still down */
    ASSERT_TRUE(frames.back().button == 0 && !frames.back().buttonDown);
    ASSERT_TRUE(frames.back().x == 50.0f && frames.back().y == 40.0f);
    ASSERT_TRUE(expand("path 1 2 3", error).empty()); /* a pair is a point: an odd count is not */
    ASSERT_TRUE(expand("path 1 2", error).empty());   /* one point is not a path */
}

TEST(wait_gives_empty_frames_and_shot_names_a_file)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("wait 3\nshot out dir/a.png", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() == 4);
    ASSERT_TRUE(!frames[0].moved && frames[0].button == -1 && frames[0].key.empty());
    ASSERT_TRUE(frames[3].shot == "out dir/a.png"); /* the rest of the line, spaces kept */
}

TEST(sleep_waits_real_time_after_one_frame)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("sleep 250", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() == 1);
    ASSERT_TRUE(frames[0].sleepMs == 250);
    ASSERT_TRUE(expand("sleep -1", error).empty());
    ASSERT_TRUE(expand("sleep 600000", error).empty());
}

TEST(type_sends_the_rest_of_the_line_as_text)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("type Layer 2", error);
    ASSERT_TRUE(frames.size() == 1);
    ASSERT_TRUE(frames[0].text == "Layer 2");
}

TEST(comments_and_blank_lines_are_ignored)
{
    std::string error;
    std::vector<ScriptInput> frames = expand("# a comment\n\n  \nwait 1  # trailing\r\n", error);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(frames.size() == 1);
}

TEST(a_bad_line_is_refused_with_its_number)
{
    std::string error;
    ASSERT_TRUE(expand("wait 1\nhover 1 2", error).empty());
    ASSERT_TRUE(error.find("line 2") != std::string::npos);
    ASSERT_TRUE(error.find("hover") != std::string::npos);
    ASSERT_TRUE(expand("click 4", error).empty());
    ASSERT_TRUE(error.find("line 1") != std::string::npos);
    ASSERT_TRUE(expand("click a b", error).empty());
    ASSERT_TRUE(expand("wait -2", error).empty());
    ASSERT_TRUE(expand("key ctrl+", error).empty());
    ASSERT_TRUE(expand("shot", error).empty());
}

TEST(next_after_the_end_gives_an_empty_input)
{
    Script script;
    std::string error;
    ASSERT_TRUE(script.parse("wait 1", error));
    script.next();
    ASSERT_TRUE(script.done());
    ScriptInput input = script.next();
    ASSERT_TRUE(!input.moved && input.shot.empty());
}

/* Runs a script through script_feed and the harness's ImGui frames, and counts the frames in which
 * `key` was pressed, noting whether the modifiers ImGui saw on those frames were the held ones. */
struct KeyRun {
    int pressed = 0;
    bool ctrl = false;
    bool shift = false;
};

static KeyRun run_key(const std::string &commands, ImGuiKey key)
{
    test_ui::Harness h;
    KeyRun run;
    h.body = [&] {
        if (ImGui::IsKeyPressed(key, false)) {
            run.pressed++;
            run.ctrl = ImGui::GetIO().KeyCtrl;
            run.shift = ImGui::GetIO().KeyShift;
        }
    };
    Script script;
    std::string error;
    script.parse(commands, error);
    for (int i = 0; i < 12; i++) {
        script_feed(ImGui::GetIO(), script.next());
        h.frame(10.0f, 10.0f, false);
    }
    return run;
}

TEST(a_scripted_key_is_pressed_once_with_its_modifiers_held)
{
    KeyRun run = run_key("key ctrl+shift+N", ImGuiKey_N);
    ASSERT_TRUE(run.pressed == 1);
    ASSERT_TRUE(run.ctrl && run.shift);
    KeyRun plain = run_key("key N", ImGuiKey_N);
    ASSERT_TRUE(plain.pressed == 1);
    ASSERT_TRUE(!plain.ctrl && !plain.shift);
}

TEST(a_scripted_key_is_found_by_its_imgui_name)
{
    ASSERT_TRUE(run_key("key Escape", ImGuiKey_Escape).pressed == 1);
    ASSERT_TRUE(run_key("key F1", ImGuiKey_F1).pressed == 1);
    ASSERT_TRUE(run_key("key Nonsense", ImGuiKey_N).pressed == 0);
}

int main()
{
    RUN(a_click_moves_presses_and_releases_on_three_frames);
    RUN(a_right_click_uses_the_right_button);
    RUN(a_drag_presses_at_the_start_and_releases_at_the_end);
    RUN(a_key_is_pressed_between_frames_that_hold_its_modifiers);
    RUN(a_key_without_modifiers_takes_two_frames);
    RUN(a_path_presses_at_the_first_point_goes_through_the_others_and_releases_at_the_last);
    RUN(wait_gives_empty_frames_and_shot_names_a_file);
    RUN(sleep_waits_real_time_after_one_frame);
    RUN(type_sends_the_rest_of_the_line_as_text);
    RUN(comments_and_blank_lines_are_ignored);
    RUN(a_bad_line_is_refused_with_its_number);
    RUN(next_after_the_end_gives_an_empty_input);
    RUN(a_scripted_key_is_pressed_once_with_its_modifiers_held);
    RUN(a_scripted_key_is_found_by_its_imgui_name);
    return TEST_SUMMARY();
}
