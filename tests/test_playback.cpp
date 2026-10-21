#include "minitest.h"
#include "tinysprite/playback.h"

/* A sprite with `n` frames whose durations are 100, 200, 300, ... milliseconds. */
static Sprite frames(int n)
{
    Sprite s(2, 2);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    for (int i = 0; i < n; i++) {
        s.setFrameDuration(i, 100 * (i + 1));
    }
    return s;
}

TEST(idle_playback_does_not_move)
{
    Sprite s = frames(3);
    Playback p;
    ASSERT_TRUE(!p.playing());
    ASSERT_EQ_INT(-1, p.advance(s, 1000));
}

TEST(forward_waits_for_each_duration)
{
    Sprite s = frames(3);
    Playback p;
    p.start(0, 0, 2, PlayMode::Forward);
    ASSERT_TRUE(p.playing());
    ASSERT_EQ_INT(0, p.advance(s, 99));
    ASSERT_EQ_INT(1, p.advance(s, 1));
    ASSERT_EQ_INT(1, p.advance(s, 199));
    ASSERT_EQ_INT(2, p.advance(s, 1));
}

TEST(forward_wraps_to_the_first_frame)
{
    Sprite s = frames(3);
    Playback p;
    p.start(2, 0, 2, PlayMode::Forward);
    ASSERT_EQ_INT(0, p.advance(s, 300));
    ASSERT_EQ_INT(1, p.advance(s, 100));
}

TEST(a_long_step_skips_several_frames_and_keeps_the_remainder)
{
    Sprite s = frames(3);
    Playback p;
    p.start(0, 0, 2, PlayMode::Forward);
    /* 100 + 200 + 50 into frame 2 (which lasts 300) */
    ASSERT_EQ_INT(2, p.advance(s, 350));
    ASSERT_EQ_INT(0, p.advance(s, 250));
}

TEST(a_huge_step_still_lands_on_a_frame)
{
    Sprite s = frames(3);
    Playback p;
    p.start(0, 0, 2, PlayMode::Forward);
    int f = p.advance(s, 2000000000);
    ASSERT_TRUE(f >= 0 && f <= 2);
}

TEST(reverse_runs_backwards_and_wraps)
{
    Sprite s = frames(3);
    Playback p;
    p.start(1, 0, 2, PlayMode::Reverse);
    ASSERT_EQ_INT(0, p.advance(s, 200));
    ASSERT_EQ_INT(2, p.advance(s, 100));
}

TEST(ping_pong_turns_at_the_ends_without_repeating_them)
{
    Sprite s = frames(3);
    s.setFrameDuration(0, 10);
    s.setFrameDuration(1, 10);
    s.setFrameDuration(2, 10);
    Playback p;
    p.start(0, 0, 2, PlayMode::PingPong);
    int seen[6];
    for (int &f : seen) {
        f = p.advance(s, 10);
    }
    const int want[6] = {1, 2, 1, 0, 1, 2};
    for (int i = 0; i < 6; i++) {
        ASSERT_EQ_INT(want[i], seen[i]);
    }
}

TEST(ping_pong_after_a_long_pause_keeps_its_phase)
{
    Sprite s = frames(3); /* 100, 200, 300 ms; one out-and-back cycle is 100+200+300+200 = 800 */
    Playback a;
    Playback b;
    a.start(0, 0, 2, PlayMode::PingPong);
    b.start(0, 0, 2, PlayMode::PingPong);
    int fa = a.advance(s, 800 * 1000 + 350);
    int fb = b.advance(s, 350);
    ASSERT_EQ_INT(fb, fa);
}

TEST(the_range_limits_playback)
{
    Sprite s = frames(4);
    Playback p;
    p.start(1, 1, 2, PlayMode::Forward);
    ASSERT_EQ_INT(2, p.advance(s, 200));
    ASSERT_EQ_INT(1, p.advance(s, 300));
}

TEST(a_start_frame_outside_the_range_is_pulled_in)
{
    Sprite s = frames(4);
    Playback p;
    p.start(0, 2, 3, PlayMode::Forward);
    ASSERT_EQ_INT(2, p.frame());
    p.start(3, 0, 1, PlayMode::Forward);
    ASSERT_EQ_INT(1, p.frame());
}

TEST(a_range_outside_the_sprite_is_clamped_when_advancing)
{
    Sprite s = frames(2);
    Playback p;
    p.start(0, 0, 9, PlayMode::Forward);
    int f = p.advance(s, 100);
    ASSERT_EQ_INT(1, f);
    f = p.advance(s, 200);
    ASSERT_EQ_INT(0, f);
}

TEST(one_frame_stays_put)
{
    Sprite s = frames(1);
    Playback p;
    p.start(0, 0, 0, PlayMode::PingPong);
    ASSERT_EQ_INT(0, p.advance(s, 5000));
    ASSERT_TRUE(p.playing());
}

TEST(stop_ends_playback)
{
    Sprite s = frames(2);
    Playback p;
    p.start(0, 0, 1, PlayMode::Forward);
    p.stop();
    ASSERT_TRUE(!p.playing());
    ASSERT_EQ_INT(-1, p.advance(s, 1000));
}

int main(void)
{
    RUN(idle_playback_does_not_move);
    RUN(forward_waits_for_each_duration);
    RUN(forward_wraps_to_the_first_frame);
    RUN(a_long_step_skips_several_frames_and_keeps_the_remainder);
    RUN(a_huge_step_still_lands_on_a_frame);
    RUN(reverse_runs_backwards_and_wraps);
    RUN(ping_pong_turns_at_the_ends_without_repeating_them);
    RUN(ping_pong_after_a_long_pause_keeps_its_phase);
    RUN(the_range_limits_playback);
    RUN(a_start_frame_outside_the_range_is_pulled_in);
    RUN(a_range_outside_the_sprite_is_clamped_when_advancing);
    RUN(one_frame_stays_put);
    RUN(stop_ends_playback);
    return TEST_SUMMARY();
}
