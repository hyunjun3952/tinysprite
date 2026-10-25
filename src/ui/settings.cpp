#include "settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#include "text_util.hpp"
#include "tinysprite/io.hpp"

namespace
{

const std::string *find(const SettingsMap &settings, const std::string &key)
{
    auto it = settings.find(key);
    return it == settings.end() ? nullptr : &it->second;
}

/* The variable as UTF-8, which is what every path in the program is (on Windows the narrow
 * environment is in the ANSI code page, so a non-ASCII user name would come out garbled). */
std::string env(const char *name)
{
#ifdef _WIN32
    std::wstring wide(name, name + std::strlen(name));
    const wchar_t *value = _wgetenv(wide.c_str());
    return value != nullptr ? path_to_utf8(std::filesystem::path(value)) : "";
#else
    const char *value = std::getenv(name);
    return value != nullptr ? value : "";
#endif
}

constexpr int MIN_WIDTH = 640;
constexpr int MIN_HEIGHT = 420;

} // namespace

std::string settings_format(const SettingsMap &settings)
{
    std::string out;
    for (const auto &[key, value] : settings) {
        out += key;
        out += "=";
        out += value;
        out += "\n";
    }
    return out;
}

SettingsMap settings_parse(const std::string &text)
{
    SettingsMap settings;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        size_t equals = line.find('=');
        if (line.empty() || line[0] == '#' || equals == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, equals));
        if (!key.empty()) {
            settings[key] = trim(line.substr(equals + 1));
        }
    }
    return settings;
}

bool settings_get_bool(const SettingsMap &settings, const std::string &key, bool &out)
{
    const std::string *value = find(settings, key);
    if (!value || (*value != "0" && *value != "1")) {
        return false;
    }
    out = *value == "1";
    return true;
}

bool settings_get_int(const SettingsMap &settings, const std::string &key, int min, int max,
                      int &out)
{
    const std::string *value = find(settings, key);
    if (!value || value->empty()) {
        return false;
    }
    char *end = nullptr;
    long number = std::strtol(value->c_str(), &end, 10);
    if (*end != '\0' || number < min || number > max) {
        return false;
    }
    out = static_cast<int>(number);
    return true;
}

bool settings_get_float(const SettingsMap &settings, const std::string &key, float min, float max,
                        float &out)
{
    const std::string *value = find(settings, key);
    if (!value || value->empty()) {
        return false;
    }
    char *end = nullptr;
    float number = std::strtof(value->c_str(), &end);
    /* Written so that NaN is rejected too. */
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if (*end != '\0' || !(number >= min && number <= max)) {
        return false;
    }
    out = number;
    return true;
}

void settings_set_bool(SettingsMap &settings, const std::string &key, bool value)
{
    settings[key] = value ? "1" : "0";
}

void settings_set_int(SettingsMap &settings, const std::string &key, int value)
{
    settings[key] = std::to_string(value);
}

void settings_set_float(SettingsMap &settings, const std::string &key, float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
    settings[key] = text;
}

void recent_add(std::vector<std::string> &recent, const std::string &path, size_t limit)
{
    if (path.empty()) {
        return;
    }
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if (recent.size() > limit) {
        recent.resize(limit);
    }
}

void settings_set_recent(SettingsMap &settings, const std::vector<std::string> &recent)
{
    for (auto it = settings.begin(); it != settings.end();) {
        it = it->first.rfind("recent.", 0) == 0 ? settings.erase(it) : std::next(it);
    }
    for (size_t i = 0; i < recent.size(); i++) {
        settings["recent." + std::to_string(i)] = recent[i];
    }
}

std::vector<std::string> settings_get_recent(const SettingsMap &settings, size_t limit)
{
    std::vector<std::string> recent;
    for (size_t i = 0; i < limit; i++) {
        auto it = settings.find("recent." + std::to_string(i));
        if (it != settings.end() && !it->second.empty()) {
            recent_add(recent, it->second, limit);
            std::rotate(recent.begin(), recent.begin() + 1, recent.end()); /* keep file order */
        }
    }
    return recent;
}

std::string config_dir()
{
    if (std::string dir = env("TINYSPRITE_CONFIG_DIR"); !dir.empty()) {
        return dir;
    }
#ifdef _WIN32
    if (std::string base = env("APPDATA"); !base.empty()) {
        return path_to_utf8(path_from_utf8(base) / "tinysprite");
    }
#endif
    if (std::string base = env("XDG_CONFIG_HOME"); !base.empty()) {
        return path_to_utf8(path_from_utf8(base) / "tinysprite");
    }
    if (std::string home = env("HOME"); !home.empty()) {
        return path_to_utf8(path_from_utf8(home) / ".config" / "tinysprite");
    }
    return "";
}

SettingsMap settings_load(const std::string &dir)
{
    std::ifstream file(path_from_utf8(dir) / "settings.ini", std::ios::binary);
    if (dir.empty() || !file) {
        return {};
    }
    return settings_parse(
        std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
}

bool settings_save(const std::string &dir, const SettingsMap &settings)
{
    if (dir.empty()) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(path_from_utf8(dir), error);
    return !error && file_write_atomic(path_to_utf8(path_from_utf8(dir) / "settings.ini"),
                                       settings_format(settings));
}

WindowSize window_fit(WindowSize wanted, WindowSize usable)
{
    if (usable.width > 0) {
        wanted.width = std::min(wanted.width, usable.width - WINDOW_FRAME_MARGIN);
    }
    if (usable.height > 0) {
        wanted.height = std::min(wanted.height, usable.height - WINDOW_FRAME_MARGIN);
    }
    return wanted;
}

WindowSize window_minimum(float scale)
{
    return WindowSize{static_cast<int>(std::lround(MIN_WIDTH * scale)),
                      static_cast<int>(std::lround(MIN_HEIGHT * scale))};
}

float window_max_scale(WindowSize usable)
{
    constexpr float NO_LIMIT = 1000.0f;
    float widthLimit = NO_LIMIT;
    float heightLimit = NO_LIMIT;
    if (usable.width > 0) {
        widthLimit = static_cast<float>(usable.width - WINDOW_FRAME_MARGIN) / MIN_WIDTH;
    }
    if (usable.height > 0) {
        heightLimit = static_cast<float>(usable.height - WINDOW_FRAME_MARGIN) / MIN_HEIGHT;
    }
    return std::max(1.0f, std::min(widthLimit, heightLimit));
}
