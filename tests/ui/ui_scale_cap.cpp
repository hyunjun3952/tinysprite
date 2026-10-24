/* A UI scale the display cannot hold is capped for this run only: the scale the user chose stays in
 * the settings, so it is back on a bigger display. */
#include <filesystem>
#include <fstream>
#include <string>

#include "app.hpp"
#include "minitest.hpp"
#include "settings.hpp"
#include "theme.hpp"

struct AppTestAccess {
    App app;
    void shutdown()
    {
        app.shutdown();
    }
};

TEST(a_capped_scale_is_not_written_over_the_one_the_user_chose)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "tinysprite_scale_cap_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "settings.ini") << "ui.scale=3\n";

    float written = 0.0f;
    {
        AppTestAccess access;
        App &app = access.app;
        AppOptions options;
        options.width = 16;
        options.height = 16;
        options.configDir = dir.string();
        ASSERT_TRUE(app.init(options));
        ASSERT_TRUE(theme::scale() < 3.0f); /* the dummy display is smaller than 3 x 640 x 420 */
        access.shutdown();
    }
    SettingsMap settings = settings_load(dir.string());
    ASSERT_TRUE(
        settings_get_float(settings, "ui.scale", theme::SCALE_MIN, theme::SCALE_MAX, written));
    ASSERT_TRUE(written == 3.0f);
    fs::remove_all(dir);
}

int main()
{
    RUN(a_capped_scale_is_not_written_over_the_one_the_user_chose);
    return TEST_SUMMARY();
}
