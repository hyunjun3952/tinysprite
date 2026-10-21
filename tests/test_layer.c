#include "minitest.h"
#include "tinysprite/layer.h"

#include <string.h>

TEST(create_defaults)
{
    Layer *l = layer_create("Body", 8, 6, 2);
    ASSERT_TRUE(l != NULL);
    ASSERT_TRUE(strcmp(layer_name(l), "Body") == 0);
    ASSERT_TRUE(layer_visible(l));
    ASSERT_EQ_INT(255, layer_opacity(l));
    ASSERT_EQ_INT(2, layer_frame_count(l));
    layer_destroy(l);
}

TEST(each_frame_has_a_blank_cel_of_layer_size)
{
    Layer *l = layer_create("L", 8, 6, 3);
    for (int f = 0; f < 3; f++) {
        Cel *c = layer_cel(l, f);
        ASSERT_TRUE(c != NULL);
        ASSERT_EQ_INT(8, cel_width(c));
        ASSERT_EQ_INT(6, cel_height(c));
    }
    ASSERT_TRUE(layer_cel(l, 0) != layer_cel(l, 1));
    layer_destroy(l);
}

TEST(cel_out_of_range_is_null)
{
    Layer *l = layer_create("L", 4, 4, 2);
    ASSERT_TRUE(layer_cel(l, -1) == NULL);
    ASSERT_TRUE(layer_cel(l, 2) == NULL);
    layer_destroy(l);
}

TEST(null_name_becomes_empty)
{
    Layer *l = layer_create(NULL, 4, 4, 1);
    ASSERT_TRUE(strcmp(layer_name(l), "") == 0);
    layer_destroy(l);
}

TEST(long_name_is_truncated)
{
    char long_name[200];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    Layer *l = layer_create(long_name, 4, 4, 1);
    ASSERT_EQ_INT(LAYER_NAME_MAX, strlen(layer_name(l)));
    layer_destroy(l);
}

TEST(setters_update_state)
{
    Layer *l = layer_create("L", 4, 4, 1);
    layer_set_visible(l, 0);
    layer_set_opacity(l, 128);
    ASSERT_TRUE(!layer_visible(l));
    ASSERT_EQ_INT(128, layer_opacity(l));
    layer_destroy(l);
}

TEST(invalid_arguments_return_null)
{
    ASSERT_TRUE(layer_create("L", 0, 4, 1) == NULL);
    ASSERT_TRUE(layer_create("L", 4, 4, 0) == NULL);
}

TEST(destroy_null_is_safe)
{
    layer_destroy(NULL);
}

int main(void)
{
    RUN(create_defaults);
    RUN(each_frame_has_a_blank_cel_of_layer_size);
    RUN(cel_out_of_range_is_null);
    RUN(null_name_becomes_empty);
    RUN(long_name_is_truncated);
    RUN(setters_update_state);
    RUN(invalid_arguments_return_null);
    RUN(destroy_null_is_safe);
    return TEST_SUMMARY();
}
