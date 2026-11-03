#include "session.hpp"

#include <algorithm>

namespace
{

const char *const PREFIX = "session.";
const char *const TAB_PREFIX = "session.tab.";

/* Removes every key that belongs to a session. */
void erase_session_keys(SettingsMap &settings)
{
    const std::string prefix = PREFIX;
    auto it = settings.lower_bound(prefix);
    while (it != settings.end() && it->first.compare(0, prefix.size(), prefix) == 0) {
        it = settings.erase(it);
    }
}

/* Whether the split of `session` names two different tabs that exist. */
bool split_is_valid(const Session &session)
{
    const int count = static_cast<int>(session.paths.size());
    return session.split && session.firstPane >= 0 && session.firstPane < count &&
           session.secondPane >= 0 && session.secondPane < count &&
           session.firstPane != session.secondPane;
}

void store_split(SettingsMap &settings, const Session &session)
{
    settings_set_bool(settings, "session.split", true);
    settings_set_bool(settings, "session.split.side", session.sideBySide);
    settings_set_int(settings, "session.split.first", session.firstPane);
    settings_set_int(settings, "session.split.second", session.secondPane);
    settings_set_int(settings, "session.split.active", session.activePane);
    settings_set_float(settings, "session.split.ratio", session.ratio);
}

/* The tabs, from "session.tab.0" up to the first entry that is missing; empty ones are skipped. */
std::vector<std::string> load_tabs(const SettingsMap &settings)
{
    std::vector<std::string> paths;
    for (size_t i = 0; i < SESSION_MAX_TABS; i++) {
        auto found = settings.find(TAB_PREFIX + std::to_string(i));
        if (found == settings.end()) {
            break;
        }
        if (!found->second.empty()) {
            paths.push_back(found->second);
        }
    }
    return paths;
}

void load_split(const SettingsMap &settings, Session &session)
{
    bool on = false;
    if (!settings_get_bool(settings, "session.split", on) || !on) {
        return;
    }
    Session split = session;
    const int last = static_cast<int>(session.paths.size()) - 1;
    split.split = true;
    settings_get_bool(settings, "session.split.side", split.sideBySide);
    settings_get_int(settings, "session.split.first", 0, last, split.firstPane);
    settings_get_int(settings, "session.split.second", 0, last, split.secondPane);
    settings_get_int(settings, "session.split.active", 0, 1, split.activePane);
    settings_get_float(settings, "session.split.ratio", SPLIT_RATIO_MIN, SPLIT_RATIO_MAX,
                       split.ratio);
    if (split_is_valid(split)) {
        session = split;
    }
}

} // namespace

void session_store(SettingsMap &settings, const Session &session)
{
    erase_session_keys(settings);
    const size_t count = std::min(session.paths.size(), SESSION_MAX_TABS);
    if (count == 0) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        settings[TAB_PREFIX + std::to_string(i)] = session.paths[i];
    }
    settings_set_int(settings, "session.active", session.active);
    Session kept = session;
    kept.paths.resize(count);
    if (split_is_valid(kept)) {
        store_split(settings, kept);
    }
}

Session session_load(const SettingsMap &settings)
{
    Session session;
    session.paths = load_tabs(settings);
    if (session.paths.empty()) {
        return session;
    }
    settings_get_int(settings, "session.active", 0, static_cast<int>(session.paths.size()) - 1,
                     session.active);
    load_split(settings, session);
    return session;
}
