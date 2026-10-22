#include <string>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

static bool has(const std::string &text, const char *part)
{
    return text.find(part) != std::string::npos;
}

/* A 4x2 sprite with 3 frames of 100, 200 and 100 ms and a "walk" tag on frames 1..2. */
static Sprite walker()
{
    Sprite s(4, 2);
    s.insertFrame(1, s.makeFrame());
    s.insertFrame(2, s.makeFrame());
    s.setFrameDuration(1, 200);
    s.addTag(Sprite::Tag{"walk", 1, 2, PlayMode::PingPong});
    return s;
}

static std::string json_of(const Sprite &sprite, const SheetOptions &o, const char *image)
{
    std::optional<SpriteSheet> sheet = spritesheet_render(sprite, o);
    return sheet ? spritesheet_metadata_json(sprite, o, *sheet, image) : std::string();
}

TEST(lists_the_frames_with_rects_and_durations)
{
    std::string j = json_of(walker(), SheetOptions{}, "walk.png");
    ASSERT_TRUE(has(j, "\"image\": \"walk.png\""));
    ASSERT_TRUE(has(j, "\"size\": {\"w\": 12, \"h\": 2}"));
    ASSERT_TRUE(has(j, "\"frame\": {\"x\": 4, \"y\": 0, \"w\": 4, \"h\": 2}, \"duration\": 200"));
    ASSERT_TRUE(has(j, "\"frame\": {\"x\": 8, \"y\": 0, \"w\": 4, \"h\": 2}, \"duration\": 100"));
}

TEST(lists_the_tags_with_their_direction)
{
    std::string j = json_of(walker(), SheetOptions{}, "a.png");
    ASSERT_TRUE(
        has(j, "{\"name\": \"walk\", \"from\": 1, \"to\": 2, \"direction\": \"pingpong\"}"));
}

TEST(a_range_renumbers_the_tags_and_drops_the_ones_outside)
{
    Sprite s = walker();
    s.addTag(Sprite::Tag{"idle", 0, 0, PlayMode::Forward});
    SheetOptions o;
    o.first = 1;
    std::string j = json_of(s, o, "a.png");
    ASSERT_TRUE(has(j, "\"name\": \"walk\", \"from\": 0, \"to\": 1"));
    ASSERT_TRUE(!has(j, "idle"));
}

TEST(a_scale_is_reported_and_the_rects_follow_it)
{
    SheetOptions o;
    o.scale = 2;
    std::string j = json_of(walker(), o, "a.png");
    ASSERT_TRUE(has(j, "\"scale\": 2"));
    ASSERT_TRUE(has(j, "\"frame\": {\"x\": 8, \"y\": 0, \"w\": 8, \"h\": 4}"));
}

TEST(names_are_escaped)
{
    Sprite s(2, 2);
    s.addTag(Sprite::Tag{"a\"b\\c\n", 0, 0, PlayMode::Reverse});
    std::string j = json_of(s, SheetOptions{}, "x\"y.png");
    ASSERT_TRUE(has(j, "\"name\": \"a\\\"b\\\\c\\n\""));
    ASSERT_TRUE(has(j, "\"image\": \"x\\\"y.png\""));
    ASSERT_TRUE(has(j, "\"direction\": \"reverse\""));
}

TEST(no_tags_gives_an_empty_list)
{
    std::string j = json_of(Sprite(2, 2), SheetOptions{}, "a.png");
    ASSERT_TRUE(has(j, "\"tags\": []"));
}

<<<<<<< HEAD
TEST(lists_the_slices_with_bounds_and_an_optional_pivot)
{
    Sprite s = walker();
    Sprite::Slice hit;
    hit.name = "hitbox";
    hit.rect = Rect{1, 0, 2, 2};
    hit.hasPivot = true;
    hit.pivotX = 1;
    hit.pivotY = 1;
    Sprite::Slice head;
    head.name = "head";
    head.rect = Rect{0, 0, 1, 1};
    s.addSlice(hit);
    s.addSlice(head);
    std::string j = json_of(s, SheetOptions{}, "a.png");
    ASSERT_TRUE(has(j,
                    "{\"name\": \"hitbox\", \"bounds\": {\"x\": 1, \"y\": 0, \"w\": 2, \"h\": 2}, "
                    "\"pivot\": {\"x\": 1, \"y\": 1}}"));
    ASSERT_TRUE(
        has(j, "{\"name\": \"head\", \"bounds\": {\"x\": 0, \"y\": 0, \"w\": 1, \"h\": 1}}"));
}

TEST(a_sprite_without_slices_has_an_empty_list)
{
    ASSERT_TRUE(has(json_of(walker(), SheetOptions{}, "a.png"), "\"slices\": []"));
}

TEST(slices_follow_the_export_scale)
{
    Sprite s = walker();
    Sprite::Slice hit;
    hit.name = "hitbox";
    hit.rect = Rect{1, 0, 2, 2};
    hit.hasPivot = true;
    hit.pivotX = 1;
    hit.pivotY = 0;
    s.addSlice(hit);
    SheetOptions o;
    o.scale = 3;
    std::string j = json_of(s, o, "a.png");
    ASSERT_TRUE(has(j, "\"bounds\": {\"x\": 3, \"y\": 0, \"w\": 6, \"h\": 6}"));
    ASSERT_TRUE(has(j, "\"pivot\": {\"x\": 3, \"y\": 0}"));
=======
TEST(a_packed_sheet_lists_the_trim_of_every_frame)
{
    Sprite s(8, 8);
    s.insertFrame(1, s.makeFrame());
    s.layer(0)->cel(0)->set(2, 3, Rgba{255, 0, 0, 255});
    s.layer(0)->cel(1)->set(5, 6, Rgba{0, 255, 0, 255});
    SheetOptions o;
    o.layout = SheetLayout::Packed;
    std::string j = json_of(s, o, "a.png");
    ASSERT_TRUE(has(j, "\"trimmed\": true"));
    ASSERT_TRUE(has(j, "\"spriteSourceSize\": {\"x\": 2, \"y\": 3, \"w\": 1, \"h\": 1}"));
    ASSERT_TRUE(has(j, "\"spriteSourceSize\": {\"x\": 5, \"y\": 6, \"w\": 1, \"h\": 1}"));
    ASSERT_TRUE(has(j, "\"sourceSize\": {\"w\": 8, \"h\": 8}"));
    ASSERT_TRUE(has(j, "\"duration\": 100"));
}

TEST(the_other_layouts_do_not_mention_a_trim)
{
    std::string j = json_of(walker(), SheetOptions{}, "a.png");
    ASSERT_TRUE(!has(j, "trimmed"));
    ASSERT_TRUE(!has(j, "spriteSourceSize"));
>>>>>>> 9d11f7b (feat(io): packed sprite sheets with trimmed frames (milestone 21))
}

int main()
{
<<<<<<< HEAD
    RUN(lists_the_slices_with_bounds_and_an_optional_pivot);
    RUN(a_sprite_without_slices_has_an_empty_list);
    RUN(slices_follow_the_export_scale);
=======
    RUN(a_packed_sheet_lists_the_trim_of_every_frame);
    RUN(the_other_layouts_do_not_mention_a_trim);
>>>>>>> 9d11f7b (feat(io): packed sprite sheets with trimmed frames (milestone 21))
    RUN(lists_the_frames_with_rects_and_durations);
    RUN(lists_the_tags_with_their_direction);
    RUN(a_range_renumbers_the_tags_and_drops_the_ones_outside);
    RUN(a_scale_is_reported_and_the_rects_follow_it);
    RUN(names_are_escaped);
    RUN(no_tags_gives_an_empty_list);
    return TEST_SUMMARY();
}
