#ifndef TINYSPRITE_UI_THEME_HPP
#define TINYSPRITE_UI_THEME_HPP

#include <string>

#include "imgui.h"

/* The look of the UI: a neutral near-black with a short ladder of lifted surfaces, no borders on
 * controls (a fill tells them from the panel), hairline dividers, 6-8 px corners and one accent
 * color that only marks what is selected or focused. The widgets are plain ImGui ones styled by
 * apply(); the helpers here are the few building blocks the panels share. */
namespace theme
{

constexpr ImVec4 rgb(unsigned hex, float alpha = 1.0f)
{
    return {static_cast<float>((hex >> 16) & 0xff) / 255.0f,
            static_cast<float>((hex >> 8) & 0xff) / 255.0f, static_cast<float>(hex & 0xff) / 255.0f,
            alpha};
}

/* Surfaces. */
inline constexpr unsigned CANVAS = 0x0f1011;      /* window background */
inline constexpr unsigned CANVAS_SOFT = 0x1d1e21; /* cards, inputs, buttons */
inline constexpr unsigned HAIRLINE = 0x2a2c31;  /* 1 px dividers and the few borders that remain */
inline constexpr unsigned HOVER = 0x26282c;     /* hovered soft surface */
inline constexpr unsigned PRESSED = 0x2f3136;   /* pressed / selected soft surface */
inline constexpr unsigned WORKSPACE = 0x08090a; /* behind the sprite, darker than the panels */

/* The accent marks what is selected or focused (the active tool, checkmarks, the slider grab) and
 * nothing else; ACCENT_TEXT is the text and icon color on top of it. */
inline constexpr unsigned ACCENT = 0x6c78e6;
inline constexpr unsigned ACCENT_STRONG = 0x5865d0; /* the pressed accent */
inline constexpr unsigned ACCENT_TEXT = 0xffffff;

/* Text. */
inline constexpr unsigned INK = 0xf7f8f8;
inline constexpr unsigned BODY_STRONG = 0xd0d6e0;
inline constexpr unsigned BODY = 0xb0b6c0;
inline constexpr unsigned MUTE = 0x8a8f98;

/* The font size at scale 1, in pixels. */
inline constexpr float FONT_SIZE = 14.0f;

/* Adds the embedded UI font (Geist Regular, a subset) to the current context's atlas and sets the
 * base font size. Call it once after creating the context and before apply(). Returns false and
 * leaves ImGui's built-in font in place if the font data cannot be read. */
bool load_fonts();

/* The UI scale: 1 is the FONT_SIZE font and the sizes below as designed. Everything drawn in pixels
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

/* The printf format of a whole number followed by `unit` ("%d ms" for " ms"; a "%" in the unit is
 * escaped), for the number fields and sliders that should show what they count. */
std::string int_format(const char *unit);

/* ImGui::InputInt with a unit after the number: "100 ms" while the field is not being edited. The
 * unit text is ignored when a value is typed. */
bool input_int(const char *id, int *value, int step, int stepFast, const char *unit,
               ImGuiInputTextFlags flags = 0);

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
