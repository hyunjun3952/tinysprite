#include "theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "geist_regular_subset.hpp"
#include "imgui_internal.h"
#include "material_symbols_outlined_subset.hpp"

namespace theme
{

namespace
{

constexpr float DIALOG_BUTTON_MIN_WIDTH = 88.0f;
constexpr float DIALOG_BUTTON_PADDING = 16.0f; /* at each side of the label */
constexpr float DIALOG_BUTTON_GAP = 8.0f;
constexpr float HEX_HASH_SCALE = 1.4f; /* the # of the hex field, times the text size */

constexpr float ROUND_SMALL = 6.0f; /* buttons, inputs */
constexpr float ROUND_CARD = 8.0f;  /* popups, child areas */

/* The one UI scale, set at startup. */
float uiScale = 1.0f; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/* The fill of a slider field: lifts while hovered and again while dragged. */
unsigned fieldFill(bool active, bool hovered)
{
    if (active) {
        return PRESSED;
    }
    return hovered ? HOVER : CANVAS_SOFT;
}

ImU32 toU32(unsigned hex, float alpha = 1.0f)
{
    return ImGui::ColorConvertFloat4ToU32(rgb(hex, alpha));
}

} // namespace

void setScale(float scale)
{
    uiScale = std::clamp(scale, SCALE_MIN, SCALE_MAX);
}

float scale()
{
    return uiScale;
}

float px(float pixels)
{
    return pixels * uiScale;
}

float scaleForDpi(float dpi)
{
    if (!(dpi > 0.0f)) {
        return 1.0f;
    }
    return std::clamp(std::round(dpi / 96.0f * 4.0f) / 4.0f, SCALE_MIN, SCALE_MAX);
}

bool load_fonts()
{
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false; /* the data is static */
    /* stb_truetype does not hint, so a glyph at a fractional position, or oversampled, comes out
     * soft and unevenly spaced at 14 px. Whole-pixel positions and one sample per pixel keep the
     * stems and the letter spacing even. */
    config.PixelSnapH = true;
    config.OversampleH = 1;
    config.OversampleV = 1;
    std::snprintf(config.Name, sizeof(config.Name), "Geist Regular");
    ImFont *font = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(GEIST_REGULAR_SUBSET),
                                                  static_cast<int>(sizeof(GEIST_REGULAR_SUBSET)),
                                                  0.0f, &config);
    if (font == nullptr) {
        return false;
    }
    /* The icons: Material Symbols merged into the same font, so a glyph is drawn with the text. */
    static const ImWchar ICON_RANGE[] = {0xE000, 0xF8FF, 0};
    ImFontConfig icons;
    icons.FontDataOwnedByAtlas = false;
    icons.MergeMode = true;
    icons.PixelSnapH = true;
    icons.OversampleH = 1;
    icons.OversampleV = 1;
    std::snprintf(icons.Name, sizeof(icons.Name), "Material Symbols Outlined");
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(MATERIAL_SYMBOLS_OUTLINED_SUBSET),
                                   static_cast<int>(sizeof(MATERIAL_SYMBOLS_OUTLINED_SUBSET)), 0.0f,
                                   &icons, ICON_RANGE);
    ImGui::GetStyle().FontSizeBase = FONT_SIZE;
    return true;
}

void apply()
{
    ImGuiStyle &style = ImGui::GetStyle();
    /* Start from the defaults each time: ScaleAllSizes multiplies, so a second apply() would
     * otherwise compound. The base font size belongs to the context, not to the look. */
    float fontSize = style.FontSizeBase;
    style = ImGuiStyle();
    style.FontSizeBase = fontSize;
    ImGui::StyleColorsDark(&style);

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f; /* controls are told apart by their fill */
    style.TabBorderSize = 0.0f;

    /* Docked panels fill the window edge to edge, so windows stay square. */
    style.WindowRounding = 0.0f;
    style.ChildRounding = ROUND_CARD;
    style.PopupRounding = ROUND_CARD;
    style.FrameRounding = ROUND_SMALL;
    style.GrabRounding = ROUND_SMALL;
    style.ScrollbarRounding = ROUND_SMALL;
    style.TabRounding = ROUND_SMALL;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4 *c = style.Colors;
    c[ImGuiCol_Text] = rgb(INK);
    c[ImGuiCol_TextDisabled] = rgb(MUTE);
    c[ImGuiCol_WindowBg] = rgb(CANVAS);
    c[ImGuiCol_ChildBg] = rgb(CANVAS);
    c[ImGuiCol_PopupBg] = rgb(CANVAS); /* the fields and buttons in a popup are CANVAS_SOFT */
    c[ImGuiCol_Border] = rgb(HAIRLINE);
    c[ImGuiCol_BorderShadow] = rgb(0x000000, 0.0f);
    c[ImGuiCol_FrameBg] = rgb(CANVAS_SOFT);
    c[ImGuiCol_FrameBgHovered] = rgb(HOVER);
    c[ImGuiCol_FrameBgActive] = rgb(PRESSED);
    c[ImGuiCol_TitleBg] = rgb(CANVAS);
    c[ImGuiCol_TitleBgActive] = rgb(CANVAS);
    c[ImGuiCol_TitleBgCollapsed] = rgb(CANVAS);
    c[ImGuiCol_MenuBarBg] = rgb(CANVAS);
    c[ImGuiCol_ScrollbarBg] = rgb(CANVAS);
    c[ImGuiCol_ScrollbarGrab] = rgb(PRESSED);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(MUTE, 0.6f);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(MUTE);
    c[ImGuiCol_CheckMark] = rgb(ACCENT);
    c[ImGuiCol_SliderGrab] = rgb(BODY); /* slider_int draws its own */
    c[ImGuiCol_SliderGrabActive] = rgb(ACCENT);
    c[ImGuiCol_Button] = rgb(CANVAS_SOFT);
    c[ImGuiCol_ButtonHovered] = rgb(HOVER);
    c[ImGuiCol_ButtonActive] = rgb(PRESSED);
    c[ImGuiCol_Header] = rgb(PRESSED);
    c[ImGuiCol_HeaderHovered] = rgb(HOVER);
    c[ImGuiCol_HeaderActive] = rgb(PRESSED);
    c[ImGuiCol_Separator] = rgb(HAIRLINE);
    c[ImGuiCol_SeparatorHovered] = rgb(MUTE, 0.6f);
    c[ImGuiCol_SeparatorActive] = rgb(MUTE);
    c[ImGuiCol_ResizeGrip] = rgb(HAIRLINE);
    c[ImGuiCol_ResizeGripHovered] = rgb(MUTE, 0.6f);
    c[ImGuiCol_ResizeGripActive] = rgb(MUTE);
    c[ImGuiCol_TableHeaderBg] = rgb(CANVAS_SOFT);
    c[ImGuiCol_TableBorderStrong] = rgb(HAIRLINE);
    c[ImGuiCol_TableBorderLight] = rgb(HAIRLINE);
    c[ImGuiCol_TableRowBg] = rgb(0x000000, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = rgb(INK, 0.03f);
    c[ImGuiCol_TextSelectedBg] = rgb(INK, 0.25f);
    c[ImGuiCol_DragDropTarget] = rgb(ACCENT);
    c[ImGuiCol_NavCursor] = rgb(INK, 0.7f);
    c[ImGuiCol_NavWindowingHighlight] = rgb(INK, 0.7f);
    c[ImGuiCol_NavWindowingDimBg] = rgb(0x000000, 0.4f);
    c[ImGuiCol_ModalWindowDimBg] = rgb(0x000000, 0.5f);

    style.ScaleAllSizes(uiScale);
    style.FontScaleMain = uiScale;
}

bool section(const char *label, bool openByDefault)
{
    /* A hairline above and a plain caret and title, not a filled bar: a stack of sections then
     * reads as one list with dividers, and the controls under a header stay the heaviest thing. */
    ImVec2 at = ImGui::GetCursorScreenPos();
    float y = std::floor(at.y - ImGui::GetStyle().ItemSpacing.y * 0.5f);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(at.x, y),
                                        ImVec2(at.x + ImGui::GetContentRegionAvail().x, y),
                                        ImGui::ColorConvertFloat4ToU32(rgb(HAIRLINE, 0.6f)));
    ImGui::PushStyleColor(ImGuiCol_Header, rgb(0x000000, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, rgb(HOVER, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, rgb(PRESSED, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(BODY_STRONG));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGuiTreeNodeFlags flags = openByDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    bool open = ImGui::CollapsingHeader(label, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return open;
}

float columnWidth(int count)
{
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float width = (ImGui::GetContentRegionAvail().x - spacing * static_cast<float>(count - 1)) /
                  static_cast<float>(count);
    return std::max(width, 1.0f);
}

bool begin_menu(const char *label)
{
    const bool open = ImGui::BeginMenu(label);
    if (!open && ImGui::IsItemHovered() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        ImGui::OpenPopup(label);
    }
    return open;
}

std::string int_format(const char *unit)
{
    std::string format = "%d";
    for (const char *c = unit; *c != '\0'; c++) {
        format += *c;
        if (*c == '%') {
            format += '%';
        }
    }
    return format;
}

ImVec2 unit_origin(ImVec2 min, ImVec2 max, float textWidth, float textHeight, float padding)
{
    return {max.x - padding - textWidth, std::floor((min.y + max.y - textHeight) / 2.0f)};
}

bool input_int(const char *id, int *value, int step, int stepFast, const char *unit,
               ImGuiInputTextFlags flags)
{
    const bool changed =
        ImGui::InputScalar(id, ImGuiDataType_S32, value, step > 0 ? &step : nullptr,
                           stepFast > 0 ? &stepFast : nullptr, "%d", flags);
    while (*unit == ' ') {
        unit++; /* the gap is the field's padding, not a space */
    }
    /* the - and + buttons of a stepped field are part of the item: the unit ends before them */
    const float buttons =
        step > 0 ? 2.0f * (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x) : 0.0f;
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max(ImGui::GetItemRectMax().x - buttons, ImGui::GetItemRectMax().y);
    const ImVec2 size = ImGui::CalcTextSize(unit);
    ImGui::GetWindowDrawList()->AddText(unit_origin(min, max, size.x, size.y, px(8.0f)),
                                        ImGui::ColorConvertFloat4ToU32(rgb(MUTE)), unit);
    return changed;
}

bool slider_int(const char *id, int *value, int min, int max, const char *unit)
{
    /* ImGui does the input; it is made invisible and everything is drawn over it afterwards. */
    ImGui::PushStyleColor(ImGuiCol_FrameBg, rgb(0x000000, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, rgb(0x000000, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, rgb(0x000000, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, rgb(0x000000, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, rgb(0x000000, 0.0f));
    const bool changed = ImGui::SliderInt(id, value, min, max, "", ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor(5);

    const ImVec2 lo = ImGui::GetItemRectMin();
    const ImVec2 hi = ImGui::GetItemRectMax();
    const float rounding = ImGui::GetStyle().FrameRounding;
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(lo, hi, toU32(fieldFill(active, hovered)), rounding);
    const float t =
        max > min ? std::clamp(static_cast<float>(*value - min) / static_cast<float>(max - min),
                               0.0f, 1.0f)
                  : 0.0f;
    if (t > 0.0f) {
        const float x = std::floor(lo.x + (hi.x - lo.x) * t);
        draw->AddRectFilled(lo, ImVec2(x, hi.y), toU32(active ? SLIDER_FILL_ACTIVE : SLIDER_FILL),
                            rounding,
                            t >= 1.0f ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
    }
    const std::string text = [&] {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, int_format(unit).c_str(), *value);
        return std::string(buffer);
    }();
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    draw->AddText(ImVec2(std::floor((lo.x + hi.x - size.x) / 2.0f),
                         std::floor((lo.y + hi.y - size.y) / 2.0f)),
                  toU32(INK), text.c_str());
    return changed;
}

bool begin_combo(const char *id, const char *preview)
{
    const bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
    /* BeginCombo's item is the field; the chevron sits inside its right end. */
    const ImVec2 lo = ImGui::GetItemRectMin();
    const ImVec2 hi = ImGui::GetItemRectMax();
    const float half = px(3.5f);
    const float cx = hi.x - ImGui::GetStyle().FramePadding.x - half - px(2.0f);
    const float cy = std::floor((lo.y + hi.y) / 2.0f);
    const ImVec2 points[3] = {ImVec2(cx - half, cy - half * 0.5f), ImVec2(cx, cy + half * 0.5f),
                              ImVec2(cx + half, cy - half * 0.5f)};
    ImGui::GetWindowDrawList()->AddPolyline(points, 3, toU32(MUTE), ImDrawFlags_None, px(1.5f));
    return open;
}

float dialog_button_width(float textWidth)
{
    return std::max(px(DIALOG_BUTTON_MIN_WIDTH), textWidth + px(2.0f * DIALOG_BUTTON_PADDING));
}

namespace
{

/* One button of a dialog's row; `filled` colors it as the main one (accent, or red for `danger`).
 */
bool dialogButton(const char *label, bool filled, bool danger, bool disabled)
{
    if (filled) {
        const unsigned fill = danger ? DANGER : ACCENT;
        const unsigned strong = danger ? DANGER_STRONG : ACCENT_STRONG;
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(fill));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(fill, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(strong));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(danger ? INK : ACCENT_TEXT));
    }
    ImGui::BeginDisabled(disabled);
    const bool clicked =
        ImGui::Button(label, ImVec2(dialog_button_width(ImGui::CalcTextSize(label).x), 0.0f));
    ImGui::EndDisabled();
    if (filled) {
        ImGui::PopStyleColor(4);
    }
    return clicked;
}

/* The width of the whole row, gaps included. */
float dialogRowWidth(std::initializer_list<const char *> labels)
{
    float total = px(DIALOG_BUTTON_GAP) * static_cast<float>(labels.size() - 1);
    for (const char *label : labels) {
        total += dialog_button_width(ImGui::CalcTextSize(label).x);
    }
    return total;
}

} // namespace

int dialog_buttons(std::initializer_list<const char *> labels, int primary, bool danger,
                   unsigned disabled)
{
    ImGui::Dummy(ImVec2(0.0f, px(4.0f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, px(2.0f)));
    const float spare = ImGui::GetContentRegionAvail().x - dialogRowWidth(labels);
    if (spare > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spare); /* right-aligned */
    }
    int clicked = -1;
    int index = 0;
    for (const char *label : labels) {
        if (index > 0) {
            ImGui::SameLine(0.0f, px(DIALOG_BUTTON_GAP));
        }
        const bool off = (disabled & (1u << static_cast<unsigned>(index))) != 0;
        if (dialogButton(label, index == primary, danger, off)) {
            clicked = index;
        }
        index++;
    }
    return clicked;
}

std::string color_hex(const float *rgba)
{
    auto byte = [](float v) {
        return static_cast<unsigned>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    char text[16];
    const unsigned alpha = byte(rgba[3]);
    if (alpha == 255) {
        std::snprintf(text, sizeof text, "%02X%02X%02X", byte(rgba[0]), byte(rgba[1]),
                      byte(rgba[2]));
    } else {
        std::snprintf(text, sizeof text, "%02X%02X%02X%02X", byte(rgba[0]), byte(rgba[1]),
                      byte(rgba[2]), alpha);
    }
    return text;
}

namespace
{

/* The value of one hex digit, or -1. */
int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

bool parse_color_hex(const char *text, float *rgba)
{
    if (*text == '#') {
        text++;
    }
    const size_t length = std::strlen(text);
    if (length != 3 && length != 6 && length != 8) {
        return false;
    }
    int digits[8];
    for (size_t i = 0; i < length; i++) {
        digits[i] = hexDigit(text[i]);
        if (digits[i] < 0) {
            return false;
        }
    }
    float parsed[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (length == 3) {
        for (size_t i = 0; i < 3; i++) {
            parsed[i] = static_cast<float>(digits[i] * 17) / 255.0f; /* F -> FF */
        }
    } else {
        for (size_t i = 0; i < length / 2; i++) {
            parsed[i] = static_cast<float>(digits[i * 2] * 16 + digits[i * 2 + 1]) / 255.0f;
        }
    }
    std::copy(parsed, parsed + 4, rgba);
    return true;
}

bool hex_field(const char *id, float *rgba, char (&buffer)[HEX_BUFFER_SIZE])
{
    ImGui::PushID(id);
    const float side = ImGui::GetFrameHeight() + px(6.0f);
    ImGui::ColorButton("##preview", ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]),
                       ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoInputs,
                       ImVec2(side, side));
    ImGui::SameLine();
    /* the # is a prefix to read at a glance, not a hint: larger than the text, in the body color */
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * HEX_HASH_SCALE);
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(BODY));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("#");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine(0.0f, px(4.0f));
    ImGui::SetNextItemWidth(px(112.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(px(10.0f), (side - ImGui::GetFontSize()) / 2.0f));
    /* while the field is not being typed in, it follows the color (the wheel, the other picker) */
    if (ImGui::GetActiveID() != ImGui::GetID("##hex")) {
        std::snprintf(buffer, HEX_BUFFER_SIZE, "%s", color_hex(rgba).c_str());
    }
    constexpr ImGuiInputTextFlags FLAGS =
        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase |
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
    bool entered = ImGui::InputText("##hex", buffer, HEX_BUFFER_SIZE, FLAGS);
    entered = entered || ImGui::IsItemDeactivatedAfterEdit();
    ImGui::PopStyleVar();
    tip("Type a color as RGB, RRGGBB or RRGGBBAA, then press Enter.");
    ImGui::PopID();
    return entered && parse_color_hex(buffer, rgba);
}

void tip(const char *text)
{
    /* Without AllowWhenDisabled a disabled item is never hovered, so it gets no tooltip. */
    if (text && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s", text);
    }
}

void hint(const char *tooltip, bool enabled)
{
    if (enabled) {
        tip(tooltip);
    } else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }
}

bool choiceButton(const char *label, const char *hint, bool selected, float width,
                  const char *tooltip)
{
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(ACCENT_STRONG));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(ACCENT_TEXT));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::PopStyleVar();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    if (hint != nullptr && hint[0] != '\0') {
        ImVec2 size = ImGui::CalcTextSize(hint);
        ImVec2 at(max.x - size.x - ImGui::GetStyle().FramePadding.x,
                  min.y + (max.y - min.y - size.y) / 2.0f);
        ImGui::GetWindowDrawList()->AddText(at, selected ? toU32(CANVAS, 0.7f) : toU32(MUTE), hint);
    }
    if (selected) {
        ImGui::PopStyleColor(4);
    }
    tip(tooltip);
    return clicked;
}

bool actionButton(const char *label, const char *tooltip, float width, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::EndDisabled();
    hint(tooltip, enabled);
    return clicked && enabled;
}

float ease_toward(float current, float target, float dt, float seconds)
{
    if (seconds <= 0.0f) {
        return target;
    }
    float step = dt / seconds;
    if (current < target) {
        return std::min(current + step, target);
    }
    return std::max(current - step, target);
}

float animate(ImGuiID id, float target, float seconds)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();
    float value =
        ease_toward(storage->GetFloat(id, target), target, ImGui::GetIO().DeltaTime, seconds);
    storage->SetFloat(id, value);
    return value;
}

ImU32 mix(unsigned from, unsigned to, float t)
{
    ImVec4 a = rgb(from);
    ImVec4 b = rgb(to);
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f));
}

bool begin_modal(const char *title)
{
    /* The fade is the popup's alpha while its contents are drawn, so the style var stays pushed
     * until end_modal; with no popup it falls back to 0 so the next opening starts from nothing. */
    float fade = animate(ImGui::GetID(title), ImGui::IsPopupOpen(title) ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(fade, 0.01f));
    const bool shown = ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (!shown) {
        ImGui::PopStyleVar();
    }
    return shown;
}

void end_modal()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

ImU32 workspaceColor()
{
    return toU32(WORKSPACE);
}

} // namespace theme
