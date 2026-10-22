#include "panels.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <string>

#include "imgui.h"
#include "theme.h"

namespace
{

constexpr float TOOLS_PANEL_WIDTH = 250.0f;
constexpr float LAYERS_PANEL_WIDTH = 280.0f;
constexpr float FLOAT_OFFSET = 40.0f;
constexpr float RESIZE_GRAB = 4.0f;
constexpr float PANEL_MIN_WIDTH = 170.0f;
constexpr float PANEL_MAX_WIDTH = 480.0f;
constexpr float DOCK_ZONE =
    60.0f; /* mouse distance from a window edge that docks a dragged panel */
constexpr float TITLE_DRAG_DISTANCE = 4.0f; /* mouse travel that turns a title press into a drag */

/* Translucent off-white, ABGR. */
constexpr ImU32 COL32_PANEL_DROP = 0x66f0f5f7U;

} // namespace

Panels::Panels()
{
    reset();
}

void Panels::reset()
{
    auto make = [](const char *title, float width, DockSide side,
                   std::initializer_list<const char *> sectionNames) {
        Panel panel{};
        panel.title = title;
        panel.width = width;
        panel.side = side;
        panel.home = side;
        for (const char *name : sectionNames) {
            panel.sections.push_back({name, true});
        }
        return panel;
    };
    panels_[static_cast<size_t>(PanelId::Tools)] =
        make("Tools", TOOLS_PANEL_WIDTH, DockSide::Left,
             {sections::TOOLS, sections::SHAPES, sections::TOOL_OPTIONS, sections::SYMMETRY,
              sections::SELECTION, sections::COLOR, sections::RECENT});
    /* Symmetry is occasional: hidden until it is switched on in the Window menu. */
    for (Section &section : panels_[static_cast<size_t>(PanelId::Tools)].sections) {
        if (std::strcmp(section.name, sections::SYMMETRY) == 0) {
            section.visible = false;
        }
    }
    panels_[static_cast<size_t>(PanelId::Layers)] = make(
        "Layers", LAYERS_PANEL_WIDTH, DockSide::Right, {sections::LAYERS, sections::PROPERTIES});
}

bool Panels::sectionVisible(PanelId id, const char *name) const
{
    for (const Section &section : panels_[static_cast<size_t>(id)].sections) {
        if (std::strcmp(section.name, name) == 0) {
            return section.visible;
        }
    }
    return true;
}

void Panels::setSectionVisible(PanelId id, const char *name, bool visible)
{
    for (Section &section : panels_[static_cast<size_t>(id)].sections) {
        if (std::strcmp(section.name, name) == 0) {
            section.visible = visible;
        }
    }
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

/* True when the mouse is over the title bar of the current window. */
bool Panels::titleHovered()
{
    ImVec2 mouse = ImGui::GetMousePos();
    ImVec2 pos = ImGui::GetWindowPos();
    return ImGui::IsWindowHovered() && mouse.y >= pos.y &&
           mouse.y < pos.y + ImGui::GetFrameHeight() && mouse.x < pos.x + ImGui::GetWindowWidth();
}

/* Dragging a docked panel's title bar pulls it out: it floats under the mouse and follows it (see
 * begin) until the button is released, where dragToDock may dock it to an edge again. */
void Panels::undockOnDrag(Panel &panel)
{
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
        panel.titlePress = titleHovered();
    }
    if (!panel.titlePress) {
        return;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        panel.titlePress = false;
    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, TITLE_DRAG_DISTANCE)) {
        ImVec2 mouse = ImGui::GetMousePos();
        ImVec2 pos = ImGui::GetWindowPos();
        panel.grabX = std::clamp(mouse.x - pos.x, 0.0f, panel.width);
        panel.grabY = mouse.y - pos.y;
        dock(panel, DockSide::Float);
        panel.place = false;
        panel.moving = true;
    }
}

/* While a floating panel's title bar is dragged near a window edge, highlights where it would dock
 * and docks it on release. */
void Panels::dragToDock(Panel &panel)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 mouse = ImGui::GetMousePos();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)) {
        panel.titlePress = titleHovered();
    }
    if (!panel.titlePress) {
        return;
    }
    float leftEdge = viewport->WorkPos.x;
    float rightEdge = leftEdge + viewport->WorkSize.x;
    bool nearLeft = mouse.x < leftEdge + theme::px(DOCK_ZONE);
    bool atEdge = nearLeft || mouse.x > rightEdge - theme::px(DOCK_ZONE);
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
    float grabX0 = panel.side == DockSide::Left ? edge - theme::px(RESIZE_GRAB) : edge;
    bool hovered = inside && mouse.x >= grabX0 && mouse.x <= grabX0 + theme::px(RESIZE_GRAB);
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
        panel.width = std::clamp(width, theme::px(PANEL_MIN_WIDTH), theme::px(PANEL_MAX_WIDTH));
    }
    if (hovered || panel.resizing) {
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(edge - 1.0f, viewport->WorkPos.y),
            ImVec2(edge + 1.0f, viewport->WorkPos.y + viewport->WorkSize.y), COL32_PANEL_DROP);
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
        windowOpen_ = true;
        bool shown = ImGui::Begin(panel.title, nullptr,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse);
        undockOnDrag(panel);
        resizeEdge(panel);
        return shown;
    }
    if (panel.moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        panel.moving = false;
    }
    if (panel.moving) {
        ImVec2 mouse = ImGui::GetMousePos();
        ImGui::SetNextWindowPos(ImVec2(mouse.x - panel.grabX, mouse.y - panel.grabY));
    } else if (panel.place) {
        ImGui::SetNextWindowPos(ImVec2(panel.dockX + theme::px(FLOAT_OFFSET),
                                       viewport->WorkPos.y + theme::px(FLOAT_OFFSET)));
        panel.place = false;
    }
    ImGui::SetNextWindowSize(ImVec2(panel.width, height * 0.8f), ImGuiCond_FirstUseEver);
    bool open = true;
    windowOpen_ = true;
    bool shown = ImGui::Begin(panel.title, &open);
    if (!open) {
        panel.visible = false;
    }
    dragToDock(panel);
    return shown;
}

namespace
{

std::string keyPrefix(const char *title)
{
    std::string key = "panel.";
    for (const char *c = title; *c; c++) {
        key += static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
    }
    return key + ".";
}

} // namespace

void Panels::store(SettingsMap &settings) const
{
    for (const Panel &panel : panels_) {
        std::string prefix = keyPrefix(panel.title);
        settings_set_bool(settings, prefix + "visible", panel.visible);
        settings_set_int(settings, prefix + "side", static_cast<int>(panel.side));
        settings_set_int(settings, prefix + "home", static_cast<int>(panel.home));
        settings_set_float(settings, prefix + "width", panel.width);
        for (const Section &section : panel.sections) {
            settings_set_bool(settings, prefix + "section." + section.name, section.visible);
        }
    }
}

void Panels::load(const SettingsMap &settings)
{
    for (Panel &panel : panels_) {
        std::string prefix = keyPrefix(panel.title);
        settings_get_bool(settings, prefix + "visible", panel.visible);
        int side = 0;
        if (settings_get_int(settings, prefix + "side", 0, 2, side)) {
            panel.side = static_cast<DockSide>(side);
            panel.place = panel.side == DockSide::Float; /* floats near where it docks */
        }
        int home = 0;
        if (settings_get_int(settings, prefix + "home", 0, 1, home)) {
            panel.home = static_cast<DockSide>(home);
        }
        settings_get_float(settings, prefix + "width", theme::px(PANEL_MIN_WIDTH),
                           theme::px(PANEL_MAX_WIDTH), panel.width);
        for (Section &section : panel.sections) {
            settings_get_bool(settings, prefix + "section." + section.name, section.visible);
        }
    }
}

void Panels::rescale(float ratio)
{
    for (Panel &panel : panels_) {
        panel.width =
            std::clamp(panel.width * ratio, theme::px(PANEL_MIN_WIDTH), theme::px(PANEL_MAX_WIDTH));
    }
}

bool Panels::visible(PanelId id) const
{
    return panels_[static_cast<size_t>(id)].visible;
}

void Panels::setVisible(PanelId id, bool visible)
{
    panels_[static_cast<size_t>(id)].visible = visible;
}

bool Panels::floating(PanelId id) const
{
    return panels_[static_cast<size_t>(id)].side == DockSide::Float;
}

void Panels::end()
{
    if (windowOpen_) {
        ImGui::End();
        windowOpen_ = false;
    }
}

/* One panel's submenu: visibility, where it docks, and which of its sections show. */
void Panels::panelMenu(Panel &panel)
{
    if (!ImGui::BeginMenu(panel.title)) {
        return;
    }
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
    ImGui::Separator();
    ImGui::TextDisabled("Sections");
    theme::tip("Choose which sections of this panel are shown.");
    /* Stay open while the sections are toggled one after another. */
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    for (Section &section : panel.sections) {
        ImGui::MenuItem(section.name, nullptr, &section.visible);
    }
    if (ImGui::MenuItem("Show all")) {
        for (Section &section : panel.sections) {
            section.visible = true;
        }
    }
    ImGui::PopItemFlag();
    ImGui::EndMenu();
}

void Panels::windowMenu()
{
    if (!ImGui::BeginMenu("Window")) {
        return;
    }
    for (Panel &panel : panels_) {
        panelMenu(panel);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset layout and sections")) {
        reset();
    }
    ImGui::EndMenu();
}
