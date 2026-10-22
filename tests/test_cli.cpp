#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "minitest.h"
#include "tinysprite/cli.h"
#include "tinysprite/io.h"

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

TEST(a_single_frame_png_export_works_too)
{
    make_input("cli_in3.pxa");
    ASSERT_EQ_INT(0, run({"export", "cli_in3.pxa", "--image", "cli_o3.png", "--frame", "1"}));
    ASSERT_TRUE(image_import("cli_o3.png").has_value());
    std::remove("cli_in3.pxa");
    std::remove("cli_o3.png");
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
    RUN(a_single_frame_png_export_works_too);
    RUN(bad_input_fails_with_a_status);
    return TEST_SUMMARY();
}
