#include "theme.h"

#include <algorithm>

namespace theme
{

namespace
{

constexpr float ROUND_SMALL = 3.0f; /* buttons, inputs */
constexpr float ROUND_CARD = 4.0f;  /* popups, child areas */

ImU32 toU32(unsigned hex, float alpha = 1.0f)
{
    return ImGui::ColorConvertFloat4ToU32(rgb(hex, alpha));
}

} // namespace

void apply()
{
    ImGuiStyle &style = ImGui::GetStyle();
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
    style.FrameBorderSize = 1.0f;
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
    c[ImGuiCol_PopupBg] = rgb(CANVAS_SOFT);
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
    c[ImGuiCol_CheckMark] = rgb(INK);
    c[ImGuiCol_SliderGrab] = rgb(BODY);
    c[ImGuiCol_SliderGrabActive] = rgb(INK);
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
    c[ImGuiCol_DragDropTarget] = rgb(INK);
    c[ImGuiCol_NavCursor] = rgb(INK, 0.7f);
    c[ImGuiCol_NavWindowingHighlight] = rgb(INK, 0.7f);
    c[ImGuiCol_NavWindowingDimBg] = rgb(0x000000, 0.4f);
    c[ImGuiCol_ModalWindowDimBg] = rgb(0x000000, 0.5f);
}

bool section(const char *label, bool openByDefault)
{
    ImGuiTreeNodeFlags flags = openByDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    return ImGui::CollapsingHeader(label, flags);
}

float columnWidth(int count)
{
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float width = (ImGui::GetContentRegionAvail().x - spacing * static_cast<float>(count - 1)) /
                  static_cast<float>(count);
    return std::max(width, 1.0f);
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
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(INK));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(INK));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(BODY_STRONG));
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(CANVAS));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::PopStyleVar();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    if (hint && hint[0]) {
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

ImU32 workspaceColor()
{
    return toU32(WORKSPACE);
}

} // namespace theme
