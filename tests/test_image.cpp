#include <cstdio>

#include "minitest.h"
#include "tinysprite/io.h"

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

int main(void)
{
    RUN(export_then_import_round_trips_the_flattened_image);
    RUN(lossless_formats_round_trip);
    RUN(jpeg_keeps_the_size_and_shows_transparency_as_white);
    RUN(unknown_extension_is_refused);
    RUN(semi_transparent_pixels_survive);
    RUN(bad_frame_or_path_fails);
    RUN(non_image_file_is_rejected);
    RUN(export_can_enlarge_by_whole_pixels);
    RUN(export_rejects_a_bad_scale);
    RUN(write_formats_list_what_export_accepts);
    return TEST_SUMMARY();
}
