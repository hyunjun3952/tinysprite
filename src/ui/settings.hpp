#ifndef TINYSPRITE_UI_SETTINGS_HPP
#define TINYSPRITE_UI_SETTINGS_HPP

#include <map>
#include <string>
#include <vector>

/* The settings file: one "key=value" per line (blank lines and lines starting with '#' are
 * ignored), kept as a sorted map so the file is stable between runs. The panels and the editor read
 * and write their own keys (Panels::store / load, Editor::store / load); a missing or unreadable
 * key leaves the default, so an old or hand-edited file never breaks the start. */
using SettingsMap = std::map<std::string, std::string>;

std::string settings_format(const SettingsMap &settings);
SettingsMap settings_parse(const std::string &text);

/* Typed reads: return false and leave `out` alone when the key is missing or does not parse (or is
 * outside [min, max]). */
bool settings_get_bool(const SettingsMap &settings, const std::string &key, bool &out);
bool settings_get_int(const SettingsMap &settings, const std::string &key, int min, int max,
                      int &out);
bool settings_get_float(const SettingsMap &settings, const std::string &key, float min, float max,
                        float &out);
void settings_set_bool(SettingsMap &settings, const std::string &key, bool value);
void settings_set_int(SettingsMap &settings, const std::string &key, int value);
void settings_set_float(SettingsMap &settings, const std::string &key, float value);

/* The "recent files" list: puts `path` first, drops an earlier copy of it and keeps at most `limit`
 * entries. */
void recent_add(std::vector<std::string> &recent, const std::string &path, size_t limit);

/* Stored as recent.0, recent.1, ... (newest first); loading skips gaps and empty entries. */
void settings_set_recent(SettingsMap &settings, const std::vector<std::string> &recent);
std::vector<std::string> settings_get_recent(const SettingsMap &settings, size_t limit);

/* Where tinysprite keeps its per-user files (settings, autosave): $TINYSPRITE_CONFIG_DIR if set,
 * else $XDG_CONFIG_HOME/tinysprite, %APPDATA%\tinysprite on Windows or ~/.config/tinysprite.
 * Empty when no home directory can be found, in which case nothing is persisted. */
std::string config_dir();

/* Reads / writes <dir>/settings.ini. Loading a missing file gives an empty map; saving creates the
 * directory and returns false if the file cannot be written. */
SettingsMap settings_load(const std::string &dir);
bool settings_save(const std::string &dir, const SettingsMap &settings);

#endif
