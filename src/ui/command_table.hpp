#ifndef TINYSPRITE_UI_COMMAND_TABLE_HPP
#define TINYSPRITE_UI_COMMAND_TABLE_HPP

#include <functional>
#include <string>

#include "imgui.h"

/* The menu commands that have a Ctrl shortcut. Each one's label and keys are written once, in
 * COMMANDS; the menus show them and the shortcut handler matches them, so the two cannot drift.
 * What a command does and when it can run is the table in App::commandAction. */
enum class CommandId {
    New,
    Open,
    Save,
    SaveAs,
    ExportImage,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    SelectAll,
    ZoomIn,
    ZoomOut,
    Fit,
    ToggleTimeline,
    NewLayer,
    DuplicateLayer,
    MergeLayerDown,
    MoveLayerUp,
    MoveLayerDown,
    NewFrame,
    DuplicateFrame,
    CloseTab,
    NextTab,
    PreviousTab,
    Count,
};

inline constexpr int COMMAND_COUNT = static_cast<int>(CommandId::Count);

/* Whether Shift has to be held, has to be up, or does not matter for a key to match. */
enum class ShiftRule { Any, Held, NotHeld };

struct Shortcut {
    ImGuiKey key = ImGuiKey_None; /* with Ctrl held; None marks an unused slot */
    ShiftRule shift = ShiftRule::Any;
};

struct Command {
    const char *label;
    const char *hint; /* the menu's shortcut text; null derives it from the first shortcut */
    CommandId id;
    Shortcut shortcuts[2];
    bool repeat; /* keeps firing while the keys are held */
};

/* In CommandId order. */
extern const Command COMMANDS[COMMAND_COUNT];

inline const Command &command_of(CommandId id)
{
    return COMMANDS[static_cast<int>(id)];
}

/* The shortcut text for a menu item, e.g. "Ctrl+Shift+S". */
std::string command_hint(const Command &command);

using KeyPressed = std::function<bool(ImGuiKey key, bool repeat)>;

/* The first enabled command with a shortcut that was pressed this frame (Ctrl is assumed held),
 * or null. */
const Command *find_command(bool shift, const KeyPressed &pressed,
                            const std::function<bool(CommandId)> &enabled);

#endif
