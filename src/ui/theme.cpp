#include "theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "geist_regular_subset.hpp"
#include "imgui_internal.h"
#include "material_symbols_outlined_subset.hpp"
#include "noto_sans_kr_subset.hpp"

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

/* The UI scale in use: requestedScale, or less when cap_scale fitted it to the display. */
float uiScale = 1.0f; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
/* The scale last passed to setScale: what the user chose, and what is saved. */
float requestedScale = 1.0f; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
    requestedScale = std::clamp(scale, SCALE_MIN, SCALE_MAX);
    uiScale = requestedScale;
}

float scale()
{
    return uiScale;
}

float requested_scale()
{
    return requestedScale;
}

void cap_scale(float largest)
{
    uiScale = std::clamp(std::min(requestedScale, largest), SCALE_MIN, SCALE_MAX);
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
    /* Korean: the Hangul subset merged in after the Latin font, so Latin keeps Geist's shapes. */
    static const ImWchar HANGUL_RANGE[] = {0x00B7, 0x00B7, 0x2018, 0x201D, 0x2026, 0x2026,
                                           0x203B, 0x203B, 0x3001, 0x3002, 0x300C, 0x300F,
                                           0x3131, 0x3163, 0xAC00, 0xD7A3, 0};
    ImFontConfig korean;
    korean.FontDataOwnedByAtlas = false;
    korean.MergeMode = true;
    korean.PixelSnapH = true;
    korean.OversampleH = 1;
    korean.OversampleV = 1;
    std::snprintf(korean.Name, sizeof(korean.Name), "Noto Sans KR");
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(NOTO_SANS_KR_SUBSET),
                                   static_cast<int>(sizeof(NOTO_SANS_KR_SUBSET)), 0.0f, &korean,
                                   HANGUL_RANGE);
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
    /* document tabs: the selected one sits on a soft fill with a thin accent line on top */
    c[ImGuiCol_Tab] = rgb(CANVAS);
    c[ImGuiCol_TabHovered] = rgb(HOVER);
    c[ImGuiCol_TabSelected] = rgb(CANVAS_SOFT);
    c[ImGuiCol_TabSelectedOverline] = rgb(ACCENT, 0.9f);
    c[ImGuiCol_TabDimmed] = rgb(CANVAS);
    c[ImGuiCol_TabDimmedSelected] = rgb(CANVAS_SOFT);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(MUTE, 0.7f);
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
    bool open = tree_node(label, flags, toU32(BODY_STRONG), true);
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

namespace
{

constexpr float MENU_LEAVE_SECONDS =
    0.25f;                                /* how long the mouse may be away before a menu closes */
constexpr float MENU_LEAVE_MARGIN = 6.0f; /* the slack around the bar and the open menus */

/* Whether the mouse is over `bar` (the menu bar's rectangle) or any open popup (a menu and its
 * submenus are popups), with a little slack for the gap between a title and its menu. */
bool mouse_near_menus(ImRect bar)
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float slack = px(MENU_LEAVE_MARGIN);
    bar.Expand(slack);
    if (bar.Contains(mouse)) {
        return true;
    }
    for (const ImGuiPopupData &popup : ImGui::GetCurrentContext()->OpenPopupStack) {
        if (popup.Window != nullptr) {
            ImRect rect = popup.Window->Rect();
            rect.Expand(slack);
            if (rect.Contains(mouse)) {
                return true;
            }
        }
    }
    return false;
}

/* A closed menu title opens when the mouse rests on it, unless a click on the title just closed it
 * (`closedKey` remembers that until the mouse leaves) or another popup is open. */
void open_on_hover(const char *label, ImGuiStorage *storage, ImGuiID closedKey, bool wasOpen)
{
    const bool hovered = ImGui::IsItemHovered();
    if (wasOpen && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        storage->SetBool(closedKey, true); /* a click on the title closed it: leave it closed */
    }
    if (!hovered) {
        storage->SetBool(closedKey, false);
    }
    if (hovered && !storage->GetBool(closedKey, false) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        ImGui::OpenPopup(label);
    }
}

} // namespace

bool begin_menu(const char *label)
{
    /* The bar's own state (not the popup's, which is current once BeginMenu returns true): how long
     * the mouse has been away from the menus while this one is open, whether it was open last
     * frame, and whether a click closed it (so the hover below does not open it again at once). */
    ImGuiStorage *storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetID(label);
    const ImGuiID openKey = key + 1;
    const ImGuiID closedKey = key + 2;
    float away = storage->GetFloat(key, 0.0f);
    const bool wasOpen = storage->GetBool(openKey, false);
    /* taken before BeginMenu: once it returns true the current window is the popup, not the bar */
    const ImRect bar = ImGui::GetCurrentWindow()->MenuBarRect();
    const bool open = ImGui::BeginMenu(label);
    if (open) {
        away = mouse_near_menus(bar) ? 0.0f : away + ImGui::GetIO().DeltaTime;
        if (away > MENU_LEAVE_SECONDS) {
            ImGui::CloseCurrentPopup(); /* the menu and its submenus */
            away = 0.0f;
        }
    } else {
        away = 0.0f;
        open_on_hover(label, storage, closedKey, wasOpen);
    }
    storage->SetFloat(key, away);
    storage->SetBool(openKey, open);
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

std::string ellipsize(const std::string &text, float maxWidth)
{
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
        return text;
    }
    size_t end = text.size();
    while (end > 0) {
        end--;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) {
            end--; /* back to the first byte of the character */
        }
        std::string cut = text.substr(0, end) + "...";
        if (end == 0 || ImGui::CalcTextSize(cut.c_str()).x <= maxWidth) {
            return cut;
        }
    }
    return "...";
}

void hint(const char *tooltip, bool enabled, const char *reason)
{
    if (enabled) {
        tip(tooltip);
    } else {
        why_disabled(false, reason);
    }
}

ImVec2 fit_in_box(int width, int height, float box)
{
    const float longer = static_cast<float>(std::max(std::max(width, height), 1));
    const float scale = box / longer;
    const float w = std::max(2.0f, std::round(static_cast<float>(width) * scale));
    const float h = std::max(2.0f, std::round(static_cast<float>(height) * scale));
    return {std::min(w, box), std::min(h, box)};
}

void size_preview(int width, int height, float box)
{
    constexpr float CHECKER_CELLS = 12.0f; /* tiles across the box */
    ImGui::Dummy(ImVec2(box, box));
    const ImVec2 well = ImGui::GetItemRectMin();
    ImDrawList *list = ImGui::GetWindowDrawList();
    list->AddRectFilled(well, ImVec2(well.x + box, well.y + box), toU32(WORKSPACE),
                        ImGui::GetStyle().FrameRounding);
    const ImVec2 size = fit_in_box(width, height, box - 2.0f * px(6.0f));
    const float x0 = std::floor(well.x + (box - size.x) / 2.0f);
    const float y0 = std::floor(well.y + (box - size.y) / 2.0f);
    list->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + size.x, y0 + size.y), overlay::CHECKER_LIGHT);
    const float tile = std::max(2.0f, std::round(box / CHECKER_CELLS));
    for (float ty = 0.0f; ty * tile < size.y; ty += 1.0f) {
        for (float tx = (std::fmod(ty, 2.0f) == 0.0f ? 1.0f : 0.0f); tx * tile < size.x;
             tx += 2.0f) {
            list->AddRectFilled(ImVec2(x0 + tx * tile, y0 + ty * tile),
                                ImVec2(std::min(x0 + (tx + 1.0f) * tile, x0 + size.x),
                                       std::min(y0 + (ty + 1.0f) * tile, y0 + size.y)),
                                overlay::CHECKER_TILE);
        }
    }
}

void why_disabled(bool enabled, const char *reason)
{
    constexpr ImGuiHoveredFlags HOVER = ImGuiHoveredFlags_AllowWhenDisabled;
    if (enabled || !ImGui::IsItemHovered(HOVER)) {
        return;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    if (reason != nullptr && ImGui::IsItemHovered(HOVER | ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s", reason);
    }
}

unsigned hint_text(bool pressed)
{
    return pressed ? BODY : MUTE;
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
        ImGui::GetWindowDrawList()->AddText(
            at, selected ? toU32(CANVAS, 0.7f) : toU32(hint_text(ImGui::IsItemActive())), hint);
    }
    if (selected) {
        ImGui::PopStyleColor(4);
    }
    tip(tooltip);
    return clicked;
}

bool actionButton(const char *label, const char *tooltip, float width, bool enabled,
                  const char *reason)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::EndDisabled();
    hint(tooltip, enabled, reason);
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

void pixel_image(ImDrawList *list, ImTextureRef texture, ImVec2 min, ImVec2 max, ImVec2 uv0,
                 ImVec2 uv1, ImU32 tint)
{
    const ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    const bool backendSamples = platform.DrawCallback_SetSamplerNearest != nullptr &&
                                platform.DrawCallback_SetSamplerLinear != nullptr;
    if (backendSamples) {
        list->AddCallback(platform.DrawCallback_SetSamplerNearest, nullptr);
    }
    list->AddImage(texture, min, max, uv0, uv1, tint);
    if (backendSamples) {
        list->AddCallback(platform.DrawCallback_SetSamplerLinear, nullptr);
    }
}

void chevron_points(ImVec2 center, float size, bool open, ImVec2 (&points)[3])
{
    const float half = size * 0.5f;
    const float tip = size * 0.25f;
    if (open) {
        points[0] = ImVec2(center.x - half, center.y - tip);
        points[1] = ImVec2(center.x, center.y + tip);
        points[2] = ImVec2(center.x + half, center.y - tip);
    } else {
        points[0] = ImVec2(center.x - tip, center.y - half);
        points[1] = ImVec2(center.x + tip, center.y);
        points[2] = ImVec2(center.x - tip, center.y + half);
    }
}

void chevron(ImDrawList *list, ImVec2 center, float size, bool open, ImU32 color)
{
    ImVec2 points[3];
    chevron_points(center, size, open, points);
    list->AddPolyline(points, 3, color, ImDrawFlags_None, std::max(1.0f, std::round(size * 0.16f)));
}

bool tree_node(const char *label, ImGuiTreeNodeFlags flags, ImU32 textColor, bool header)
{
    /* ImGui draws its arrow and the label in the Text color, so both are made invisible and drawn
     * again here: the chevron where its arrow would be, the label where its text would be. */
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    const std::string hidden = std::string("##") + label;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    const bool open = header ? ImGui::CollapsingHeader(hidden.c_str(), flags)
                             : ImGui::TreeNodeEx(hidden.c_str(), flags);
    ImGui::PopStyleColor();
    const float font = ImGui::GetFontSize();
    const ImVec2 min = ImGui::GetItemRectMin();
    const float middle = (min.y + ImGui::GetItemRectMax().y) / 2.0f;
    ImDrawList *list = ImGui::GetWindowDrawList();
    chevron(list, ImVec2(min.x + pad.x + font * 0.4f, middle), font * 0.7f, open, textColor);
    const float textX = min.x + font + (header ? pad.x * 3.0f : pad.x * 2.0f);
    list->AddText(ImVec2(textX, std::floor(middle - font / 2.0f)), textColor, label);
    return open;
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
