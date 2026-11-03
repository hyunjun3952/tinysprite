#ifndef TINYSPRITE_UI_SESSION_HPP
#define TINYSPRITE_UI_SESSION_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "settings.hpp"

/* The most tabs a session remembers; the ones past it are left out when it is stored and read. */
inline constexpr size_t SESSION_MAX_TABS = 32;

/* The range the first pane's share of a split view may take (the split sash keeps to it too). */
inline constexpr float SPLIT_RATIO_MIN = 0.15f;
inline constexpr float SPLIT_RATIO_MAX = 0.85f;

/*
 * The open tabs and the split view as they were when the program closed, kept in settings.ini so
 * the next run opens the same files. Only documents saved to a file count: a tab that was never
 * saved has no path to open, and its unsaved changes are the autosave recovery's job.
 */
struct Session {
    std::vector<std::string> paths; /* the file of each remembered tab, in tab order */
    float ratio = 0.5f;             /* the first pane's share of the canvas area, when split */
    int active = 0;                 /* the tab that was in front (an index into paths) */
    int firstPane = -1;             /* when split: the tab shown in the first pane */
    int secondPane = -1;            /* when split: the tab shown in the second pane */
    int activePane = 0;             /* when split: the pane that held the active tab, 0 or 1 */
    bool split = false;             /* two panes showed documents */
    bool sideBySide = true;         /* when split: left | right, else top / bottom */
};

/*
 * Writes `session` into `settings` under the "session." keys, first removing the keys of an
 * earlier session so tabs closed since do not come back. Other keys are left alone. A session
 * with no tabs leaves no keys, and a split is written only when both panes name different tabs.
 */
void session_store(SettingsMap &settings, const Session &session);

/*
 * Reads the session written by session_store. A missing or damaged value falls back to its
 * default instead of failing: the tab list ends at the first missing entry, an active tab out of
 * range becomes the first, and a split that names a tab that is not there is dropped.
 */
Session session_load(const SettingsMap &settings);

#endif
