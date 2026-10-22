#include <cstdio>
#include <fstream>
#include <string>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

static Palette sample()
{
    Palette p;
    p.add(Rgba{255, 0, 0, 255});
    p.add(Rgba{0, 128, 255, 255});
    p.add(Rgba{1, 2, 3, 128});
    return p;
}

static bool same(const Palette &a, const Palette &b)
{
    if (a.count() != b.count()) {
        return false;
    }
    for (int i = 0; i < a.count(); i++) {
        if (a.get(i) != b.get(i)) {
            return false;
        }
    }
    return true;
}

TEST(hex_round_trips_with_alpha)
{
    std::string text = palette_encode_hex(sample());
    ASSERT_TRUE(text == "ff0000\n0080ff\n01020380\n");
    std::optional<Palette> back = palette_decode(text);
    ASSERT_TRUE(back.has_value() && same(*back, sample()));
}

TEST(hex_accepts_hash_blank_lines_crlf_and_case)
{
    std::optional<Palette> p = palette_decode("#FF0000\r\n\r\n  00ff00  \r\n0000FF80\n");
    ASSERT_TRUE(p.has_value());
    if (!p) {
        return;
    }
    ASSERT_EQ_INT(3, p->count());
    ASSERT_TRUE(p->get(0) == (Rgba{255, 0, 0, 255}));
    ASSERT_TRUE(p->get(1) == (Rgba{0, 255, 0, 255}));
    ASSERT_TRUE(p->get(2) == (Rgba{0, 0, 255, 128}));
}

TEST(gpl_round_trips_the_opaque_colors)
{
    Palette opaque;
    opaque.add(Rgba{255, 0, 0, 255});
    opaque.add(Rgba{0, 128, 255, 255});
    std::string text = palette_encode_gpl(opaque, "My colors");
    ASSERT_TRUE(text.rfind("GIMP Palette\n", 0) == 0);
    ASSERT_TRUE(text.find("Name: My colors") != std::string::npos);
    std::optional<Palette> back = palette_decode(text);
    ASSERT_TRUE(back.has_value() && same(*back, opaque));
}

TEST(gpl_reads_the_usual_layout)
{
    const char *text =
        "GIMP Palette\nName: Test\nColumns: 4\n#\n255   0   0\tRed\n  0 128 255 Sky blue\n";
    std::optional<Palette> p = palette_decode(text);
    ASSERT_TRUE(p.has_value());
    if (!p) {
        return;
    }
    ASSERT_EQ_INT(2, p->count());
    ASSERT_TRUE(p->get(1) == (Rgba{0, 128, 255, 255}));
}

TEST(gpl_drops_alpha_on_export)
{
    std::optional<Palette> back = palette_decode(palette_encode_gpl(sample(), "x"));
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(255, back->get(2).a);
    ASSERT_EQ_INT(1, back->get(2).r);
}

TEST(invalid_or_empty_text_is_rejected)
{
    ASSERT_TRUE(!palette_decode("").has_value());
    ASSERT_TRUE(!palette_decode("\n\n").has_value());
    ASSERT_TRUE(!palette_decode("zzzzzz\n").has_value());
    ASSERT_TRUE(!palette_decode("ff00\n").has_value());
    ASSERT_TRUE(!palette_decode("GIMP Palette\n300 0 0 Bad\n").has_value());
    ASSERT_TRUE(!palette_decode("GIMP Palette\nName: only a header\n").has_value());
}

TEST(only_the_first_256_colors_are_kept)
{
    std::string text;
    for (int i = 0; i < 300; i++) {
        char line[16];
        std::snprintf(line, sizeof(line), "%02x%02x00\n", i % 256, i / 256);
        text += line;
    }
    std::optional<Palette> p = palette_decode(text);
    ASSERT_TRUE(p.has_value());
    if (!p) {
        return;
    }
    ASSERT_EQ_INT(PALETTE_MAX_COLORS, p->count());
}

TEST(save_and_load_follow_the_extension)
{
    const char *hex = "test_palette_io.hex";
    const char *gpl = "test_palette_io.gpl";
    ASSERT_TRUE(palette_save(sample(), hex));
    std::optional<Palette> a = palette_load(hex);
    ASSERT_TRUE(a.has_value() && same(*a, sample()));
    ASSERT_TRUE(palette_save(sample(), gpl));
    std::optional<Palette> b = palette_load(gpl);
    ASSERT_TRUE(b.has_value() && b->count() == 3);
    ASSERT_TRUE(!palette_save(sample(), "test_palette_io.txt"));
    ASSERT_TRUE(!palette_load("no_such_palette.hex").has_value());
    std::remove(hex);
    std::remove(gpl);
}

TEST(atomic_write_replaces_a_file_and_leaves_no_temp)
{
    const char *path = "test_atomic_write.txt";
    ASSERT_TRUE(file_write_atomic(path, "one"));
    ASSERT_TRUE(file_write_atomic(path, "two"));
    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_TRUE(text == "two");
    ASSERT_TRUE(!std::ifstream(std::string(path) + ".tmp").good());
    ASSERT_TRUE(!file_write_atomic("no_such_dir/x.txt", "x"));
    std::remove(path);
}

TEST(formats_list_the_extensions_that_save_and_load_accept)
{
    ASSERT_TRUE(palette_formats() == ".gpl or .hex");
    Palette p;
    p.add(Rgba{1, 2, 3, 255});
    for (const char *ext : {".gpl", ".hex", ".GPL"}) {
        std::string path = std::string("test_palette_formats") + ext;
        ASSERT_TRUE(palette_save(p, path));
        ASSERT_TRUE(palette_load(path).has_value());
        std::remove(path.c_str());
    }
    ASSERT_TRUE(!palette_save(p, "test_palette_formats.txt"));
}

int main()
{
    RUN(hex_round_trips_with_alpha);
    RUN(hex_accepts_hash_blank_lines_crlf_and_case);
    RUN(gpl_round_trips_the_opaque_colors);
    RUN(gpl_reads_the_usual_layout);
    RUN(gpl_drops_alpha_on_export);
    RUN(invalid_or_empty_text_is_rejected);
    RUN(only_the_first_256_colors_are_kept);
    RUN(save_and_load_follow_the_extension);
    RUN(formats_list_the_extensions_that_save_and_load_accept);
    RUN(atomic_write_replaces_a_file_and_leaves_no_temp);
    return TEST_SUMMARY();
}
