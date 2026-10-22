/* The settings file, the panel layout stored in it and the autosave recovery copy. No window is
 * needed, so this runs without SDL's dummy driver. */
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "minitest.hpp"
#include "panels.hpp"
#include "recovery.hpp"
#include "settings.hpp"
#include "tinysprite/io.hpp"

namespace fs = std::filesystem;

/* A fresh scratch directory under the current directory. */
static std::string scratch(const char *name)
{
    fs::remove_all(name);
    fs::create_directories(name);
    return name;
}

TEST(format_and_parse_round_trip)
{
    SettingsMap in = {{"a", "1"}, {"b.c", "hello world"}};
    SettingsMap out = settings_parse(settings_format(in));
    ASSERT_TRUE(out == in);
}

TEST(parse_ignores_comments_blank_lines_and_junk)
{
    SettingsMap out =
        settings_parse("# comment\n\n  key = value  \nno equals here\n=novalue\nk2=\n");
    ASSERT_EQ_INT(2, out.size());
    ASSERT_TRUE(out["key"] == "value");
    ASSERT_TRUE(out["k2"].empty());
}

TEST(typed_reads_reject_bad_or_out_of_range_values)
{
    SettingsMap s = {{"b", "1"},     {"badb", "yes"}, {"i", "7"},     {"badi", "7x"},
                     {"big", "999"}, {"f", "2.5"},    {"nan", "nan"}, {"hugef", "1e9"}};
    bool b = false;
    ASSERT_TRUE(settings_get_bool(s, "b", b) && b);
    ASSERT_TRUE(!settings_get_bool(s, "badb", b));
    ASSERT_TRUE(!settings_get_bool(s, "missing", b));
    int i = 0;
    ASSERT_TRUE(settings_get_int(s, "i", 0, 10, i) && i == 7);
    ASSERT_TRUE(!settings_get_int(s, "badi", 0, 10, i));
    ASSERT_TRUE(!settings_get_int(s, "big", 0, 10, i));
    ASSERT_EQ_INT(7, i);
    float f = 0.0f;
    ASSERT_TRUE(settings_get_float(s, "f", 0.0f, 10.0f, f) && f == 2.5f);
    ASSERT_TRUE(!settings_get_float(s, "nan", 0.0f, 10.0f, f));
    ASSERT_TRUE(!settings_get_float(s, "hugef", 0.0f, 10.0f, f));
}

TEST(save_and_load_use_the_directory)
{
    std::string dir = scratch("test_settings_dir") + "/nested";
    ASSERT_TRUE(settings_load(dir).empty()); /* missing file, missing directory */
    SettingsMap in = {{"x", "y"}};
    ASSERT_TRUE(settings_save(dir, in));
    ASSERT_TRUE(settings_load(dir) == in);
    ASSERT_TRUE(!settings_save("", in));
    fs::remove_all("test_settings_dir");
}

TEST(config_dir_honors_the_override)
{
#ifdef _WIN32
    _putenv_s("TINYSPRITE_CONFIG_DIR", "somewhere");
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe): the test is single-threaded
    setenv("TINYSPRITE_CONFIG_DIR", "somewhere", 1);
#endif
    ASSERT_TRUE(config_dir() == "somewhere");
}

TEST(panel_layout_survives_a_round_trip)
{
    Panels a;
    a.setVisible(PanelId::Layers, false);
    a.setSectionVisible(PanelId::Tools, sections::SYMMETRY, true);
    a.setSectionVisible(PanelId::Tools, sections::RECENT, false);
    SettingsMap s;
    a.store(s);

    Panels b;
    b.load(s);
    ASSERT_TRUE(!b.visible(PanelId::Layers));
    ASSERT_TRUE(b.visible(PanelId::Tools));
    ASSERT_TRUE(b.sectionVisible(PanelId::Tools, sections::SYMMETRY));
    ASSERT_TRUE(!b.sectionVisible(PanelId::Tools, sections::RECENT));
    ASSERT_TRUE(b.sectionVisible(PanelId::Tools, sections::COLOR));
}

TEST(panel_load_ignores_bad_entries)
{
    SettingsMap s = {{"panel.tools.visible", "maybe"},
                     {"panel.tools.side", "9"},
                     {"panel.tools.width", "5"},
                     {"panel.layers.side", "2"}};
    Panels p;
    p.load(s);
    ASSERT_TRUE(p.visible(PanelId::Tools));
    ASSERT_TRUE(!p.floating(PanelId::Tools));
    ASSERT_TRUE(p.floating(PanelId::Layers));
}

TEST(recovery_round_trips_the_document_and_its_path)
{
    std::string dir = scratch("test_recovery_dir");
    ASSERT_TRUE(!recovery_load(dir).has_value());
    Sprite sprite(6, 4);
    sprite.layer(0)->cel(0)->set(2, 1, Rgba{9, 8, 7, 255});
    sprite.addLayer("Glow").setBlendMode(BlendMode::Add);
    ASSERT_TRUE(recovery_save(dir, sprite, "/work/hero.pxa"));
    std::optional<Recovered> back = recovery_load(dir);
    ASSERT_TRUE(back.has_value());
    if (!back) {
        return;
    }
    ASSERT_EQ_INT(6, back->sprite.width());
    ASSERT_TRUE(back->sprite.layer(0)->cel(0)->get(2, 1) == (Rgba{9, 8, 7, 255}));
    ASSERT_TRUE(back->path == "/work/hero.pxa");
    ASSERT_EQ_INT(static_cast<int>(BlendMode::Add),
                  static_cast<int>(back->sprite.layer(1)->blendMode()));
    recovery_clear(dir);
    ASSERT_TRUE(!recovery_load(dir).has_value());
    recovery_clear(dir); /* clearing nothing is fine */
    ASSERT_TRUE(!recovery_save("", sprite, ""));
    fs::remove_all("test_recovery_dir");
}

TEST(a_corrupt_recovery_copy_is_ignored)
{
    std::string dir = scratch("test_recovery_bad");
    std::ofstream(dir + "/recovery.pxa") << "not a sprite";
    ASSERT_TRUE(!recovery_load(dir).has_value());
    fs::remove_all("test_recovery_bad");
}

TEST(recent_list_puts_the_newest_first_without_repeats)
{
    std::vector<std::string> r;
    recent_add(r, "a", 3);
    recent_add(r, "b", 3);
    recent_add(r, "c", 3);
    recent_add(r, "a", 3); /* moves up */
    ASSERT_EQ_INT(3, r.size());
    ASSERT_TRUE(r[0] == "a" && r[1] == "c" && r[2] == "b");
    recent_add(r, "d", 3); /* the oldest drops off */
    ASSERT_TRUE(r.size() == 3 && r[0] == "d" && r[2] == "c");
    recent_add(r, "", 3);
    ASSERT_EQ_INT(3, r.size());
}

TEST(recent_list_round_trips_through_the_settings_in_order)
{
    std::vector<std::string> r = {"/x/one.pxa", "/x/two.pxa", "/x/three.pxa"};
    SettingsMap s = {{"recent.9", "stale"}, {"other", "kept"}};
    settings_set_recent(s, r);
    ASSERT_TRUE(s.count("recent.9") == 0);
    ASSERT_TRUE(s["other"] == "kept");
    ASSERT_TRUE(settings_get_recent(s, 10) == r);
    s["recent.1"] = "";
    std::vector<std::string> gaps = settings_get_recent(s, 10);
    ASSERT_TRUE(gaps.size() == 2 && gaps[0] == "/x/one.pxa" && gaps[1] == "/x/three.pxa");
    ASSERT_EQ_INT(1, settings_get_recent(s, 1).size());
}

int main()
{
    RUN(format_and_parse_round_trip);
    RUN(parse_ignores_comments_blank_lines_and_junk);
    RUN(typed_reads_reject_bad_or_out_of_range_values);
    RUN(save_and_load_use_the_directory);
    RUN(config_dir_honors_the_override);
    RUN(panel_layout_survives_a_round_trip);
    RUN(panel_load_ignores_bad_entries);
    RUN(recovery_round_trips_the_document_and_its_path);
    RUN(a_corrupt_recovery_copy_is_ignored);
    RUN(recent_list_puts_the_newest_first_without_repeats);
    RUN(recent_list_round_trips_through_the_settings_in_order);
    return TEST_SUMMARY();
}
