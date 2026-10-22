#include "minitest.hpp"
#include "tinysprite/sprite.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

TEST(create_has_one_frame_and_one_layer)
{
    Sprite s(16, 8);
    ASSERT_EQ_INT(16, s.width());
    ASSERT_EQ_INT(8, s.height());
    ASSERT_EQ_INT(1, s.frameCount());
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_TRUE(strcmp(s.layer(0)->name().c_str(), "Layer 1") == 0);
    ASSERT_EQ_INT(0, s.palette().count());
}

TEST(create_rejects_invalid_size)
{
    ASSERT_THROWS(Sprite(0, 8), std::invalid_argument);
    ASSERT_THROWS(Sprite(8, -1), std::invalid_argument);
    ASSERT_THROWS(Sprite(CEL_MAX_DIMENSION + 1, 8), std::invalid_argument);
}

TEST(first_layer_cel_matches_sprite_size)
{
    Sprite s(16, 8);
    Cel *c = s.layer(0)->cel(0);
    ASSERT_EQ_INT(16, c->width());
    ASSERT_EQ_INT(8, c->height());
}

TEST(add_layer_appends_on_top)
{
    Sprite s(4, 4);
    Layer *top = &s.addLayer("Top");
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(s.layer(1) == top);
    ASSERT_TRUE(strcmp(top->name().c_str(), "Top") == 0);
    ASSERT_EQ_INT(4, top->cel(0)->width());
}

TEST(add_many_layers_keeps_pointers_valid)
{
    Sprite s(4, 4);
    Layer *first = s.layer(0);
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(&s.addLayer("x") != nullptr);
    }
    ASSERT_EQ_INT(101, s.layerCount());
    ASSERT_TRUE(s.layer(0) == first);
}

TEST(layer_out_of_range_is_null)
{
    Sprite s(4, 4);
    ASSERT_TRUE(s.layer(-1) == nullptr);
    ASSERT_TRUE(s.layer(1) == nullptr);
}

TEST(remove_layer_shifts_upper_layers_down)
{
    Sprite s(4, 4);
    s.addLayer("B");
    s.addLayer("C");
    ASSERT_TRUE(s.removeLayer(1));
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(strcmp(s.layer(0)->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(strcmp(s.layer(1)->name().c_str(), "C") == 0);
    ASSERT_TRUE(s.layer(2) == nullptr);
}

TEST(remove_layer_keeps_at_least_one_layer)
{
    Sprite s(4, 4);
    ASSERT_TRUE(!s.removeLayer(0));
    ASSERT_EQ_INT(1, s.layerCount());
    s.addLayer("B");
    ASSERT_TRUE(!s.removeLayer(2));
    ASSERT_TRUE(!s.removeLayer(-1));
    ASSERT_EQ_INT(2, s.layerCount());
}

TEST(move_layer_up_and_down)
{
    Sprite s(4, 4);
    s.addLayer("B");
    s.addLayer("C");
    ASSERT_TRUE(s.moveLayer(0, 2)); /* Layer 1 to the top */
    ASSERT_TRUE(strcmp(s.layer(0)->name().c_str(), "B") == 0);
    ASSERT_TRUE(strcmp(s.layer(1)->name().c_str(), "C") == 0);
    ASSERT_TRUE(strcmp(s.layer(2)->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(s.moveLayer(2, 1));
    ASSERT_TRUE(strcmp(s.layer(1)->name().c_str(), "Layer 1") == 0);
    ASSERT_TRUE(strcmp(s.layer(2)->name().c_str(), "C") == 0);
}

TEST(move_layer_same_index_is_noop_and_bad_index_fails)
{
    Sprite s(4, 4);
    s.addLayer("B");
    ASSERT_TRUE(s.moveLayer(1, 1));
    ASSERT_TRUE(strcmp(s.layer(1)->name().c_str(), "B") == 0);
    ASSERT_TRUE(!s.moveLayer(0, 2));
    ASSERT_TRUE(!s.moveLayer(-1, 0));
}

TEST(detach_and_insert_round_trip)
{
    Sprite s(4, 4);
    Layer *a = s.layer(0);
    Layer *b = &s.addLayer("B");
    Layer *c = &s.addLayer("C");
    std::unique_ptr<Layer> detached = s.detachLayer(1);
    ASSERT_TRUE(detached.get() == b);
    ASSERT_EQ_INT(2, s.layerCount());
    ASSERT_TRUE(s.layer(0) == a && s.layer(1) == c);
    ASSERT_TRUE(b->cel(0)->width() == 4); /* still alive */
    ASSERT_TRUE(s.insertLayer(1, std::move(detached)));
    ASSERT_TRUE(s.layer(1) == b && s.layer(2) == c);
}

TEST(detach_and_insert_reject_bad_arguments)
{
    Sprite s(4, 4);
    ASSERT_TRUE(s.detachLayer(0) == nullptr); /* last layer */
    s.addLayer("B");
    ASSERT_TRUE(s.detachLayer(5) == nullptr);
    ASSERT_TRUE(s.detachLayer(-1) == nullptr);
    auto spare = std::make_unique<Layer>("X", 4, 4, 1);
    Layer *raw = spare.get();
    ASSERT_TRUE(!s.insertLayer(3, std::move(spare)));
    ASSERT_TRUE(spare != nullptr); /* a rejected layer stays with the caller */
    ASSERT_TRUE(!s.insertLayer(-1, std::move(spare)));
    ASSERT_TRUE(!s.insertLayer(0, nullptr));
    ASSERT_TRUE(s.insertLayer(2, std::move(spare))); /* append */
    ASSERT_TRUE(s.layer(2) == raw);
}

int main()
{
    RUN(create_has_one_frame_and_one_layer);
    RUN(create_rejects_invalid_size);
    RUN(first_layer_cel_matches_sprite_size);
    RUN(add_layer_appends_on_top);
    RUN(add_many_layers_keeps_pointers_valid);
    RUN(layer_out_of_range_is_null);
    RUN(remove_layer_shifts_upper_layers_down);
    RUN(remove_layer_keeps_at_least_one_layer);
    RUN(move_layer_up_and_down);
    RUN(move_layer_same_index_is_noop_and_bad_index_fails);
    RUN(detach_and_insert_round_trip);
    RUN(detach_and_insert_reject_bad_arguments);
    return TEST_SUMMARY();
}
