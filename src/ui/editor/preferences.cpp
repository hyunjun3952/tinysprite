/* Editor: saving and loading the preferences. */

#include "editor.hpp"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "editor/detail.hpp"

namespace
{

/* The range a saved timeline height may have, at UI scale 1 (a hand-edited file cannot make it
 * absurd). */
constexpr float TIMELINE_HEIGHT_MIN = 60.0f;
constexpr float TIMELINE_HEIGHT_MAX = 4000.0f;

std::string colorText(Rgba color)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
    return text;
}

bool parseColor(const std::string &text, Rgba &out)
{
    if (text.size() != 8) {
        return false;
    }
    char *end = nullptr;
    unsigned long value = std::strtoul(text.c_str(), &end, 16);
    if (*end != '\0') {
        return false;
    }
    out = Rgba{static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
               static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    return true;
}

} // namespace

void Editor::store(SettingsMap &settings) const
{
    settings_set_bool(settings, "view.grid", showGrid_);
    settings_set_bool(settings, "view.status", overlays_.status);
    settings_set_bool(settings, "view.cursor", overlays_.cursor);
    settings_set_bool(settings, "view.mirror_axes", overlays_.mirror_axes);
    settings_set_bool(settings, "view.slices", overlays_.slices);
    settings_set_bool(settings, "view.timeline", overlays_.timeline);
    settings_set_bool(settings, "view.options_bar", overlays_.options_bar);
    settings_set_int(settings, "view.status_items", static_cast<int>(overlays_.status_items));
    settings_set_float(settings, "view.timeline_height", overlays_.timeline_height);
    settings_set_bool(settings, "onion.on", onionOn_);
    settings_set_int(settings, "onion.before", onion_.before);
    settings_set_int(settings, "onion.after", onion_.after);
    settings_set_int(settings, "onion.opacity", onion_.opacity);
    settings_set_int(settings, "brush.size", options_.brush_size);
    settings_set_bool(settings, "brush.round", options_.brush_round);
    settings_set_bool(settings, "brush.pixel_perfect", options_.pixel_perfect);
    settings_set_bool(settings, "shape.filled", options_.shape_filled);
    settings["color.fg"] = colorText(foreground());
    settings["color.bg"] =
        colorText(Rgba{editor_detail::toByte(bgColor_[0]), editor_detail::toByte(bgColor_[1]),
                       editor_detail::toByte(bgColor_[2]), editor_detail::toByte(bgColor_[3])});
    /* A tool borrowed by the middle-button eyedropper is not the user's choice. */
    settings_set_int(settings, "tool", static_cast<int>(tempPick_ ? pickReturn_ : tool_));
}

void Editor::adoptPreferences(const Editor &other)
{
    SettingsMap preferences;
    other.store(preferences);
    load(preferences);
    options_.symmetry = other.options_.symmetry;
    if (other.clipboard_) {
        clipboard_ = other.clipboard_;
    }
}

void Editor::load(const SettingsMap &settings)
{
    settings_get_bool(settings, "view.grid", showGrid_);
    settings_get_bool(settings, "view.status", overlays_.status);
    settings_get_bool(settings, "view.cursor", overlays_.cursor);
    settings_get_bool(settings, "view.mirror_axes", overlays_.mirror_axes);
    settings_get_bool(settings, "view.slices", overlays_.slices);
    settings_get_bool(settings, "view.timeline", overlays_.timeline);
    settings_get_bool(settings, "view.options_bar", overlays_.options_bar);
    int statusItems = 0;
    if (settings_get_int(settings, "view.status_items", 0, INT_MAX, statusItems)) {
        overlays_.status_items = static_cast<unsigned>(statusItems);
    }
    settings_get_float(settings, "view.timeline_height", TIMELINE_HEIGHT_MIN, TIMELINE_HEIGHT_MAX,
                       overlays_.timeline_height);
    bool onionOn = onionOn_;
    OnionSkin onion = onion_;
    int value = 0;
    settings_get_bool(settings, "onion.on", onionOn);
    if (settings_get_int(settings, "onion.before", 0, ONION_MAX_FRAMES, value)) {
        onion.before = value;
    }
    if (settings_get_int(settings, "onion.after", 0, ONION_MAX_FRAMES, value)) {
        onion.after = value;
    }
    if (settings_get_int(settings, "onion.opacity", 0, 255, value)) {
        onion.opacity = static_cast<uint8_t>(value);
    }
    setOnion(onionOn, onion);
    settings_get_int(settings, "brush.size", 1, PENCIL_BRUSH_MAX, options_.brush_size);
    settings_get_bool(settings, "brush.round", options_.brush_round);
    settings_get_bool(settings, "brush.pixel_perfect", options_.pixel_perfect);
    settings_get_bool(settings, "shape.filled", options_.shape_filled);
    Rgba color;
    if (auto it = settings.find("color.fg");
        it != settings.end() && parseColor(it->second, color)) {
        setForeground(color);
    }
    if (auto it = settings.find("color.bg");
        it != settings.end() && parseColor(it->second, color)) {
        editor_detail::setFloats(bgColor_, color);
    }
    int tool = 0;
    if (settings_get_int(settings, "tool", 0, TOOL_COUNT - 1, tool)) {
        selectTool(static_cast<ToolKind>(tool));
    }
}
