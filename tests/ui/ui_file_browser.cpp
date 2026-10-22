/* The folder listing behind the file dialog: plain functions of the disk, no ImGui frame needed. */
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "file_browser.hpp"
#include "minitest.hpp"

namespace fs = std::filesystem;

/* A scratch folder with a few entries, removed again when it goes out of scope. */
struct Scratch {
    fs::path root = fs::temp_directory_path() / "tinysprite_file_browser_test";

    Scratch()
    {
        fs::remove_all(root);
        fs::create_directories(root / "Zebra");
        fs::create_directories(root / "alpha");
        fs::create_directories(root / ".hidden_dir");
        touch("b.png");
        touch("A.PXA");
        touch("notes.txt");
        touch(".secret.png");
    }
    Scratch(const Scratch &) = delete;
    Scratch &operator=(const Scratch &) = delete;
    Scratch(Scratch &&) = delete;
    Scratch &operator=(Scratch &&) = delete;
    ~Scratch()
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    void touch(const char *name) const
    {
        std::ofstream(root / name) << "x";
    }
};

static std::string names(const std::vector<DirEntry> &entries)
{
    std::string out;
    for (const DirEntry &entry : entries) {
        out += entry.name + (entry.directory ? "/" : "") + " ";
    }
    return out;
}

TEST(folders_come_first_then_the_matching_files_each_sorted_ignoring_case)
{
    Scratch scratch;
    std::vector<DirEntry> entries = list_directory(scratch.root.string(), ".png .pxa");
    ASSERT_TRUE(names(entries) == "alpha/ Zebra/ A.PXA b.png ");
}

TEST(an_empty_filter_lists_every_file_but_never_hidden_ones)
{
    Scratch scratch;
    std::vector<DirEntry> entries = list_directory(scratch.root.string(), "");
    ASSERT_TRUE(names(entries) == "alpha/ Zebra/ A.PXA b.png notes.txt ");
}

TEST(an_unreadable_folder_lists_nothing)
{
    ASSERT_TRUE(list_directory("/nonexistent-tinysprite-dir", "").empty());
}

TEST(the_parent_of_a_folder)
{
    ASSERT_TRUE(parent_directory("/a/b") == "/a");
    ASSERT_TRUE(parent_directory("/a/b/") == "/a");
    ASSERT_TRUE(parent_directory("/a") == "/");
    ASSERT_TRUE(parent_directory("/") == "/");
    ASSERT_TRUE(parent_directory("a/b") == "a");
    ASSERT_TRUE(parent_directory("a").empty());
    ASSERT_TRUE(parent_directory("") == "..");
}

TEST(the_browsed_folder_is_the_typed_ones_or_a_fallback)
{
    Scratch scratch;
    std::string root = scratch.root.string();
    ASSERT_TRUE(browse_directory(root + "/sprite.pxa", "/elsewhere") == root);
    ASSERT_TRUE(browse_directory(root + "/", "/elsewhere") == root);
    ASSERT_TRUE(browse_directory(root, "/elsewhere") == root); /* a folder typed without a slash */
    ASSERT_TRUE(browse_directory("nothing-here/x.pxa", root) ==
                root); /* not a folder: the fallback */
    ASSERT_TRUE(browse_directory("", "") == ".");
}

TEST(a_search_keeps_the_entries_that_contain_the_text_ignoring_case)
{
    std::vector<DirEntry> all = {
        {"Sprites", true}, {"music", true}, {"hero.PXA", false}, {"tileset.png", false}};
    ASSERT_TRUE(names(filter_entries(all, "")) == "Sprites/ music/ hero.PXA tileset.png ");
    ASSERT_TRUE(names(filter_entries(all, "SPR")) == "Sprites/ ");
    ASSERT_TRUE(names(filter_entries(all, "e")) == "Sprites/ hero.PXA tileset.png ");
    ASSERT_TRUE(filter_entries(all, "zzz").empty());
}

static void set_home(const char *path)
{
#ifdef _WIN32
    _putenv_s("HOME", path);
#else
    setenv("HOME", path, 1);
#endif
}

TEST(a_leading_tilde_means_the_home_folder)
{
    set_home("/home/someone");
    ASSERT_TRUE(expand_home("~") == "/home/someone");
    ASSERT_TRUE(expand_home("~/project/a.pxa") == "/home/someone/project/a.pxa");
    ASSERT_TRUE(expand_home("/x/~/y") == "/x/~/y");
    ASSERT_TRUE(expand_home("~other/a") == "~other/a"); /* another user's home is not handled */
    ASSERT_TRUE(expand_home("").empty());
}

TEST(a_search_finds_matches_in_the_folders_below_with_their_relative_paths)
{
    Scratch scratch;
    fs::create_directories(scratch.root / "alpha" / "deep");
    fs::create_directories(scratch.root / "Zebra" / "Hero_dir");
    std::ofstream(scratch.root / "alpha" / "deep" / "hero.pxa") << "x";
    std::ofstream(scratch.root / "hero.png") << "x";
    std::ofstream(scratch.root / "alpha" / "other.pxa") << "x";
    std::vector<DirEntry> found =
        search_directory(scratch.root.string(), ".png .pxa", "HERO", 5, 100);
    ASSERT_TRUE(names(found) == "alpha/deep/hero.pxa Zebra/Hero_dir/ hero.png ");
}

TEST(a_search_stops_at_the_depth_and_the_result_limits)
{
    Scratch scratch;
    fs::create_directories(scratch.root / "alpha" / "deep");
    std::ofstream(scratch.root / "alpha" / "deep" / "hero.pxa") << "x";
    std::ofstream(scratch.root / "hero.png") << "x";
    ASSERT_TRUE(names(search_directory(scratch.root.string(), "", "hero", 1, 100)) == "hero.png ");
    ASSERT_EQ_INT(
        1, static_cast<int>(search_directory(scratch.root.string(), "", "hero", 5, 1).size()));
    ASSERT_TRUE(
        search_directory(scratch.root.string(), "", "", 5, 100).empty()); /* no query: nothing */
}

int main()
{
    RUN(folders_come_first_then_the_matching_files_each_sorted_ignoring_case);
    RUN(an_empty_filter_lists_every_file_but_never_hidden_ones);
    RUN(an_unreadable_folder_lists_nothing);
    RUN(the_parent_of_a_folder);
    RUN(the_browsed_folder_is_the_typed_ones_or_a_fallback);
    RUN(a_search_keeps_the_entries_that_contain_the_text_ignoring_case);
    RUN(a_leading_tilde_means_the_home_folder);
    RUN(a_search_finds_matches_in_the_folders_below_with_their_relative_paths);
    RUN(a_search_stops_at_the_depth_and_the_result_limits);
    return TEST_SUMMARY();
}
