#ifndef TINYSPRITE_UI_THEME_HPP
#define TINYSPRITE_UI_THEME_HPP

#include "imgui.h"

/* The look of the UI: a warm near-charcoal dark theme with hairline borders, tight corner radii and
 * an off-white as the only accent (after the Warp design language, see the colors below). The
 * widgets are plain ImGui ones styled by apply(); the helpers here are the few building blocks the
 * panels share. */
namespace theme
{

constexpr ImVec4 rgb(unsigned hex, float alpha = 1.0f)
{
    return {static_cast<float>((hex >> 16) & 0xff) / 255.0f,
            static_cast<float>((hex >> 8) & 0xff) / 255.0f, static_cast<float>(hex & 0xff) / 255.0f,
            alpha};
}

/* Surfaces. */
inline constexpr unsigned CANVAS = 0x2b2622;      /* window background */
inline constexpr unsigned CANVAS_SOFT = 0x383330; /* cards, inputs, buttons */
inline constexpr unsigned HAIRLINE =
    0x6b645c;                                 /* 1 px borders and dividers, about 2.5:1 on CANVAS */
inline constexpr unsigned HOVER = 0x433d39;   /* hovered soft surface */
inline constexpr unsigned PRESSED = 0x4d4641; /* pressed / selected soft surface */
inline constexpr unsigned WORKSPACE = 0x1f1b19; /* behind the sprite, darker than the panels */

/* Text; INK doubles as the accent (selected tool, checkmarks). */
inline constexpr unsigned INK = 0xf7f5f0;
inline constexpr unsigned BODY_STRONG = 0xdad2c1;
inline constexpr unsigned BODY = 0xc9c0ad;
inline constexpr unsigned MUTE = 0xaea69c;

/* The UI scale: 1 is the 13 px font and the sizes below as designed. Everything drawn in pixels
 * (panel widths, cells, swatches) goes through px() so that it follows the scale. */
inline constexpr float SCALE_MIN = 1.0f;
inline constexpr float SCALE_MAX = 3.0f;
void setScale(float scale); /* clamped to SCALE_MIN..SCALE_MAX; call apply() afterwards */
float scale();
float px(float pixels); /* `pixels` at the current scale */

/* A scale that suits the primary display's DPI (96 dpi = 1), rounded to quarters; 1 when the DPI is
 * unknown. */
float scaleForDpi(float dpi);

/* Sets the ImGui style (colors, rounding, borders, spacing, font size) of the current context at
 * the current scale. Safe to call again when the scale changes. */
void apply();

/* A group header like Photoshop's panel sections: click to fold. Returns true while open, so the
 * content goes under `if (theme::section("Name")) { ... }`. */
bool section(const char *label, bool openByDefault = true);

/* Shows `text` as a tooltip when the last item has been hovered a moment (never while it is
 * disabled). Call right after the item. */
void tip(const char *text);

/* For the item just drawn, a button that can be disabled: its tooltip while it is enabled, and
 * while it is disabled no tooltip but the "not allowed" mouse cursor. */
void hint(const char *tooltip, bool enabled);

/* Width of one of `count` equal columns that fill the available width. */
float columnWidth(int count);

/* A left-aligned button with its keyboard shortcut at the right end. The selected one is drawn
 * inverted (off-white fill, dark text), the way the style marks the active choice. */
bool choiceButton(const char *label, const char *hint, bool selected, float width,
                  const char *tooltip = nullptr);

/* A plain button with a tooltip that also shows while it is disabled. */
bool actionButton(const char *label, const char *tooltip, float width, bool enabled = true);

/* Colors for the canvas area, as ImU32. */
ImU32 workspaceColor();

} // namespace theme

#endif
