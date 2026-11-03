/* The tabs and the split view kept between runs: what is written to the settings and how a damaged
 * or hand-edited file is read. */
#include <string>

#include "minitest.hpp"
#include "session.hpp"

static Session two_tabs_split()
{
    Session s;
    s.paths = {"/art/a.pxa", "/art/b.pxa", "/art/c.pxa"};
    s.active = 2;
    s.split = true;
    s.sideBySide = false;
    s.firstPane = 2;
    s.secondPane = 0;
    s.activePane = 1;
    s.ratio = 0.3f;
    return s;
}

TEST(a_session_survives_a_round_trip_through_the_settings_text)
{
    SettingsMap settings;
    session_store(settings, two_tabs_split());
    const Session back = session_load(settings_parse(settings_format(settings)));
    ASSERT_EQ_INT(3, static_cast<int>(back.paths.size()));
    ASSERT_TRUE(back.paths[1] == "/art/b.pxa");
    ASSERT_EQ_INT(2, back.active);
    ASSERT_TRUE(back.split);
    ASSERT_TRUE(!back.sideBySide);
    ASSERT_EQ_INT(2, back.firstPane);
    ASSERT_EQ_INT(0, back.secondPane);
    ASSERT_EQ_INT(1, back.activePane);
    ASSERT_TRUE(back.ratio > 0.29f && back.ratio < 0.31f);
}

TEST(a_session_without_a_split_stores_no_split_keys)
{
    Session s;
    s.paths = {"a.pxa"};
    SettingsMap settings;
    session_store(settings, s);
    ASSERT_EQ_INT(0, static_cast<int>(settings.count("session.split")));
    const Session back = session_load(settings);
    ASSERT_TRUE(!back.split);
    ASSERT_EQ_INT(1, static_cast<int>(back.paths.size()));
}

TEST(storing_replaces_the_tabs_of_the_last_run_and_keeps_other_keys)
{
    SettingsMap settings;
    settings["ui.scale"] = "1.5";
    session_store(settings, two_tabs_split());
    Session fewer;
    fewer.paths = {"/art/z.pxa"};
    session_store(settings, fewer);
    const Session back = session_load(settings);
    ASSERT_EQ_INT(1, static_cast<int>(back.paths.size()));
    ASSERT_TRUE(!back.split);
    ASSERT_TRUE(settings["ui.scale"] == "1.5");
    ASSERT_EQ_INT(0, static_cast<int>(settings.count("session.tab.1")));
}

TEST(storing_no_tabs_removes_the_old_session)
{
    SettingsMap settings;
    session_store(settings, two_tabs_split());
    session_store(settings, Session{});
    ASSERT_TRUE(session_load(settings).paths.empty());
    ASSERT_EQ_INT(0, static_cast<int>(settings.count("session.active")));
}

TEST(an_empty_settings_file_gives_an_empty_session)
{
    const Session s = session_load(SettingsMap{});
    ASSERT_TRUE(s.paths.empty());
    ASSERT_TRUE(!s.split);
    ASSERT_EQ_INT(0, s.active);
}

TEST(an_active_tab_past_the_end_falls_back_to_the_first)
{
    SettingsMap settings;
    session_store(settings, two_tabs_split());
    settings["session.active"] = "9";
    ASSERT_EQ_INT(0, session_load(settings).active);
    settings["session.active"] = "-1";
    ASSERT_EQ_INT(0, session_load(settings).active);
    settings["session.active"] = "soon";
    ASSERT_EQ_INT(0, session_load(settings).active);
}

TEST(a_split_naming_a_tab_that_is_not_there_is_dropped)
{
    SettingsMap settings;
    session_store(settings, two_tabs_split());
    settings["session.split.second"] = "7";
    ASSERT_TRUE(!session_load(settings).split);
    settings["session.split.second"] = "2"; /* the same document in both panes */
    ASSERT_TRUE(!session_load(settings).split);
    settings["session.split.second"] = "1";
    ASSERT_TRUE(session_load(settings).split);
}

TEST(a_ratio_outside_the_allowed_range_is_ignored)
{
    SettingsMap settings;
    session_store(settings, two_tabs_split());
    settings["session.split.ratio"] = "0.99";
    ASSERT_TRUE(session_load(settings).ratio == 0.5f);
    settings["session.split.ratio"] = "x";
    ASSERT_TRUE(session_load(settings).ratio == 0.5f);
}

TEST(reading_stops_at_the_first_missing_tab_and_skips_empty_paths)
{
    SettingsMap settings;
    settings["session.tab.0"] = "a.pxa";
    settings["session.tab.1"] = "b.pxa";
    settings["session.tab.3"] = "d.pxa"; /* after a gap */
    ASSERT_EQ_INT(2, static_cast<int>(session_load(settings).paths.size()));
    settings["session.tab.1"] = "";
    ASSERT_EQ_INT(1, static_cast<int>(session_load(settings).paths.size()));
}

TEST(only_the_most_tabs_the_session_keeps_are_stored_and_read)
{
    Session s;
    for (size_t i = 0; i < SESSION_MAX_TABS + 5; i++) {
        s.paths.push_back("f" + std::to_string(i) + ".pxa");
    }
    SettingsMap settings;
    session_store(settings, s);
    ASSERT_EQ_INT(
        0, static_cast<int>(settings.count("session.tab." + std::to_string(SESSION_MAX_TABS))));
    ASSERT_EQ_INT(static_cast<int>(SESSION_MAX_TABS),
                  static_cast<int>(session_load(settings).paths.size()));
}

TEST(a_split_that_names_one_tab_twice_or_none_is_not_stored)
{
    Session s = two_tabs_split();
    s.secondPane = s.firstPane;
    SettingsMap settings;
    session_store(settings, s);
    ASSERT_EQ_INT(0, static_cast<int>(settings.count("session.split")));
    s = two_tabs_split();
    s.secondPane = 3; /* only tabs 0 to 2 exist */
    session_store(settings, s);
    ASSERT_EQ_INT(0, static_cast<int>(settings.count("session.split")));
}

int main()
{
    RUN(a_session_survives_a_round_trip_through_the_settings_text);
    RUN(a_session_without_a_split_stores_no_split_keys);
    RUN(storing_replaces_the_tabs_of_the_last_run_and_keeps_other_keys);
    RUN(storing_no_tabs_removes_the_old_session);
    RUN(an_empty_settings_file_gives_an_empty_session);
    RUN(an_active_tab_past_the_end_falls_back_to_the_first);
    RUN(a_split_naming_a_tab_that_is_not_there_is_dropped);
    RUN(a_ratio_outside_the_allowed_range_is_ignored);
    RUN(reading_stops_at_the_first_missing_tab_and_skips_empty_paths);
    RUN(only_the_most_tabs_the_session_keeps_are_stored_and_read);
    RUN(a_split_that_names_one_tab_twice_or_none_is_not_stored);
    return TEST_SUMMARY();
}
