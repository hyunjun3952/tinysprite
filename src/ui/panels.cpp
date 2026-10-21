#include "panels.h"

#include <algorithm>

#include "icons.h"
#include "imgui.h"

namespace
{

constexpr float TOOLS_PANEL_WIDTH = 250.0f;
constexpr float LAYERS_PANEL_WIDTH = 220.0f;
constexpr float FLOAT_OFFSET = 40.0f;
constexpr float RESIZE_GRAB = 4.0f;
constexpr float PANEL_MIN_WIDTH = 170.0f;
constexpr float PANEL_MAX_WIDTH = 480.0f;
constexpr float DOCK_ZONE =
    60.0f; /* mouse distance from a window edge that docks a dragged panel */
constexpr float TITLE_DRAG_DISTANCE = 4.0f; /* mouse travel that turns a title press into a drag */

/* Translucent blue, ABGR. */
constexpr ImU32 COL32_PANEL_DROP = 0x66d29a4aU;

} // namespace

Panels::Panels()
{
    reset();
}

void Panels::reset()
{
    panels_[static_cast<size_t>(PanelId::Tools)] = {"Tools", TOOLS_PANEL_WIDTH, true,
                                                    DockSide::Left, DockSide::Left};
    panels_[static_cast<size_t>(PanelId::Layers)] = {"Layers", LAYERS_PANEL_WIDTH, true,
                                                     DockSide::Right, DockSide::Right};
}

void Panels::dock(Panel &panel, DockSide side)
{
    panel.side = side;
    panel.visible = true;
    panel.place = side == DockSide::Float;
    if (side != DockSide::Float) {
        panel.home = side;
    }
}

void Panels::layout(float &left, float &right)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    float rightEdge = viewport->WorkPos.x + viewport->WorkSize.x;
    left = 0.0f;
    right = 0.0f;
    for (Panel &panel : panels_) {
        if (panel.visible && panel.side == DockSide::Left) {
            panel.dockX = viewport->WorkPos.x + left;
            left += panel.width;
        }
    }
    /* Right-docked panels keep the same order, so walk them from the outer edge inwards. */
    for (auto it = panels_.rbegin(); it != panels_.rend(); ++it) {
        if (it->visible && it->side == DockSide::Right) {
            right += it->width;
            it->dockX = rightEdge - right;
        }
    }
}

/* While a floating panel's title bar is dragged near a window edge, highlights where it would dock
 * and docks it on release. */
void Panels::dragToDock(Panel &panel)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 mouse = ImGui::GetMousePos();
    ImVec2 pos = ImGui::GetWindowPos();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
        panel.titlePress = ImGui::IsWindowHovered() && mouse.y >= pos.y &&
                           mouse.y < pos.y + ImGui::GetFrameHeight() &&
                           mouse.x < pos.x + ImGui::GetWindowWidth();
    }
    if (!panel.titlePress) {
        return;
    }
    float leftEdge = viewport->WorkPos.x;
    float rightEdge = leftEdge + viewport->WorkSize.x;
    bool nearLeft = mouse.x < leftEdge + DOCK_ZONE;
    bool atEdge = nearLeft || mouse.x > rightEdge - DOCK_ZONE;
    DockSide side = nearLeft ? DockSide::Left : DockSide::Right;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        panel.titlePress = false;
        if (atEdge) {
            dock(panel, side);
        }
    } else if (atEdge && ImGui::IsMouseDragging(ImGuiMouseButton_Left, TITLE_DRAG_DISTANCE)) {
        float x0 = side == DockSide::Left ? leftEdge : rightEdge - panel.width;
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(x0, viewport->WorkPos.y),
            ImVec2(x0 + panel.width, viewport->WorkPos.y + viewport->WorkSize.y), COL32_PANEL_DROP);
    }
}

/* Drag the inner edge of a docked panel to change its width. */
void Panels::resizeEdge(Panel &panel)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 mouse = ImGui::GetMousePos();
    float edge = panel.side == DockSide::Left ? panel.dockX + panel.width : panel.dockX;
    bool inside =
        mouse.y >= viewport->WorkPos.y && mouse.y < viewport->WorkPos.y + viewport->WorkSize.y;
    /* The grab strip lies inside the panel: outside it the canvas would take the click. */
    float grabX0 = panel.side == DockSide::Left ? edge - RESIZE_GRAB : edge;
    bool hovered = inside && mouse.x >= grabX0 && mouse.x <= grabX0 + RESIZE_GRAB;
    if (hovered && !ImGui::IsAnyItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
            panel.resizing = true;
        }
    }
    if (panel.resizing && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        panel.resizing = false;
    }
    if (panel.resizing) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        float width = panel.side == DockSide::Left ? mouse.x - panel.dockX
                                                   : panel.dockX + panel.width - mouse.x;
        panel.width = std::clamp(width, PANEL_MIN_WIDTH, PANEL_MAX_WIDTH);
    }
    if (hovered || panel.resizing) {
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(edge - 1.0f, viewport->WorkPos.y),
            ImVec2(edge + 1.0f, viewport->WorkPos.y + viewport->WorkSize.y), COL32_PANEL_DROP);
    }
}

/* Pop-out / dock icon at the right end of the title bar (left of the close button when floating).
 */
void Panels::titleButton(Panel &panel)
{
    bool floating = panel.side == DockSide::Float;
    float closeButton = floating ? ImGui::GetFrameHeight() : 0.0f;
    if (icons::titleButton(icons::Icon::PopOut, floating ? "Dock panel" : "Float panel",
                           closeButton)) {
        dock(panel, floating ? panel.home : DockSide::Float);
    }
}

bool Panels::begin(PanelId id, float height)
{
    Panel &panel = panels_[static_cast<size_t>(id)];
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!panel.visible) {
        return false;
    }
    if (panel.side != DockSide::Float) {
        ImGui::SetNextWindowPos(ImVec2(panel.dockX, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(panel.width, height));
        bool shown = ImGui::Begin(panel.title, nullptr,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse);
        titleButton(panel);
        resizeEdge(panel);
        return shown;
    }
    if (panel.place) {
        ImGui::SetNextWindowPos(
            ImVec2(panel.dockX + FLOAT_OFFSET, viewport->WorkPos.y + FLOAT_OFFSET));
        panel.place = false;
    }
    ImGui::SetNextWindowSize(ImVec2(panel.width, height * 0.8f), ImGuiCond_FirstUseEver);
    bool open = true;
    bool shown = ImGui::Begin(panel.title, &open);
    titleButton(panel);
    if (!open) {
        panel.visible = false;
    }
    dragToDock(panel);
    return shown;
}

void Panels::end()
{
    ImGui::End();
}

void Panels::windowMenu()
{
    if (!ImGui::BeginMenu("Window")) {
        return;
    }
    for (Panel &panel : panels_) {
        if (ImGui::BeginMenu(panel.title)) {
            ImGui::MenuItem("Show", nullptr, &panel.visible);
            ImGui::Separator();
            if (ImGui::MenuItem("Dock left", nullptr, panel.side == DockSide::Left)) {
                dock(panel, DockSide::Left);
            }
            if (ImGui::MenuItem("Dock right", nullptr, panel.side == DockSide::Right)) {
                dock(panel, DockSide::Right);
            }
            if (ImGui::MenuItem("Float", nullptr, panel.side == DockSide::Float)) {
                dock(panel, DockSide::Float);
            }
            ImGui::EndMenu();
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset layout")) {
        reset();
    }
    ImGui::EndMenu();
}
