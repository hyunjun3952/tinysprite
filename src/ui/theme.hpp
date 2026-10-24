#ifndef TINYSPRITE_UI_THEME_HPP
#define TINYSPRITE_UI_THEME_HPP

#include <initializer_list>
#include <string>

#include "imgui.h"

/* The look of the UI: a neutral near-black with a short ladder of lifted surfaces, no borders on
 * controls (a fill tells them from the panel), hairline dividers, 6-8 px corners and one accent
 * color (off-white) that only marks what is selected or focused. The widgets are plain ImGui ones
 * styled by apply(); the helpers here are the few building blocks the panels share. */
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
inline constexpr unsigned ACCENT = 0xe8eaed;
inline constexpr unsigned ACCENT_STRONG = 0xc4c8ce; /* the pressed accent */
inline constexpr unsigned ACCENT_TEXT = 0x0f1011;

/* The filled part of a slider: a lifted grey, so the digits on it stay readable. */
inline constexpr unsigned SLIDER_FILL = 0x3a3d44;
inline constexpr unsigned SLIDER_FILL_ACTIVE = 0x4a4e57;

/* A button that destroys something (delete, discard, reset): the accent's red counterpart. */
inline constexpr unsigned DANGER = 0xc0403d;
inline constexpr unsigned DANGER_STRONG = 0xa63836;

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

/* The scale last passed to setScale: what the user chose, which is what gets saved. scale() is the
 * same unless cap_scale lowered it. */
float requested_scale();

/* Uses at most `largest` (never below SCALE_MIN) until the next setScale: a display too small for
 * the layout at the requested scale. The requested scale stays. Call apply() afterwards. */
void cap_scale(float largest);
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

/* ImGui::BeginMenu for a title of the menu bar, but the menu also opens when the mouse rests on the
 * title, without a click (while another menu is open ImGui already moves between titles). Use it
 * for the menu bar's titles; submenus inside a menu already open on hover. */
bool begin_menu(const char *label);

/* The printf format of a whole number followed by `unit` ("%d ms" for " ms"; a "%" in the unit is
 * escaped), for the number fields and sliders that should show what they count. */
std::string int_format(const char *unit);

/* Where to draw a unit's text so that it sits at the right end of a field [min, max], vertically
 * centered, `padding` from the edge. */
ImVec2 unit_origin(ImVec2 min, ImVec2 max, float textWidth, float textHeight, float padding);

/* ImGui::InputInt with a unit after the number ("100 ms"). The unit is drawn over the right end of
 * the field, so it stays while the number is typed (ImGui itself shows only the digits then). */
bool input_int(const char *id, int *value, int step, int stepFast, const char *unit,
               ImGuiInputTextFlags flags = 0);

/* ImGui::SliderInt in the style of the fields: a rounded field filled with the accent up to the
 * value, and the value with `unit` centered on it ("50%"). There is no grip to cover the digits and
 * no Ctrl+click text entry. Drag, or use the arrow keys when focused. */
bool slider_int(const char *id, int *value, int min, int max, const char *unit = "");

/* ImGui::BeginCombo with a drawn chevron at the right end instead of the arrow button. Call
 * ImGui::EndCombo() when it returns true. */
bool begin_combo(const char *id, const char *preview);

/* The width of a dialog button with a label `textWidth` wide: at least 88 px, else the text with 16
 * px at each side. */
float dialog_button_width(float textWidth);

/* The buttons at the foot of a dialog: under a hairline, right-aligned, each as wide as
 * dialog_button_width with a gap between, so they read as separate buttons. The one at index
 * `primary` is filled with the accent (red when `danger`: a button that destroys something); a
 * negative `primary` fills none. A set bit `i` of `disabled` greys out button `i`. Returns the
 * index of the button clicked this frame, or -1. */
int dialog_buttons(std::initializer_list<const char *> labels, int primary, bool danger = false,
                   unsigned disabled = 0);

/* A color as hex text for the picker: RRGGBB, with AA after it only when the color is translucent.
 * `rgba` are four floats in 0..1. */
std::string color_hex(const float *rgba);

/* Reads #RGB, #RRGGBB or #RRGGBBAA (the # is optional, any case) into `rgba`. Returns false and
 * leaves `rgba` alone for anything else. */
bool parse_color_hex(const char *text, float *rgba);

/* The hex field of a color picker: a rounded preview of the color, a "#" and the digits to edit.
 * `buffer` holds the text while it is typed (the caller keeps it between frames). Returns true when
 * a valid entry changed `rgba`. */
inline constexpr size_t HEX_BUFFER_SIZE = 10;
bool hex_field(const char *id, float *rgba, char (&buffer)[HEX_BUFFER_SIZE]);

/* Shows `text` as a tooltip when the last item has been hovered a moment (never while it is
 * disabled). Call right after the item. */
void tip(const char *text);

/* For the item just drawn, a button that can be disabled: its tooltip while it is enabled, and
 * while it is disabled no tooltip but the "not allowed" mouse cursor. */
void hint(const char *tooltip, bool enabled);

/* `text` cut at a UTF-8 character boundary and ended with "..." so that it is at most `maxWidth`
 * wide (as drawn by the current font); text that fits comes back unchanged. */
std::string ellipsize(const std::string &text, float maxWidth);

/* Width of one of `count` equal columns that fill the available width. */
float columnWidth(int count);

/* The color of a choice button's shortcut hint (an RGB token): mute at rest, body while the button
 * is `pressed`, because mute is under 4.5:1 on the pressed fill. */
unsigned hint_text(bool pressed);

/* A left-aligned button with its keyboard shortcut at the right end. The selected one is drawn
 * inverted (off-white fill, dark text), the way the style marks the active choice. */
bool choiceButton(const char *label, const char *hint, bool selected, float width,
                  const char *tooltip = nullptr);

/* A plain button with a tooltip that also shows while it is disabled. */
bool actionButton(const char *label, const char *tooltip, float width, bool enabled = true);

/* Colors for the canvas area, as ImU32. */
ImU32 workspaceColor();

/* Tag bands of the timeline, ABGR (user data: the only hue besides the artwork), and the dark text
 * drawn on them. */
inline constexpr ImU32 TAG_COLORS[] = {0xff5a78c8U, 0xff6aa860U, 0xffc08a50U,
                                       0xff5aaac0U, 0xffa069b8U, 0xff6e8ec0U};
inline constexpr ImU32 TAG_TEXT = 0xff1f1b19U;
/* The translucent off-white that marks where a dragged panel will dock, ABGR. */
inline constexpr ImU32 PANEL_DROP = 0x66f0f5f7U;

/* Colors drawn on and around the artwork (not chrome): they must read on light and dark art, so
 * they are fixed values and do not follow the surface ladder. */
namespace overlay
{
inline constexpr ImU32 CHECKER_LIGHT = IM_COL32(170, 170, 170, 255); /* transparency checker */
inline constexpr ImU32 CHECKER_TILE = IM_COL32(150, 150, 150, 255);  /* the canvas's own tiles */
inline constexpr ImU32 CHECKER_DARK = IM_COL32(120, 120, 120, 255);  /* thumbnails and loupe */
inline constexpr ImU32 PIXEL_GRID = IM_COL32(0, 0, 0, 80);
inline constexpr ImU32 ROW_HIGHLIGHT = IM_COL32(255, 255, 255, 70);
inline constexpr ImU32 PIXEL_OUTLINE = IM_COL32(255, 255, 255, 220);
inline constexpr ImU32 LINE_SHADOW = IM_COL32(0, 0, 0, 220);
inline constexpr ImU32 LINE_LIGHT = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 LINE_DARK = IM_COL32(0, 0, 0, 255);
inline constexpr ImU32 SPRITE_FRAME = IM_COL32(200, 200, 205, 255);
inline constexpr ImU32 MIRROR_AXIS = IM_COL32(0, 200, 255, 200);
inline constexpr ImU32 LOUPE_BACKGROUND = IM_COL32(24, 24, 28, 240);
inline constexpr ImU32 LOUPE_OUTSIDE = IM_COL32(48, 48, 54, 255); /* beside the sprite */
inline constexpr ImU32 SLICE = IM_COL32(76, 201, 240, 255);       /* an unselected slice */
inline constexpr ImU32 SLICE_SELECTED = IM_COL32(255, 183, 3, 255);
inline constexpr ImU32 SLICE_SELECTED_FILL = IM_COL32(255, 183, 3, 34);
inline constexpr ImU32 SLICE_PIVOT = IM_COL32(255, 92, 161, 255);
inline constexpr ImU32 SLICE_SHADOW = IM_COL32(0, 0, 0, 200);
inline constexpr ImU32 SLICE_LABEL_BACKGROUND = IM_COL32(0, 0, 0, 170);
inline constexpr ImU32 TILE_GRID = IM_COL32(255, 255, 255, 85);   /* the tile picker's cell lines */
inline constexpr ImU32 TILE_PICKED = IM_COL32(74, 210, 255, 255); /* the picked tile's outline */
} // namespace overlay

/* Small transitions. ease_toward moves `current` toward `target` at a steady rate that covers 0..1
 * in `seconds` (never past the target; no duration jumps). animate keeps one such value per widget
 * id in the current window and advances it by the frame time: pass 1 while a state holds (hovered,
 * selected) and 0 otherwise, and use the result as the blend between the two looks. mix blends two
 * 0xRRGGBB colors. */
inline constexpr float ANIM_SECONDS = 0.12f;
float ease_toward(float current, float target, float dt, float seconds);
float animate(ImGuiID id, float target, float seconds = ANIM_SECONDS);
ImU32 mix(unsigned from, unsigned to, float t);

/* An unfilled chevron, drawn as two strokes: ">" when closed, "v" when open, in a box of `size`
 * pixels around `center`. chevron_points gives its three points (for tests and for the drawing). */
void chevron_points(ImVec2 center, float size, bool open, ImVec2 (&points)[3]);
void chevron(ImDrawList *list, ImVec2 center, float size, bool open, ImU32 color);

/* A tree node (or, with `header`, a collapsing header) with an unfilled chevron instead of ImGui's
 * filled triangle. The label is drawn here in `textColor`; ImGui's own arrow and text are hidden.
 * Takes and returns what ImGui::TreeNodeEx / CollapsingHeader do (a node still needs TreePop). */
bool tree_node(const char *label, ImGuiTreeNodeFlags flags, ImU32 textColor, bool header = false);

/* A modal dialog (BeginPopupModal, auto-resizing) that fades in over ANIM_SECONDS when it opens.
 * Open it with ImGui::OpenPopup as usual; call end_modal instead of EndPopup, and only when
 * begin_modal returned true. */
bool begin_modal(const char *title);
void end_modal();

} // namespace theme

#endif
