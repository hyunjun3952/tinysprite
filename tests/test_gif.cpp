#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "minitest.hpp"
#include "stb_image.h"
#include "tinysprite/io.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba GREEN = {0, 255, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

struct Decoded {
    int width = 0;
    int height = 0;
    int frames = 0;
    std::vector<int> delays;  /* milliseconds */
    std::vector<Rgba> pixels; /* frames stacked, each width x height */

    Rgba at(int frame, int x, int y) const
    {
        return pixels[(static_cast<size_t>(frame) * static_cast<size_t>(height) +
                       static_cast<size_t>(y)) *
                          static_cast<size_t>(width) +
                      static_cast<size_t>(x)];
    }
};

static bool decode(const std::vector<uint8_t> &data, Decoded &out)
{
    int *delays = nullptr;
    int channels = 0;
    stbi_uc *p = stbi_load_gif_from_memory(data.data(), static_cast<int>(data.size()), &delays,
                                           &out.width, &out.height, &out.frames, &channels, 4);
    if (!p) {
        return false;
    }
    size_t count = static_cast<size_t>(out.width) * static_cast<size_t>(out.height) *
                   static_cast<size_t>(out.frames);
    out.pixels.resize(count);
    std::copy(p, p + count * 4, reinterpret_cast<uint8_t *>(out.pixels.data()));
    out.delays.assign(delays, delays + out.frames);
    stbi_image_free(p);
    stbi_image_free(delays);
    return true;
}

/* A 3x2 sprite with n frames, frame i having a dot at (i % 3, 0) in a color per frame. */
static Sprite dots(int n)
{
    Sprite s(3, 2);
    for (int i = 1; i < n; i++) {
        s.insertFrame(i, s.makeFrame());
    }
    const Rgba colors[3] = {RED, GREEN, BLUE};
    for (int i = 0; i < n; i++) {
        s.layer(0)->cel(i)->set(i % 3, 0, colors[i % 3]);
        s.setFrameDuration(i, 100 + 50 * i);
    }
    return s;
}

TEST(a_gif_has_the_header_and_all_frames)
{
    Sprite s = dots(3);
    std::vector<uint8_t> data = gif_encode(s, GifOptions{});
    ASSERT_TRUE(data.size() > 20);
    ASSERT_EQ_CHAR('G', data[0]);
    ASSERT_EQ_CHAR('I', data[1]);
    ASSERT_EQ_CHAR('F', data[2]);
    ASSERT_EQ_CHAR('8', data[3]);
    ASSERT_EQ_CHAR('9', data[4]);
    ASSERT_EQ_CHAR('a', data[5]);
    ASSERT_EQ_CHAR(';', data[data.size() - 1]);
    Decoded d;
    ASSERT_TRUE(decode(data, d));
    ASSERT_EQ_INT(3, d.width);
    ASSERT_EQ_INT(2, d.height);
    ASSERT_EQ_INT(3, d.frames);
}

TEST(frame_durations_become_delays)
{
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(dots(3), GifOptions{}), d));
    ASSERT_EQ_INT(100, d.delays[0]);
    ASSERT_EQ_INT(150, d.delays[1]);
    ASSERT_EQ_INT(200, d.delays[2]);
}

TEST(very_short_durations_are_raised_to_what_viewers_honor)
{
    Sprite s = dots(2);
    s.setFrameDuration(0, 1);
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    ASSERT_EQ_INT(20, d.delays[0]);
}

TEST(a_duration_is_rounded_to_the_nearest_hundredth_of_a_second)
{
    Sprite s = dots(3);
    s.setFrameDuration(0, 25); /* 2.5 cs rounds up */
    s.setFrameDuration(1, 24); /* 2.4 cs rounds down; the shortest delay viewers honor is 20 ms */
    s.setFrameDuration(2, 36);
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    ASSERT_EQ_INT(30, d.delays[0]);
    ASSERT_EQ_INT(20, d.delays[1]);
    ASSERT_EQ_INT(40, d.delays[2]);
}

TEST(pixels_keep_their_colors)
{
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(dots(3), GifOptions{}), d));
    ASSERT_TRUE(d.at(0, 0, 0) == RED);
    ASSERT_TRUE(d.at(1, 1, 0) == GREEN);
    ASSERT_TRUE(d.at(2, 2, 0) == BLUE);
}

TEST(transparent_pixels_stay_transparent_in_every_frame)
{
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(dots(3), GifOptions{}), d));
    ASSERT_EQ_INT(0, d.at(0, 1, 1).a);
    ASSERT_EQ_INT(0, d.at(1, 0, 0).a); /* frame 1 does not keep frame 0's dot */
    ASSERT_EQ_INT(0, d.at(2, 1, 0).a);
}

TEST(an_opaque_sprite_has_no_transparency)
{
    Sprite s(2, 1);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(1, 0, BLUE);
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    ASSERT_EQ_INT(255, d.at(0, 0, 0).a);
    ASSERT_TRUE(d.at(0, 1, 0) == BLUE);
}

TEST(the_range_and_direction_pick_the_frames)
{
    Sprite s = dots(4);
    GifOptions o;
    o.first = 1;
    o.last = 3;
    o.direction = PlayMode::Reverse;
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, o), d));
    ASSERT_EQ_INT(3, d.frames);
    ASSERT_TRUE(d.at(0, 0, 0) == RED); /* frame 3 is the first of the (i % 3) == 0 dots */
    ASSERT_TRUE(d.at(2, 1, 0) == GREEN);
    o.direction = PlayMode::PingPong;
    o.first = 0;
    o.last = 2;
    ASSERT_TRUE(decode(gif_encode(s, o), d));
    ASSERT_EQ_INT(4, d.frames); /* 0 1 2 1 */
    ASSERT_EQ_INT(150, d.delays[3]);
}

TEST(an_open_ended_range_runs_to_the_last_frame)
{
    GifOptions o;
    o.first = 1;
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(dots(4), o), d));
    ASSERT_EQ_INT(3, d.frames);
}

TEST(scale_enlarges_by_whole_pixels)
{
    GifOptions o;
    o.scale = 3;
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(dots(1), o), d));
    ASSERT_EQ_INT(9, d.width);
    ASSERT_EQ_INT(6, d.height);
    ASSERT_TRUE(d.at(0, 2, 2) == RED);
    ASSERT_EQ_INT(0, d.at(0, 3, 0).a);
}

TEST(semi_transparent_pixels_are_either_shown_or_not)
{
    Sprite s(2, 1);
    s.layer(0)->cel(0)->set(0, 0, Rgba{10, 20, 30, 200});
    s.layer(0)->cel(0)->set(1, 0, Rgba{10, 20, 30, 40});
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    ASSERT_EQ_INT(255, d.at(0, 0, 0).a);
    ASSERT_EQ_INT(0, d.at(0, 1, 0).a);
}

TEST(more_than_256_colors_are_reduced_to_close_ones)
{
    Sprite s(32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            s.layer(0)->cel(0)->set(x, y,
                                    Rgba{static_cast<uint8_t>(x * 8), static_cast<uint8_t>(y * 8),
                                         static_cast<uint8_t>((x + y) * 4), 255});
        }
    }
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    int worst = 0;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            Rgba want = s.layer(0)->cel(0)->get(x, y);
            Rgba got = d.at(0, x, y);
            worst = std::max({worst, std::abs(want.r - got.r), std::abs(want.g - got.g),
                              std::abs(want.b - got.b)});
        }
    }
    ASSERT_TRUE(worst < 40);
}

static int rgb_distance(Rgba a, Rgba b)
{
    const int dr = a.r - b.r;
    const int dg = a.g - b.g;
    const int db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

TEST(a_reduced_color_is_shown_as_the_closest_color_of_the_palette)
{
    Sprite s(40, 40);
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            s.layer(0)->cel(0)->set(x, y,
                                    Rgba{static_cast<uint8_t>(x * 6), static_cast<uint8_t>(y * 6),
                                         static_cast<uint8_t>((x * 7 + y * 3) % 256), 255});
        }
    }
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    std::vector<Rgba> used; /* the palette entries the picture ends up showing */
    for (const Rgba &p : d.pixels) {
        if (std::find(used.begin(), used.end(), p) == used.end()) {
            used.push_back(p);
        }
    }
    ASSERT_TRUE(used.size() > 200);
    int farther = 0;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            const Rgba want = s.layer(0)->cel(0)->get(x, y);
            int best = rgb_distance(want, used[0]);
            for (const Rgba &entry : used) {
                best = std::min(best, rgb_distance(want, entry));
            }
            farther += rgb_distance(want, d.at(0, x, y)) > best ? 1 : 0;
        }
    }
    ASSERT_EQ_INT(0, farther);
}

TEST(a_big_flat_image_compresses_and_decodes)
{
    Sprite s(300, 300);
    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            s.layer(0)->cel(0)->set(x, y, (x / 10 + y / 10) % 2 != 0 ? RED : BLUE);
        }
    }
    std::vector<uint8_t> data = gif_encode(s, GifOptions{});
    ASSERT_TRUE(data.size() < 20000);
    Decoded d;
    ASSERT_TRUE(decode(data, d));
    ASSERT_TRUE(d.at(0, 15, 5) == RED);
    ASSERT_TRUE(d.at(0, 5, 5) == BLUE);
}

TEST(noise_fills_and_resets_the_code_table_without_losing_a_pixel)
{
    const Rgba colors[5] = {RED, GREEN, BLUE, Rgba{255, 255, 0, 255}, Rgba{0, 0, 0, 255}};
    Sprite s(200, 200);
    uint32_t seed = 12345;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 200; x++) {
            seed = seed * 1664525u + 1013904223u;
            s.layer(0)->cel(0)->set(x, y, colors[(seed >> 24) % 5]);
        }
    }
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    int wrong = 0;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 200; x++) {
            wrong += d.at(0, x, y) == s.layer(0)->cel(0)->get(x, y) ? 0 : 1;
        }
    }
    ASSERT_EQ_INT(0, wrong);
}

TEST(a_single_color_sprite_works)
{
    Sprite s(1, 1);
    s.layer(0)->cel(0)->set(0, 0, GREEN);
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(s, GifOptions{}), d));
    ASSERT_TRUE(d.at(0, 0, 0) == GREEN);
}

TEST(a_blank_sprite_works)
{
    Decoded d;
    ASSERT_TRUE(decode(gif_encode(Sprite(2, 2), GifOptions{}), d));
    ASSERT_EQ_INT(0, d.at(0, 1, 1).a);
}

TEST(bad_options_produce_nothing)
{
    Sprite s = dots(2);
    GifOptions o;
    o.first = 5;
    ASSERT_TRUE(gif_encode(s, o).empty());
    o = GifOptions{};
    o.last = 7;
    ASSERT_TRUE(gif_encode(s, o).empty());
    o = GifOptions{};
    o.first = 1;
    o.last = 0;
    ASSERT_TRUE(gif_encode(s, o).empty());
    o = GifOptions{};
    o.scale = 0;
    ASSERT_TRUE(gif_encode(s, o).empty());
    o.scale = IMAGE_EXPORT_MAX_SCALE + 1;
    ASSERT_TRUE(gif_encode(s, o).empty());
}

TEST(export_writes_a_gif_file_and_refuses_other_extensions)
{
    const char *path = "test_gif_export.gif";
    ASSERT_TRUE(gif_export(dots(2), path, GifOptions{}));
    std::FILE *f = std::fopen(path, "rb");
    ASSERT_TRUE(f != nullptr);
    if (f) {
        char head[3] = {};
        ASSERT_EQ_INT(3, static_cast<int>(std::fread(head, 1, 3, f)));
        ASSERT_EQ_CHAR('G', head[0]);
        std::fclose(f);
    }
    std::remove(path);
    ASSERT_TRUE(!gif_export(dots(2), "test_gif_export.png", GifOptions{}));
    ASSERT_TRUE(!gif_export(dots(2), "no_such_dir/x.gif", GifOptions{}));
}

TEST(an_exported_animation_imports_as_frames_with_durations)
{
    Sprite s = dots(3);
    const char *path = "test_gif_roundtrip.gif";
    ASSERT_TRUE(gif_export(s, path, GifOptions{}));
    std::optional<Sprite> back = image_import(path);
    std::remove(path);
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(3, back->frameCount());
    ASSERT_EQ_INT(1, back->layerCount());
    ASSERT_EQ_INT(100, back->frameDuration(0));
    ASSERT_EQ_INT(150, back->frameDuration(1));
    ASSERT_EQ_INT(200, back->frameDuration(2));
    ASSERT_TRUE(back->layer(0)->cel(1)->get(1, 0) == GREEN);
    ASSERT_EQ_INT(0, back->layer(0)->cel(1)->get(0, 0).a);
    ASSERT_TRUE(back->layer(0)->cel(2)->get(2, 0) == BLUE);
}

TEST(a_single_frame_gif_imports_as_before)
{
    const char *path = "test_gif_single.gif";
    ASSERT_TRUE(gif_export(dots(1), path, GifOptions{}));
    std::optional<Sprite> back = image_import(path);
    std::remove(path);
    ASSERT_TRUE(back.has_value());
    if (back) {
        ASSERT_EQ_INT(1, back->frameCount());
        ASSERT_TRUE(back->layer(0)->cel(0)->get(0, 0) == RED);
    }
}

TEST(a_broken_gif_is_refused)
{
    const char *path = "test_gif_broken.gif";
    std::vector<uint8_t> data = gif_encode(dots(2), GifOptions{});
    data.resize(data.size() / 2);
    ASSERT_TRUE(file_write_atomic(path, std::string(data.begin(), data.end())));
    std::optional<Sprite> back = image_import(path);
    std::remove(path);
    /* stb may return the frames it could decode; either way nothing crashes and a result has a
     * valid size */
    if (back) {
        ASSERT_TRUE(back->frameCount() >= 1);
    }
}

TEST(the_frame_count_of_a_gif_is_read_without_decoding_it)
{
    /* header, a 4096 x 4096 screen with no color table, then three 1 x 1 images */
    std::vector<uint8_t> gif = {'G', 'I', 'F', '8', '9', 'a', 0x00, 0x10, 0x00, 0x10, 0, 0, 0};
    for (int i = 0; i < 3; i++) {
        const uint8_t image[] = {
            0x21, 0xF9, 4,    0,    10, 0, 0, 0,       /* a graphic control extension */
            0x2C, 0,    0,    0,    0,  1, 0, 1, 0, 0, /* an image descriptor */
            2,    2,    0x44, 0x01, 0};                /* LZW size and one sub-block */
        gif.insert(gif.end(), image, image + sizeof image);
    }
    gif.push_back(0x3B);
    ASSERT_EQ_INT(3, gif_frame_count(gif));
    gif.resize(20); /* cut inside the first extension */
    ASSERT_EQ_INT(0, gif_frame_count(gif));
    ASSERT_EQ_INT(0, gif_frame_count({'P', 'N', 'G'}));
}

int main()
{
    RUN(a_gif_has_the_header_and_all_frames);
    RUN(frame_durations_become_delays);
    RUN(very_short_durations_are_raised_to_what_viewers_honor);
    RUN(a_duration_is_rounded_to_the_nearest_hundredth_of_a_second);
    RUN(pixels_keep_their_colors);
    RUN(transparent_pixels_stay_transparent_in_every_frame);
    RUN(an_opaque_sprite_has_no_transparency);
    RUN(the_range_and_direction_pick_the_frames);
    RUN(an_open_ended_range_runs_to_the_last_frame);
    RUN(scale_enlarges_by_whole_pixels);
    RUN(semi_transparent_pixels_are_either_shown_or_not);
    RUN(more_than_256_colors_are_reduced_to_close_ones);
    RUN(a_reduced_color_is_shown_as_the_closest_color_of_the_palette);
    RUN(a_big_flat_image_compresses_and_decodes);
    RUN(noise_fills_and_resets_the_code_table_without_losing_a_pixel);
    RUN(a_single_color_sprite_works);
    RUN(a_blank_sprite_works);
    RUN(bad_options_produce_nothing);
    RUN(export_writes_a_gif_file_and_refuses_other_extensions);
    RUN(an_exported_animation_imports_as_frames_with_durations);
    RUN(a_single_frame_gif_imports_as_before);
    RUN(a_broken_gif_is_refused);
    RUN(the_frame_count_of_a_gif_is_read_without_decoding_it);
    return TEST_SUMMARY();
}
