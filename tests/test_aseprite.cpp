#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

/* Builds the bytes of an Aseprite file by hand: the header, frames and chunks of the format, with
 * pixel data either raw or in a stored (uncompressed) zlib stream. */
struct Bytes {
    std::vector<uint8_t> v;

    Bytes &u8(unsigned value)
    {
        v.push_back(static_cast<uint8_t>(value));
        return *this;
    }
    Bytes &u16(unsigned value)
    {
        return u8(value & 0xFF).u8(value >> 8);
    }
    Bytes &u32(unsigned value)
    {
        return u16(value & 0xFFFF).u16(value >> 16);
    }
    Bytes &s16(int value)
    {
        return u16(static_cast<unsigned>(value) & 0xFFFF);
    }
    Bytes &s32(int value)
    {
        return u32(static_cast<unsigned>(value));
    }
    Bytes &zeros(size_t count)
    {
        v.insert(v.end(), count, 0);
        return *this;
    }
    Bytes &str(const std::string &text)
    {
        u16(static_cast<unsigned>(text.size()));
        v.insert(v.end(), text.begin(), text.end());
        return *this;
    }
    Bytes &add(const Bytes &other)
    {
        v.insert(v.end(), other.v.begin(), other.v.end());
        return *this;
    }
};

static Bytes chunk(unsigned type, const Bytes &payload)
{
    Bytes c;
    c.u32(static_cast<unsigned>(payload.v.size()) + 6).u16(type).add(payload);
    return c;
}

static Bytes frame(unsigned duration, const std::vector<Bytes> &chunks)
{
    Bytes body;
    for (const Bytes &c : chunks) {
        body.add(c);
    }
    Bytes f;
    f.u32(static_cast<unsigned>(body.v.size()) + 16)
        .u16(0xF1FA)
        .u16(static_cast<unsigned>(chunks.size()));
    f.u16(duration).zeros(2).u32(0).add(body);
    return f;
}

static Bytes file(int width, int height, int depth, const std::vector<Bytes> &frames,
                  unsigned transparentIndex = 0)
{
    Bytes body;
    for (const Bytes &f : frames) {
        body.add(f);
    }
    Bytes h;
    h.u32(static_cast<unsigned>(body.v.size()) + 128)
        .u16(0xA5E0)
        .u16(static_cast<unsigned>(frames.size()));
    h.u16(static_cast<unsigned>(width))
        .u16(static_cast<unsigned>(height))
        .u16(static_cast<unsigned>(depth));
    h.u32(1).u16(100).u32(0).u32(0).u8(transparentIndex).zeros(3).u16(0).u8(1).u8(1);
    h.zeros(128 - h.v.size());
    return h.add(body);
}

static Bytes layer(unsigned flags, unsigned type, unsigned level, unsigned blend, unsigned opacity,
                   const std::string &name)
{
    Bytes p;
    p.u16(flags).u16(type).u16(level).u16(0).u16(0).u16(blend).u8(opacity).zeros(3).str(name);
    if (type == 2) {
        p.u32(0);
    }
    return chunk(0x2004, p);
}

static Bytes cel_header(unsigned layerIndex, int x, int y, unsigned opacity, unsigned type)
{
    Bytes p;
    p.u16(layerIndex).s16(x).s16(y).u8(opacity).u16(type).s16(0).zeros(5);
    return p;
}

static Bytes cel_raw(unsigned layerIndex, int x, int y, unsigned opacity, int w, int h,
                     const std::vector<uint8_t> &pixels)
{
    Bytes p = cel_header(layerIndex, x, y, opacity, 0);
    p.u16(static_cast<unsigned>(w)).u16(static_cast<unsigned>(h));
    p.v.insert(p.v.end(), pixels.begin(), pixels.end());
    return chunk(0x2005, p);
}

static Bytes stored_zlib(const std::vector<uint8_t> &data)
{
    Bytes z;
    z.u8(0x78).u8(0x01).u8(0x01).u16(static_cast<unsigned>(data.size()));
    z.u16(~static_cast<unsigned>(data.size()) & 0xFFFF);
    z.v.insert(z.v.end(), data.begin(), data.end());
    return z.u32(0);
}

static Bytes cel_compressed(unsigned layerIndex, int x, int y, unsigned opacity, int w, int h,
                            const std::vector<uint8_t> &pixels)
{
    Bytes p = cel_header(layerIndex, x, y, opacity, 2);
    p.u16(static_cast<unsigned>(w)).u16(static_cast<unsigned>(h)).add(stored_zlib(pixels));
    return chunk(0x2005, p);
}

static Bytes cel_linked(unsigned layerIndex, unsigned toFrame)
{
    Bytes p = cel_header(layerIndex, 0, 0, 255, 1);
    p.u16(toFrame);
    return chunk(0x2005, p);
}

static std::vector<uint8_t> px(std::initializer_list<unsigned> values)
{
    std::vector<uint8_t> out;
    for (unsigned v : values) {
        out.push_back(static_cast<uint8_t>(v));
    }
    return out;
}

static const unsigned VISIBLE = 1;
static const unsigned EDITABLE = 2;
static const unsigned BACKGROUND = 8;

static Rgba at(const Sprite &s, int layerIndex, int frameIndex, int x, int y)
{
    return s.layer(layerIndex)->cel(frameIndex)->get(x, y);
}

TEST(a_plain_rgba_file_becomes_a_sprite_with_frames_and_layers)
{
    Bytes bytes = file(3, 2, 32,
                       {frame(50, {layer(VISIBLE | EDITABLE, 0, 0, 0, 255, "Body"),
                                   cel_raw(0, 0, 0, 255, 3, 2,
                                           px({255, 0, 0, 255, 0, 255, 0, 255, 0, 0,  255, 255,
                                               1,   2, 3, 4,   5, 6,   7, 8,   9, 10, 11,  12}))}),
                        frame(200, {cel_raw(0, 0, 0, 255, 1, 1, px({9, 8, 7, 255}))})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(3, s.width());
    ASSERT_EQ_INT(2, s.height());
    ASSERT_EQ_INT(2, s.frameCount());
    ASSERT_EQ_INT(50, s.frameDuration(0));
    ASSERT_EQ_INT(200, s.frameDuration(1));
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_TRUE(s.layer(0)->name() == "Body");
    ASSERT_TRUE(at(s, 0, 0, 0, 0) == Rgba({255, 0, 0, 255}));
    ASSERT_TRUE(at(s, 0, 0, 2, 0) == Rgba({0, 0, 255, 255}));
    ASSERT_TRUE(at(s, 0, 0, 2, 1) == Rgba({9, 10, 11, 12}));
    ASSERT_TRUE(at(s, 0, 1, 0, 0) == Rgba({9, 8, 7, 255}));
    ASSERT_TRUE(at(s, 0, 1, 1, 0) == Rgba{});
}

TEST(a_compressed_cel_reads_like_a_raw_one)
{
    Bytes bytes =
        file(2, 1, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                          cel_compressed(0, 0, 0, 255, 2, 1, px({1, 2, 3, 4, 5, 6, 7, 8}))})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_TRUE(at(s, 0, 0, 1, 0) == Rgba({5, 6, 7, 8}));
}

TEST(a_cel_is_placed_at_its_offset_and_clipped_to_the_sprite)
{
    std::vector<uint8_t> red = px({255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255});
    Bytes bytes =
        file(3, 3, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "A"), layer(VISIBLE, 0, 0, 0, 255, "B"),
                          cel_raw(0, 2, 1, 255, 2, 2, red), cel_raw(1, -1, -1, 255, 2, 2, red)})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_TRUE(at(s, 0, 0, 2, 1) == Rgba({255, 0, 0, 255}));
    ASSERT_TRUE(at(s, 0, 0, 2, 2) == Rgba({255, 0, 0, 255}));
    ASSERT_TRUE(at(s, 0, 0, 1, 1) == Rgba{});
    ASSERT_TRUE(at(s, 1, 0, 0, 0) == Rgba({255, 0, 0, 255}));
    ASSERT_TRUE(at(s, 1, 0, 1, 0) == Rgba{});
}

TEST(layer_flags_opacity_and_blend_mode_are_kept)
{
    Bytes bytes = file(1, 1, 32,
                       {frame(100, {layer(0, 0, 0, 0, 255, "hidden"),
                                    layer(VISIBLE | EDITABLE, 0, 0, 1, 128, "multiply"),
                                    layer(VISIBLE, 0, 0, 16, 255, "add, locked"),
                                    layer(VISIBLE | EDITABLE, 0, 0, 12, 255, "hue")})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(4, s.layerCount());
    ASSERT_TRUE(!s.layer(0)->visible());
    ASSERT_EQ_INT(128, s.layer(1)->opacity());
    ASSERT_TRUE(s.layer(1)->blendMode() == BlendMode::Multiply);
    ASSERT_TRUE(s.layer(1)->visible());
    ASSERT_TRUE(!s.layer(1)->locked());
    ASSERT_TRUE(s.layer(2)->blendMode() == BlendMode::Add);
    ASSERT_TRUE(s.layer(2)->locked());
    ASSERT_TRUE(s.layer(3)->blendMode() == BlendMode::Normal); /* hue has no counterpart */
}

TEST(groups_are_flattened_and_a_hidden_group_hides_its_layers)
{
    std::vector<uint8_t> dot = px({10, 20, 30, 255});
    Bytes bytes =
        file(1, 1, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "below"),         /* 0 */
                          layer(EDITABLE, 1, 0, 0, 255, "hidden group"), /* 1: not visible */
                          layer(VISIBLE, 0, 1, 0, 255, "inside"),        /* 2 */
                          layer(VISIBLE, 2, 0, 0, 255, "tiles"),         /* 3: a tilemap */
                          layer(VISIBLE, 0, 0, 0, 255, "above"),         /* 4 */
                          cel_raw(2, 0, 0, 255, 1, 1, dot), cel_raw(4, 0, 0, 255, 1, 1, dot)})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(3, s.layerCount()); /* below, inside, above */
    ASSERT_TRUE(s.layer(0)->name() == "below");
    ASSERT_TRUE(s.layer(1)->name() == "inside");
    ASSERT_TRUE(!s.layer(1)->visible());
    ASSERT_TRUE(s.layer(2)->name() == "above");
    ASSERT_TRUE(s.layer(2)->visible());
    ASSERT_TRUE(at(s, 1, 0, 0, 0) == Rgba({10, 20, 30, 255}));
    ASSERT_TRUE(at(s, 2, 0, 0, 0) == Rgba({10, 20, 30, 255}));
    ASSERT_TRUE(at(s, 0, 0, 0, 0) == Rgba{});
}

TEST(the_opacity_of_a_cel_goes_into_its_pixels)
{
    Bytes bytes =
        file(2, 1, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                          cel_raw(0, 0, 0, 128, 2, 1, px({1, 2, 3, 255, 4, 5, 6, 100}))})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(128, at(s, 0, 0, 0, 0).a);
    ASSERT_EQ_INT(50, at(s, 0, 0, 1, 0).a);
}

TEST(a_linked_cel_shares_the_cel_of_the_earlier_frame)
{
    Bytes bytes = file(1, 1, 32,
                       {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                    cel_raw(0, 0, 0, 255, 1, 1, px({1, 1, 1, 255}))}),
                        frame(100, {cel_linked(0, 0)})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_TRUE(s.layer(0)->celLinked(1));
    ASSERT_TRUE(at(s, 0, 1, 0, 0) == Rgba({1, 1, 1, 255}));
}

static Bytes palette_chunk(const std::vector<Rgba> &colors)
{
    Bytes p;
    p.u32(static_cast<unsigned>(colors.size()))
        .u32(0)
        .u32(static_cast<unsigned>(colors.size()) - 1)
        .zeros(8);
    for (const Rgba &c : colors) {
        p.u16(0).u8(c.r).u8(c.g).u8(c.b).u8(c.a);
    }
    return chunk(0x2019, p);
}

TEST(an_indexed_file_looks_its_colors_up_in_the_palette)
{
    std::vector<Rgba> colors = {{9, 9, 9, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}};
    Bytes bytes = file(3, 1, 8,
                       {frame(100, {palette_chunk(colors), layer(VISIBLE, 0, 0, 0, 255, "fg"),
                                    layer(VISIBLE | BACKGROUND, 0, 0, 0, 255, "bg"),
                                    cel_raw(0, 0, 0, 255, 3, 1, px({0, 1, 2})),
                                    cel_raw(1, 0, 0, 255, 3, 1, px({0, 1, 9}))})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_TRUE(at(s, 0, 0, 0, 0) == Rgba{}); /* the transparent index on an ordinary layer */
    ASSERT_TRUE(at(s, 0, 0, 1, 0) == Rgba({255, 0, 0, 255}));
    ASSERT_TRUE(at(s, 0, 0, 2, 0) == Rgba({0, 255, 0, 255}));
    ASSERT_TRUE(at(s, 1, 0, 0, 0) == Rgba({9, 9, 9, 255})); /* a background layer shows it */
    ASSERT_TRUE(at(s, 1, 0, 2, 0) == Rgba{});               /* an index past the palette */
    ASSERT_EQ_INT(3, s.palette().count());
    ASSERT_TRUE(s.palette().get(1) == Rgba({255, 0, 0, 255}));
}

TEST(a_grayscale_file_becomes_gray_pixels)
{
    Bytes bytes = file(2, 1, 16,
                       {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                    cel_raw(0, 0, 0, 255, 2, 1, px({200, 255, 50, 128}))})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_TRUE(at(s, 0, 0, 0, 0) == Rgba({200, 200, 200, 255}));
    ASSERT_TRUE(at(s, 0, 0, 1, 0) == Rgba({50, 50, 50, 128}));
}

static Bytes tag_chunk(const std::vector<std::vector<unsigned>> &tags,
                       const std::vector<std::string> &names)
{
    Bytes p;
    p.u16(static_cast<unsigned>(tags.size())).zeros(8);
    for (size_t i = 0; i < tags.size(); i++) {
        p.u16(tags[i][0])
            .u16(tags[i][1])
            .u8(tags[i][2])
            .u16(0)
            .zeros(6)
            .zeros(3)
            .u8(0)
            .str(names[i]);
    }
    return chunk(0x2018, p);
}

TEST(tags_keep_their_range_and_direction_and_bad_ones_are_dropped)
{
    Bytes bytes =
        file(1, 1, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                          tag_chunk({{0, 1, 0}, {1, 9, 2}, {5, 6, 0}, {1, 0, 1}, {0, 0, 3}},
                                    {"idle", "walk", "gone", "backwards", "odd"})}),
              frame(100, {})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(3, s.tags().size());
    ASSERT_TRUE(s.tags()[0].name == "idle");
    ASSERT_EQ_INT(1, s.tags()[1].to); /* clamped to the last frame */
    ASSERT_TRUE(s.tags()[1].direction == PlayMode::PingPong);
    ASSERT_TRUE(s.tags()[2].name == "odd");
    ASSERT_TRUE(s.tags()[2].direction == PlayMode::PingPong); /* ping-pong reverse */
}

TEST(slices_keep_their_first_key_and_pivot)
{
    Bytes p;
    p.u32(1).u32(2).u32(0).str("hitbox");
    p.u32(0).s32(1).s32(2).u32(3).u32(2).s32(1).s32(1);
    Bytes bad;
    bad.u32(1).u32(0).u32(0).str("outside");
    bad.u32(0).s32(6).s32(6).u32(3).u32(3);
    Bytes bytes = file(
        8, 8, 32,
        {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"), chunk(0x2022, p), chunk(0x2022, bad)})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(1, s.slices().size());
    const Sprite::Slice &slice = s.slices()[0];
    ASSERT_TRUE(slice.name == "hitbox");
    ASSERT_EQ_INT(1, slice.rect.x);
    ASSERT_EQ_INT(3, slice.rect.w);
    ASSERT_TRUE(slice.hasPivot);
    ASSERT_EQ_INT(1, slice.pivotX);
}

TEST(a_file_without_image_layers_still_gives_a_sprite)
{
    Bytes bytes = file(2, 2, 32, {frame(100, {})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(1, s.layerCount());
    ASSERT_EQ_INT(1, s.frameCount());
}

TEST(a_zero_frame_duration_falls_back_to_the_default)
{
    Bytes bytes = file(1, 1, 32, {frame(0, {})});
    Sprite s = must_have(aseprite_decode(bytes.v));
    ASSERT_EQ_INT(FRAME_DURATION_DEFAULT, s.frameDuration(0));
}

TEST(invalid_files_are_refused)
{
    Bytes good = file(2, 1, 32,
                      {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                   cel_raw(0, 0, 0, 255, 2, 1, px({1, 2, 3, 4, 5, 6, 7, 8}))})});
    ASSERT_TRUE(aseprite_decode(good.v).has_value());
    ASSERT_TRUE(!aseprite_decode({}).has_value());
    std::vector<uint8_t> badMagic = good.v;
    badMagic[4] = 0;
    ASSERT_TRUE(!aseprite_decode(badMagic).has_value());
    std::vector<uint8_t> badFrame = good.v;
    badFrame[128 + 4] = 0;
    ASSERT_TRUE(!aseprite_decode(badFrame).has_value());
    ASSERT_TRUE(!aseprite_decode(file(5000, 1, 32, {frame(100, {})}).v).has_value());
    ASSERT_TRUE(!aseprite_decode(file(1, 1, 24, {frame(100, {})}).v).has_value());
    ASSERT_TRUE(!aseprite_decode(file(1, 1, 32, {}).v).has_value());
}

TEST(a_file_cut_short_anywhere_is_refused_or_read_without_a_crash)
{
    Bytes good =
        file(2, 1, 32,
             {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                          cel_compressed(0, 0, 0, 255, 2, 1, px({1, 2, 3, 4, 5, 6, 7, 8}))})});
    for (size_t n = 0; n + 1 < good.v.size(); n++) {
        std::vector<uint8_t> cut(good.v.begin(), good.v.begin() + static_cast<long>(n));
        ASSERT_TRUE(!aseprite_decode(cut).has_value());
    }
}

TEST(a_cel_whose_data_does_not_match_its_size_is_refused)
{
    Bytes small = file(4, 4, 32,
                       {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                    cel_compressed(0, 0, 0, 255, 4, 4, px({1, 2, 3, 4}))})});
    ASSERT_TRUE(!aseprite_decode(small.v).has_value());
    Bytes huge = file(4, 4, 32,
                      {frame(100, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                   cel_compressed(0, 0, 0, 255, 65535, 65535, px({1, 2, 3, 4}))})});
    ASSERT_TRUE(!aseprite_decode(huge.v).has_value());
}

TEST(damaged_bytes_never_crash_the_reader)
{
    Bytes good = file(2, 2, 8,
                      {frame(100, {palette_chunk({{1, 2, 3, 255}, {4, 5, 6, 255}}),
                                   layer(VISIBLE, 0, 0, 0, 255, "L"),
                                   cel_compressed(0, 0, 0, 255, 2, 2, px({0, 1, 1, 0})),
                                   tag_chunk({{0, 0, 0}}, {"t"})})});
    for (size_t i = 0; i < good.v.size(); i++) {
        std::vector<uint8_t> damaged = good.v;
        damaged[i] ^= 0xA5;
        (void)aseprite_decode(damaged); /* any answer, no crash */
    }
}

TEST(a_file_is_loaded_by_its_content_and_image_import_opens_it_too)
{
    Bytes bytes = file(1, 1, 32,
                       {frame(70, {layer(VISIBLE, 0, 0, 0, 255, "L"),
                                   cel_raw(0, 0, 0, 255, 1, 1, px({1, 2, 3, 255}))})});
    const std::string path = "test_aseprite_tmp.dat"; /* the name does not matter */
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char *>(bytes.v.data()),
               static_cast<std::streamsize>(bytes.v.size()));
    Sprite direct = must_have(aseprite_load(path));
    ASSERT_EQ_INT(70, direct.frameDuration(0));
    Sprite imported = must_have(image_import(path));
    ASSERT_TRUE(at(imported, 0, 0, 0, 0) == Rgba({1, 2, 3, 255}));
    ASSERT_TRUE(!aseprite_load("no_such_file.aseprite").has_value());
    std::remove(path.c_str());
}

int main()
{
    RUN(a_plain_rgba_file_becomes_a_sprite_with_frames_and_layers);
    RUN(a_compressed_cel_reads_like_a_raw_one);
    RUN(a_cel_is_placed_at_its_offset_and_clipped_to_the_sprite);
    RUN(layer_flags_opacity_and_blend_mode_are_kept);
    RUN(groups_are_flattened_and_a_hidden_group_hides_its_layers);
    RUN(the_opacity_of_a_cel_goes_into_its_pixels);
    RUN(a_linked_cel_shares_the_cel_of_the_earlier_frame);
    RUN(an_indexed_file_looks_its_colors_up_in_the_palette);
    RUN(a_grayscale_file_becomes_gray_pixels);
    RUN(tags_keep_their_range_and_direction_and_bad_ones_are_dropped);
    RUN(slices_keep_their_first_key_and_pivot);
    RUN(a_file_without_image_layers_still_gives_a_sprite);
    RUN(a_zero_frame_duration_falls_back_to_the_default);
    RUN(invalid_files_are_refused);
    RUN(a_file_cut_short_anywhere_is_refused_or_read_without_a_crash);
    RUN(a_cel_whose_data_does_not_match_its_size_is_refused);
    RUN(damaged_bytes_never_crash_the_reader);
    RUN(a_file_is_loaded_by_its_content_and_image_import_opens_it_too);
    return TEST_SUMMARY();
}
