#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/cli.hpp"
#include "tinysprite/io.hpp"

static int run(std::vector<std::string> args)
{
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>("tinysprite"));
    for (std::string &a : args) {
        argv.push_back(a.data());
    }
    return cli_export(static_cast<int>(argv.size()), argv.data());
}

static std::string slurp(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void make_input(const char *path)
{
    Sprite s(4, 2);
    s.insertFrame(1, s.makeFrame());
    s.layer(0)->cel(1)->set(0, 0, Rgba{255, 0, 0, 255});
    pxa_save(s, path);
}

TEST(exports_a_sheet_and_its_json)
{
    make_input("cli_in.pxa");
    ASSERT_EQ_INT(
        0, run({"export", "cli_in.pxa", "--sheet", "cli_out.png", "--json", "cli_out.json"}));
    std::optional<Sprite> img = image_import("cli_out.png");
    ASSERT_TRUE(img.has_value());
    if (img) {
        ASSERT_EQ_INT(8, img->width());
    }
    ASSERT_TRUE(slurp("cli_out.json").find("\"image\": \"cli_out.png\"") != std::string::npos);
    std::remove("cli_in.pxa");
    std::remove("cli_out.png");
    std::remove("cli_out.json");
}

TEST(options_change_the_layout)
{
    make_input("cli_in2.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_in2.pxa", "--sheet", "cli_o2.png", "--columns", "1",
                          "--scale", "2"}));
    std::optional<Sprite> img = image_import("cli_o2.png");
    ASSERT_TRUE(img.has_value());
    if (img) {
        ASSERT_EQ_INT(8, img->width());
        ASSERT_EQ_INT(8, img->height());
    }
    std::remove("cli_in2.pxa");
    std::remove("cli_o2.png");
}

TEST(the_packed_layout_trims_and_lists_the_trims_in_the_json)
{
    make_input("cli_in5.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_in5.pxa", "--sheet", "cli_o5.png", "--json", "cli_o5.json",
                          "--layout", "packed"}));
    ASSERT_TRUE(slurp("cli_o5.json").find("\"spriteSourceSize\"") != std::string::npos);
    ASSERT_EQ_INT(2, run({"export", "cli_in5.pxa", "--sheet", "cli_o5.png", "--layout", "pack"}));
    std::remove("cli_in5.pxa");
    std::remove("cli_o5.png");
    std::remove("cli_o5.json");
}

TEST(a_single_frame_png_export_works_too)
{
    make_input("cli_in3.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_in3.pxa", "--image", "cli_o3.png", "--frame", "1"}));
    ASSERT_TRUE(image_import("cli_o3.png").has_value());
    std::remove("cli_in3.pxa");
    std::remove("cli_o3.png");
}

TEST(a_gif_honors_the_frame_range_and_an_image_refuses_one)
{
    make_input("cli_in3.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_in3.pxa", "--gif", "cli_o3.gif", "--first", "1"}));
    ASSERT_EQ_INT(1, run({"export", "cli_in3.pxa", "--gif", "cli_o3.gif", "--first", "5"}));
    ASSERT_EQ_INT(2, run({"export", "cli_in3.pxa", "--image", "cli_o3.png", "--first", "1"}));
    ASSERT_EQ_INT(0, run({"export", "cli_in3.pxa", "--image", "cli_o3.png", "--frame", "1"}));
    std::remove("cli_in3.pxa");
    std::remove("cli_o3.gif");
    std::remove("cli_o3.png");
}

TEST(a_tilemap_export_writes_the_map_and_its_tileset_images)
{
    Sprite s(4, 4);
    Cel cel(4, 2);
    cel.set(0, 0, Rgba{255, 0, 0, 255});
    TileMap map = tilemap_blank("level", 0, 2, 2);
    map.cells[0] = 1;
    s.setTileData({*tileset_from_cel(cel, Rect{0, 0, 4, 2}, 2, 2, "tiles")}, {map});
    pxa_save(s, "cli_tiles.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_tiles.pxa", "--tilemap", "cli_map.tmj"}));
    ASSERT_TRUE(slurp("cli_map.tmj").find("\"image\": \"cli_map_tiles.png\"") != std::string::npos);
    ASSERT_TRUE(image_import("cli_map_tiles.png").has_value());
    /* a file without a tilemap, or a tilemap together with another output, is not accepted */
    make_input("cli_plain.pxa");
    ASSERT_EQ_INT(1, run({"export", "cli_plain.pxa", "--tilemap", "cli_none.tmj"}));
    ASSERT_EQ_INT(2, run({"export", "cli_tiles.pxa", "--tilemap", "a.tmj", "--gif", "a.gif"}));
    std::remove("cli_tiles.pxa");
    std::remove("cli_map.tmj");
    std::remove("cli_map_tiles.png");
    std::remove("cli_plain.pxa");
}

TEST(bad_input_fails_with_a_status)
{
    ASSERT_EQ_INT(2, run({"export"}));
    ASSERT_EQ_INT(2, run({"export", "x.pxa"})); /* no output */
    ASSERT_EQ_INT(2, run({"export", "x.pxa", "--sheet", "a.png", "--bogus"}));
    ASSERT_EQ_INT(2, run({"export", "x.pxa", "--sheet", "a.png", "--scale", "zero"}));
    ASSERT_EQ_INT(1, run({"export", "no_such.pxa", "--sheet", "a.png"})); /* unreadable */
    make_input("cli_in4.pxa");
    ASSERT_EQ_INT(1, run({"export", "cli_in4.pxa", "--sheet", "a.unknown"}));
    ASSERT_EQ_INT(1, run({"export", "cli_in4.pxa", "--image", "a.png", "--frame", "9"}));
    std::remove("cli_in4.pxa");
}

int main()
{
    RUN(exports_a_sheet_and_its_json);
    RUN(options_change_the_layout);
    RUN(the_packed_layout_trims_and_lists_the_trims_in_the_json);
    RUN(a_single_frame_png_export_works_too);
    RUN(a_gif_honors_the_frame_range_and_an_image_refuses_one);
    RUN(a_tilemap_export_writes_the_map_and_its_tileset_images);
    RUN(bad_input_fails_with_a_status);
    return TEST_SUMMARY();
}
