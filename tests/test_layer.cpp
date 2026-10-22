#include <stdexcept>
#include <string>

#include "minitest.hpp"
#include "tinysprite/layer.hpp"

#include <cstring>

TEST(create_defaults)
{
    auto l = std::make_unique<Layer>("Body", 8, 6, 2);
    ASSERT_TRUE(strcmp(l->name().c_str(), "Body") == 0);
    ASSERT_TRUE(l->visible());
    ASSERT_EQ_INT(255, l->opacity());
    ASSERT_EQ_INT(2, l->frameCount());
}

TEST(each_frame_has_a_blank_cel_of_layer_size)
{
    auto l = std::make_unique<Layer>("L", 8, 6, 3);
    for (int f = 0; f < 3; f++) {
        Cel *c = l->cel(f);
        ASSERT_EQ_INT(8, c->width());
        ASSERT_EQ_INT(6, c->height());
    }
    ASSERT_TRUE(l->cel(0) != l->cel(1));
}

TEST(cel_out_of_range_is_null)
{
    auto l = std::make_unique<Layer>("L", 4, 4, 2);
    ASSERT_TRUE(l->cel(-1) == nullptr);
    ASSERT_TRUE(l->cel(2) == nullptr);
}

TEST(empty_name_is_kept)
{
    auto l = std::make_unique<Layer>("", 4, 4, 1);
    ASSERT_TRUE(strcmp(l->name().c_str(), "") == 0);
}

TEST(truncation_never_splits_a_utf8_character)
{
    std::string name = "x";
    for (int i = 0; i < 30; i++) {
        name += "\xEA\xB0\x80"; /* a 3-byte character */
    }
    Layer l(name, 2, 2, 1);
    ASSERT_TRUE(l.name().size() <= LAYER_NAME_MAX);
    ASSERT_EQ_INT(1, l.name().size() % 3);
}

TEST(long_name_is_truncated)
{
    char long_name[200];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    auto l = std::make_unique<Layer>(long_name, 4, 4, 1);
    ASSERT_EQ_INT(LAYER_NAME_MAX, strlen(l->name().c_str()));
}

TEST(setters_update_state)
{
    auto l = std::make_unique<Layer>("L", 4, 4, 1);
    l->setVisible(false);
    l->setOpacity(128);
    ASSERT_TRUE(!l->visible());
    ASSERT_EQ_INT(128, l->opacity());
}

TEST(set_name_replaces_and_truncates)
{
    auto l = std::make_unique<Layer>("Old", 4, 4, 1);
    l->setName("New");
    ASSERT_TRUE(strcmp(l->name().c_str(), "New") == 0);
    char long_name[LAYER_NAME_MAX + 20];
    memset(long_name, 'y', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = '\0';
    l->setName(long_name);
    ASSERT_EQ_INT(LAYER_NAME_MAX, strlen(l->name().c_str()));
    l->setName("");
    ASSERT_EQ_INT(0, strlen(l->name().c_str()));
}

TEST(invalid_arguments_throw)
{
    ASSERT_THROWS(Layer("L", 0, 4, 1), std::invalid_argument);
    ASSERT_THROWS(Layer("L", 4, 4, 0), std::invalid_argument);
}

TEST(layer_starts_unlocked_and_can_be_locked)
{
    Layer l("a", 4, 4, 1);
    ASSERT_TRUE(!l.locked());
    l.setLocked(true);
    ASSERT_TRUE(l.locked());
}

int main()
{
    RUN(create_defaults);
    RUN(each_frame_has_a_blank_cel_of_layer_size);
    RUN(cel_out_of_range_is_null);
    RUN(empty_name_is_kept);
    RUN(long_name_is_truncated);
    RUN(setters_update_state);
    RUN(set_name_replaces_and_truncates);
    RUN(invalid_arguments_throw);
    RUN(layer_starts_unlocked_and_can_be_locked);
    RUN(truncation_never_splits_a_utf8_character);
    return TEST_SUMMARY();
}
