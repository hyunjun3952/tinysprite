#include "minitest.h"
#include "tinysprite/sprite.h"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

TEST(new_sprite_has_one_frame_with_default_duration)
{
    Sprite s(4, 4);
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, s.frameDuration(0));
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, s.frameDuration(5)); /* out of range: the default */
}

TEST(duration_is_limited)
{
    Sprite s(4, 4);
    ASSERT_TRUE(s.setFrameDuration(0, 250));
    ASSERT_EQ_INT(250, s.frameDuration(0));
    ASSERT_TRUE(!s.setFrameDuration(0, 0));
    ASSERT_TRUE(!s.setFrameDuration(0, FRAME_DURATION_MAX + 1));
    ASSERT_TRUE(!s.setFrameDuration(1, 100));
    ASSERT_EQ_INT(250, s.frameDuration(0));
}

TEST(insert_frame_adds_a_cel_to_every_layer)
{
    Sprite s(4, 3);
    s.addLayer("Top");
    Sprite::Frame f = s.makeFrame();
    ASSERT_EQ_INT(2, static_cast<int>(f.cels.size()));
    ASSERT_TRUE(s.insertFrame(1, std::move(f)));
    ASSERT_EQ_INT(2, s.frameCount());
    for (int l = 0; l < 2; l++) {
        Cel *cel = s.layer(l)->cel(1);
        ASSERT_TRUE(cel != nullptr);
        ASSERT_EQ_INT(4, cel->width());
        ASSERT_EQ_INT(0, cel->get(0, 0).a);
    }
    ASSERT_EQ_INT(2, s.layer(0)->frameCount());
}

TEST(insert_frame_in_the_middle_keeps_order)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.insertFrame(1, s.makeFrame());
    s.layer(0)->cel(1)->set(0, 0, BLUE);
    Sprite::Frame mid = s.makeFrame();
    mid.duration = 40;
    ASSERT_TRUE(s.insertFrame(1, std::move(mid)));
    ASSERT_EQ_INT(3, s.frameCount());
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(0, 0).r);
    ASSERT_EQ_INT(0, s.layer(0)->cel(1)->get(0, 0).a);
    ASSERT_EQ_INT(255, s.layer(0)->cel(2)->get(0, 0).b);
    ASSERT_EQ_INT(40, s.frameDuration(1));
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, s.frameDuration(2));
}

TEST(insert_frame_rejects_bad_input)
{
    Sprite s(2, 2);
    ASSERT_TRUE(!s.insertFrame(-1, s.makeFrame()));
    ASSERT_TRUE(!s.insertFrame(2, s.makeFrame()));
    Sprite::Frame wrong = s.makeFrame();
    wrong.cels.clear();
    ASSERT_TRUE(!s.insertFrame(0, std::move(wrong)));
    ASSERT_EQ_INT(1, s.frameCount());
}

TEST(copy_frame_is_a_deep_copy_with_the_duration)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(1, 1, RED);
    s.setFrameDuration(0, 70);
    Sprite::Frame copy = s.copyFrame(0);
    ASSERT_EQ_INT(70, copy.duration);
    ASSERT_EQ_INT(255, copy.cels[0]->get(1, 1).r);
    copy.cels[0]->set(1, 1, BLUE);
    ASSERT_EQ_INT(255, s.layer(0)->cel(0)->get(1, 1).r);
}

TEST(detach_frame_hands_the_cels_over)
{
    Sprite s(2, 2);
    s.insertFrame(1, s.makeFrame());
    Cel *second = s.layer(0)->cel(1);
    second->set(0, 0, RED);
    std::optional<Sprite::Frame> f = s.detachFrame(1);
    ASSERT_TRUE(f.has_value());
    if (!f) {
        return;
    }
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_EQ_INT(1, s.layer(0)->frameCount());
    ASSERT_TRUE(f->cels[0].get() == second);
    ASSERT_TRUE(s.insertFrame(1, std::move(*f)));
    ASSERT_TRUE(s.layer(0)->cel(1) == second);
}

TEST(the_last_frame_cannot_be_detached)
{
    Sprite s(2, 2);
    ASSERT_TRUE(!s.detachFrame(0).has_value());
    s.insertFrame(1, s.makeFrame());
    ASSERT_TRUE(!s.detachFrame(2).has_value());
    ASSERT_TRUE(s.detachFrame(0).has_value());
    ASSERT_TRUE(!s.detachFrame(0).has_value());
}

TEST(move_frame_shifts_the_ones_between)
{
    Sprite s(2, 2);
    s.insertFrame(1, s.makeFrame());
    s.insertFrame(2, s.makeFrame());
    for (int f = 0; f < 3; f++) {
        s.layer(0)->cel(f)->set(0, 0, Rgba{static_cast<uint8_t>(f + 1), 0, 0, 255});
        s.setFrameDuration(f, 10 * (f + 1));
    }
    ASSERT_TRUE(s.moveFrame(0, 2));
    ASSERT_EQ_INT(2, s.layer(0)->cel(0)->get(0, 0).r);
    ASSERT_EQ_INT(3, s.layer(0)->cel(1)->get(0, 0).r);
    ASSERT_EQ_INT(1, s.layer(0)->cel(2)->get(0, 0).r);
    ASSERT_EQ_INT(20, s.frameDuration(0));
    ASSERT_EQ_INT(10, s.frameDuration(2));
    ASSERT_TRUE(s.moveFrame(2, 0));
    ASSERT_EQ_INT(1, s.layer(0)->cel(0)->get(0, 0).r);
    ASSERT_TRUE(!s.moveFrame(0, 3));
    ASSERT_TRUE(!s.moveFrame(-1, 0));
}

TEST(layers_added_later_get_a_cel_per_frame)
{
    Sprite s(2, 2);
    s.insertFrame(1, s.makeFrame());
    Layer &top = s.addLayer("Top");
    ASSERT_EQ_INT(2, top.frameCount());
}

int main()
{
    RUN(new_sprite_has_one_frame_with_default_duration);
    RUN(duration_is_limited);
    RUN(insert_frame_adds_a_cel_to_every_layer);
    RUN(insert_frame_in_the_middle_keeps_order);
    RUN(insert_frame_rejects_bad_input);
    RUN(copy_frame_is_a_deep_copy_with_the_duration);
    RUN(detach_frame_hands_the_cels_over);
    RUN(the_last_frame_cannot_be_detached);
    RUN(move_frame_shifts_the_ones_between);
    RUN(layers_added_later_get_a_cel_per_frame);
    return TEST_SUMMARY();
}
