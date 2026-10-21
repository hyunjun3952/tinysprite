#include "settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#include "tinysprite/io.h"

namespace
{

std::string trim(const std::string &text)
{
    const char *space = " \t\r\n";
    size_t first = text.find_first_not_of(space);
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

const std::string *find(const SettingsMap &settings, const std::string &key)
{
    auto it = settings.find(key);
    return it == settings.end() ? nullptr : &it->second;
}

std::string env(const char *name)
{
    const char *value = std::getenv(name);
    return value ? value : "";
}

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
    namespace fs = std::filesystem;
    if (std::string dir = env("TINYSPRITE_CONFIG_DIR"); !dir.empty()) {
        return dir;
    }
#ifdef _WIN32
    if (std::string base = env("APPDATA"); !base.empty()) {
        return (fs::path(base) / "tinysprite").string();
    }
#endif
    if (std::string base = env("XDG_CONFIG_HOME"); !base.empty()) {
        return (fs::path(base) / "tinysprite").string();
    }
    if (std::string home = env("HOME"); !home.empty()) {
        return (fs::path(home) / ".config" / "tinysprite").string();
    }
    return "";
}

SettingsMap settings_load(const std::string &dir)
{
    std::ifstream file((std::filesystem::path(dir) / "settings.ini").string(), std::ios::binary);
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
    std::filesystem::create_directories(dir, error);
    return !error && file_write_atomic((std::filesystem::path(dir) / "settings.ini").string(),
                                       settings_format(settings));
}
