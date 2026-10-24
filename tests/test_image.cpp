#include <cstdio>
#include <fstream>
#include <vector>
#ifdef __linux__
#include <sys/resource.h>
#endif

#include "minitest.hpp"
#include "tinysprite/io.hpp"

static const Rgba RED = {255, 0, 0, 255};
static const Rgba BLUE = {0, 0, 255, 255};

TEST(export_then_import_round_trips_the_flattened_image)
{
    Sprite s(4, 3);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(3, 2, BLUE);
    Layer &hidden = s.addLayer("hidden");
    hidden.cel(0)->set(1, 1, BLUE);
    hidden.setVisible(false);

    std::string path = "test_image_roundtrip.png";
    ASSERT_TRUE(image_export(s, 0, path));
    std::optional<Sprite> back = image_import(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    const Sprite &b = *back;
    ASSERT_EQ_INT(4, b.width());
    ASSERT_EQ_INT(3, b.height());
    ASSERT_EQ_INT(1, b.layerCount());
    ASSERT_EQ_INT(0, b.palette().count());
    ASSERT_TRUE(b.layer(0)->cel(0)->get(0, 0) == RED);
    ASSERT_TRUE(b.layer(0)->cel(0)->get(3, 2) == BLUE);
    ASSERT_EQ_INT(0, b.layer(0)->cel(0)->get(1, 1).a); /* hidden layers are not exported */
}

TEST(an_image_with_a_korean_file_name_imports)
{
    /* Paths are UTF-8; on Windows the narrow file APIs would read them in the ANSI code page. */
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(1, 1, RED);
    std::string path = "test_image_\xED\x95\x9C\xEA\xB8\x80.png";
    ASSERT_TRUE(image_export(s, 0, path));
    std::optional<Sprite> back = image_import(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_EQ_INT(2, back->width());
    ASSERT_TRUE(back->layer(0)->cel(0)->get(1, 1) == RED);
}

TEST(write_rgba_saves_raw_pixels_as_png)
{
    std::vector<Rgba> pixels = {RED, BLUE, BLUE, RED, RED, BLUE};
    std::string path = "test_image_write_rgba.png";
    ASSERT_TRUE(image_write_rgba(path, 3, 2, pixels));
    std::optional<Sprite> back = image_import(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_EQ_INT(3, back->width());
    ASSERT_EQ_INT(2, back->height());
    ASSERT_TRUE(back->layer(0)->cel(0)->get(0, 0) == RED);
    ASSERT_TRUE(back->layer(0)->cel(0)->get(1, 0) == BLUE);
    ASSERT_TRUE(back->layer(0)->cel(0)->get(2, 1) == BLUE);
    std::vector<Rgba> none = {RED};
    ASSERT_TRUE(!image_write_rgba("test_image_write_rgba.xyz", 1, 1, none));
}

TEST(lossless_formats_round_trip)
{
    for (const char *extension : {".bmp", ".tga"}) {
        Sprite s(3, 2);
        s.layer(0)->cel(0)->set(0, 1, Rgba{10, 20, 30, 99});
        s.layer(0)->cel(0)->set(2, 0, RED);
        std::string path = std::string("test_image_format") + extension;
        ASSERT_TRUE(image_export(s, 0, path));
        std::optional<Sprite> back = image_import(path);
        std::remove(path.c_str());
        if (!back) {
            MT_FAIL("%s did not load", extension);
            continue;
        }
        ASSERT_TRUE(back->layer(0)->cel(0)->get(0, 1) == (Rgba{10, 20, 30, 99}));
        ASSERT_TRUE(back->layer(0)->cel(0)->get(2, 0) == RED);
    }
}

TEST(jpeg_keeps_the_size_and_shows_transparency_as_white)
{
    Sprite s(16, 16);
    for (const char *name : {"test_image_jpeg.jpg", "test_image_jpeg.JPEG"}) {
        ASSERT_TRUE(image_export(s, 0, name));
        std::optional<Sprite> back = image_import(name);
        std::remove(name);
        if (!back) {
            MT_FAIL("%s did not load", name);
            continue;
        }
        ASSERT_EQ_INT(16, back->width());
        Rgba corner = back->layer(0)->cel(0)->get(8, 8);
        ASSERT_TRUE(corner.r > 250 && corner.g > 250 && corner.b > 250 && corner.a == 255);
    }
}

TEST(unknown_extension_is_refused)
{
    Sprite s(2, 2);
    ASSERT_TRUE(!image_export(s, 0, "out.gif"));
    ASSERT_TRUE(!image_export(s, 0, "out"));
}

TEST(semi_transparent_pixels_survive)
{
    Sprite s(2, 1);
    s.layer(0)->cel(0)->set(1, 0, Rgba{10, 20, 30, 99});
    std::string path = "test_image_alpha.png";
    ASSERT_TRUE(image_export(s, 0, path));
    std::optional<Sprite> back = image_import(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_TRUE(back->layer(0)->cel(0)->get(1, 0) == (Rgba{10, 20, 30, 99}));
}

TEST(bad_frame_or_path_fails)
{
    Sprite s(2, 2);
    ASSERT_TRUE(!image_export(s, 1, "unused.png"));
    ASSERT_TRUE(!image_export(s, 0, "does/not/exist/out.png"));
    ASSERT_TRUE(!image_import("does/not/exist.png"));
}

TEST(non_image_file_is_rejected)
{
    std::string path = "test_image_garbage.png";
    FILE *f = std::fopen(path.c_str(), "wb");
    ASSERT_TRUE(f != nullptr);
    if (f) {
        std::fputs("this is not a png", f);
        std::fclose(f);
    }
    ASSERT_TRUE(!image_import(path));
    std::remove(path.c_str());
}

TEST(export_can_enlarge_by_whole_pixels)
{
    Sprite s(2, 2);
    s.layer(0)->cel(0)->set(0, 0, RED);
    s.layer(0)->cel(0)->set(1, 1, BLUE);
    std::string path = "test_image_scaled.png";
    ASSERT_TRUE(image_export(s, 0, path, 3));
    std::optional<Sprite> back = image_import(path);
    std::remove(path.c_str());
    if (!back) {
        MT_FAIL("no sprite");
        return;
    }
    ASSERT_EQ_INT(6, back->width());
    ASSERT_EQ_INT(6, back->height());
    const Cel &c = *back->layer(0)->cel(0);
    ASSERT_TRUE(c.get(0, 0) == RED && c.get(2, 2) == RED);
    ASSERT_EQ_INT(0, c.get(3, 0).a);
    ASSERT_TRUE(c.get(3, 3) == BLUE && c.get(5, 5) == BLUE);
}

TEST(export_rejects_a_bad_scale)
{
    Sprite s(100, 100);
    ASSERT_TRUE(!image_export(s, 0, "unused.png", 0));
    ASSERT_TRUE(!image_export(s, 0, "unused.png", IMAGE_EXPORT_MAX_SCALE + 1));
    Sprite wide(1000, 10);
    ASSERT_TRUE(!image_export(wide, 0, "unused.png", 17)); /* 17000 px is over the size limit */
}

TEST(write_formats_list_what_export_accepts)
{
    std::string formats = image_write_formats();
    ASSERT_TRUE(formats == ".png, .bmp, .tga or .jpg");
    /* every listed extension really is written */
    Sprite s(2, 2);
    for (const char *ext : {".png", ".bmp", ".tga", ".jpg", ".jpeg"}) {
        std::string path = std::string("test_image_formats") + ext;
        ASSERT_TRUE(image_export(s, 0, path));
        std::remove(path.c_str());
    }
}

TEST(an_export_follows_the_layer_blend_mode)
{
    Sprite s(1, 1);
    s.layer(0)->cel(0)->set(0, 0, Rgba{200, 100, 50, 255});
    Layer &shade = s.addLayer("shade");
    shade.cel(0)->set(0, 0, Rgba{128, 255, 10, 255});

    auto exported = [&](BlendMode mode) {
        shade.setBlendMode(mode);
        std::string path = "test_image_blend.png";
        bool ok = image_export(s, 0, path);
        std::optional<Sprite> back = ok ? image_import(path) : std::nullopt;
        std::remove(path.c_str());
        return back ? back->layer(0)->cel(0)->get(0, 0) : Rgba{};
    };
    Rgba normal = exported(BlendMode::Normal);
    Rgba multiply = exported(BlendMode::Multiply);
    ASSERT_EQ_INT(128, normal.r);
    ASSERT_EQ_INT(100, multiply.r);
    ASSERT_EQ_INT(2, multiply.b);
    ASSERT_TRUE(!(normal == multiply));
}

#ifdef __linux__
/* A GIF that declares a huge screen but holds almost no data. */
static std::vector<uint8_t> hugeDeclaredGif(int dim)
{
    auto put16 = [](std::vector<uint8_t> &v, int x) {
        v.push_back(static_cast<uint8_t>(x & 255));
        v.push_back(static_cast<uint8_t>(x >> 8));
    };
    std::vector<uint8_t> g = {'G', 'I', 'F', '8', '9', 'a'};
    put16(g, dim);
    put16(g, dim);
    g.insert(g.end(), {0x80, 0, 0, 0, 0, 0, 255, 255, 255});
    g.push_back(0x2c);
    put16(g, 0);
    put16(g, 0);
    put16(g, dim);
    put16(g, dim);
    g.push_back(0);
    g.insert(g.end(), {2, 2, 0x44, 0x01, 0, 0x3b});
    return g;
}

TEST(an_oversized_image_is_refused_before_it_is_decoded)
{
    const char *path = "test_image_huge.gif";
    std::vector<uint8_t> gif = hugeDeclaredGif(8192); /* 268 MB per buffer once decoded */
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char *>(gif.data()),
               static_cast<std::streamsize>(gif.size()));
    struct rusage before {
    };
    getrusage(RUSAGE_SELF, &before);
    ASSERT_TRUE(!image_import(path).has_value());
    struct rusage after {
    };
    getrusage(RUSAGE_SELF, &after);
    std::remove(path);
    long grewKb = after.ru_maxrss - before.ru_maxrss;
    ASSERT_TRUE(grewKb < 100L * 1024L);
}
#endif

/* An animated GIF of `frames` 1 x 1 images. */
static std::vector<uint8_t> tinyAnimation(int frames)
{
    std::vector<uint8_t> g = {'G',  'I', 'F', '8', '9', 'a', 1,   0,   1,  0,
                              0x80, 0,   0,   0,   0,   0,   255, 255, 255};
    for (int i = 0; i < frames; i++) {
        g.insert(g.end(), {0x21, 0xf9, 4, 0, 5, 0, 0, 0});
        g.insert(g.end(), {0x2c, 0, 0, 0, 0, 1, 0, 1, 0, 0});
        g.insert(g.end(), {2, 2, 0x44, 0x01, 0});
    }
    g.push_back(0x3b);
    return g;
}

TEST(an_animation_longer_than_the_frame_limit_is_cut_at_it)
{
    const char *path = "test_image_long.gif";
    std::vector<uint8_t> gif = tinyAnimation(SPRITE_MAX_FRAMES + 20);
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char *>(gif.data()),
               static_cast<std::streamsize>(gif.size()));
    std::optional<Sprite> sprite = image_import(path);
    std::remove(path);
    ASSERT_TRUE(sprite.has_value());
    if (sprite) {
        ASSERT_EQ_INT(SPRITE_MAX_FRAMES, sprite->frameCount());
    }
}

TEST(a_failed_image_write_says_why)
{
    Sprite s(2, 2);
    ASSERT_TRUE(!image_export(s, 0, "/nonexistent-tinysprite-dir/out.png"));
    ASSERT_TRUE(!file_write_error().empty());
}

TEST(a_refused_image_clears_an_older_reason)
{
    Sprite s(2, 2);
    image_export(s, 0, "/nonexistent-tinysprite-dir/out.png");
    ASSERT_TRUE(!image_export(s, 0, "out.xyz"));
    ASSERT_TRUE(file_write_error().empty());
    ASSERT_TRUE(!image_export(s, 0, "out.png", 0));
    ASSERT_TRUE(file_write_error().empty());
}

TEST(a_failed_image_write_leaves_no_file_behind)
{
    Sprite s(2, 2);
    ASSERT_TRUE(image_export(s, 0, "test_image_atomic.png"));
    std::ifstream file("test_image_atomic.png.tmp");
    ASSERT_TRUE(!file.good());
    std::remove("test_image_atomic.png");
}

int main()
{
    RUN(a_failed_image_write_says_why);
    RUN(a_refused_image_clears_an_older_reason);
    RUN(a_failed_image_write_leaves_no_file_behind);
    RUN(export_then_import_round_trips_the_flattened_image);
    RUN(lossless_formats_round_trip);
    RUN(an_image_with_a_korean_file_name_imports);
    RUN(write_rgba_saves_raw_pixels_as_png);
    RUN(jpeg_keeps_the_size_and_shows_transparency_as_white);
    RUN(unknown_extension_is_refused);
    RUN(semi_transparent_pixels_survive);
    RUN(bad_frame_or_path_fails);
    RUN(non_image_file_is_rejected);
    RUN(export_can_enlarge_by_whole_pixels);
    RUN(export_rejects_a_bad_scale);
    RUN(write_formats_list_what_export_accepts);
    RUN(an_animation_longer_than_the_frame_limit_is_cut_at_it);
    RUN(an_export_follows_the_layer_blend_mode);
#ifdef __linux__
    RUN(an_oversized_image_is_refused_before_it_is_decoded);
#endif
    return TEST_SUMMARY();
}
