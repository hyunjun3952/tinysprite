#include "statusbar.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "imgui.h"
#include "theme.hpp"

namespace
{

const char *toolLabel(ToolKind tool)
{
    switch (tool) {
    case ToolKind::Pencil:
        return "Pencil";
    case ToolKind::Eraser:
        return "Eraser";
    case ToolKind::Bucket:
        return "Bucket";
    case ToolKind::Eyedropper:
        return "Eyedropper";
    case ToolKind::Line:
        return "Line";
    case ToolKind::Rect:
        return "Rectangle";
    case ToolKind::Ellipse:
        return "Ellipse";
    case ToolKind::Select:
        return "Select";
    case ToolKind::Triangle:
        return "Triangle";
    case ToolKind::Diamond:
        return "Diamond";
    case ToolKind::Hexagon:
        return "Hexagon";
    case ToolKind::Star:
        return "Star";
    case ToolKind::Cross:
        return "Cross";
    case ToolKind::Slice:
        return "Slice";
    case ToolKind::Pan:
        return "Pan";
    case ToolKind::Wand:
        return "Magic wand";
    case ToolKind::Lasso:
        return "Lasso";
    case ToolKind::Count:
        break;
    }
    return "";
}

constexpr float BAR_HEIGHT = 24.0f;

} // namespace

void Toasts::push(std::string text, bool error, double now)
{
    toast_ = Toast{std::move(text), error, now};
    has_ = true;
}

double Toasts::lifetime(const Toast &toast)
{
    return toast.error ? ERROR_SECONDS : INFO_SECONDS;
}

const Toasts::Toast *Toasts::current(double now) const
{
    return has_ && now - toast_.start < lifetime(toast_) ? &toast_ : nullptr;
}

float Toasts::alpha(double now) const
{
    const Toast *toast = current(now);
    if (!toast) {
        return 0.0f;
    }
    double left = toast->start + lifetime(*toast) - now;
    double entered = std::clamp((now - toast->start) / ENTER_SECONDS, 0.0, 1.0);
    return static_cast<float>(std::min(std::clamp(left / FADE_SECONDS, 0.0, 1.0), entered));
}

void Toasts::draw(double now, float bottomMargin) const
{
    const Toast *toast = current(now);
    if (!toast) {
        return;
    }
    float fade = alpha(now);
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    ImVec2 size = ImGui::CalcTextSize(toast->text.c_str());
    float pad = theme::px(10.0f);
    ImVec2 min(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) / 2.0f - pad,
               viewport->WorkPos.y + viewport->WorkSize.y - bottomMargin - size.y - pad * 2.0f);
    ImVec2 max(min.x + size.x + pad * 2.0f, min.y + size.y + pad * 2.0f);
    ImVec4 border = theme::rgb(toast->error ? theme::INK : theme::MUTE, fade);
    draw->AddRectFilled(min, max,
                        ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::CANVAS, 0.95f * fade)),
                        theme::px(6.0f));
    draw->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(border), theme::px(6.0f));
    draw->AddText(ImVec2(min.x + pad, min.y + pad),
                  ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::INK, fade)),
                  toast->text.c_str());
}

const char *status_item_name(StatusItem item)
{
    switch (item) {
    case StatusItem::Tool:
        return "Tool";
    case StatusItem::Frame:
        return "Frame";
    case StatusItem::Layer:
        return "Layer";
    case StatusItem::Zoom:
        return "Zoom";
    case StatusItem::Selection:
        return "Selection";
    case StatusItem::Cell:
    case StatusItem::Count:
        break;
    }
    return "Cel under the mouse";
}

std::string status_item_text(const Editor &editor, StatusItem item)
{
    const Sprite &sprite = editor.sprite();
    char text[160];
    switch (item) {
    case StatusItem::Tool:
        std::snprintf(text, sizeof(text), "%s", toolLabel(editor.tool()));
        break;
    case StatusItem::Frame:
        std::snprintf(text, sizeof(text), "Frame %d/%d", editor.frame() + 1, sprite.frameCount());
        break;
    case StatusItem::Layer: {
        const Layer *layer = sprite.layer(editor.activeLayer());
        std::snprintf(text, sizeof(text), "Layer: %s", layer ? layer->name().c_str() : "-");
        break;
    }
    case StatusItem::Zoom:
        std::snprintf(text, sizeof(text), "Zoom: %g%%",
                      static_cast<double>(editor.view().viewport.zoom) * 100.0);
        break;
    case StatusItem::Selection: {
        if (!editor.hasSelection()) {
            return "";
        }
        Rect r = editor.selection();
        std::snprintf(text, sizeof(text), "Sel %d\xC3\x97%d px at %d, %d", r.w, r.h, r.x, r.y);
        break;
    }
    case StatusItem::Cell:
        if (!editor.hoverInSprite()) {
            return "";
        }
        std::snprintf(text, sizeof(text), "Cel: %d, %d", editor.hoverX(), editor.hoverY());
        break;
    case StatusItem::Count:
        return "";
    }
    return text;
}

/* The text of an item, or "" when `items` leaves it out or it has nothing to say. */
static std::string shownText(const Editor &editor, unsigned items, StatusItem item)
{
    return (items & status_item_bit(item)) != 0 ? status_item_text(editor, item) : "";
}

std::string status_text(const Editor &editor, unsigned items)
{
    std::string out;
    for (unsigned i = 0; i < static_cast<unsigned>(StatusItem::Count); i++) {
        std::string text = shownText(editor, items, static_cast<StatusItem>(i));
        if (!text.empty()) {
            out += out.empty() ? text : "   " + text;
        }
    }
    return out;
}

float status_bar_height()
{
    return theme::px(BAR_HEIGHT);
}

namespace
{

constexpr float ITEM_GAP = 16.0f;

/* The layer item never takes more than this at scale 1; a longer name is cut with "...", so the
 * items after it keep their place. */
constexpr float LAYER_ITEM_MAX_WIDTH = 220.0f;

/* The items, each its own text so that a right click can tell which one it was on. Returns the
 * item that was right-clicked, or -1. */
int drawItems(const Editor &editor, unsigned items)
{
    int clicked = -1;
    bool first = true;
    for (unsigned i = 0; i < static_cast<unsigned>(StatusItem::Count); i++) {
        auto item = static_cast<StatusItem>(i);
        std::string text = shownText(editor, items, item);
        if (text.empty()) {
            continue;
        }
        if (!first) {
            ImGui::SameLine(0.0f, theme::px(ITEM_GAP));
        }
        first = false;
        if (item == StatusItem::Layer) {
            text = theme::ellipsize(text, theme::px(LAYER_ITEM_MAX_WIDTH)); /* a long layer name */
        }
        ImGui::TextUnformatted(text.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            clicked = static_cast<int>(i);
        }
    }
    return clicked;
}

/* What the menu was opened on: an item's index, or -1 for the bar itself. */
constexpr const char *MENU_ID = "##status_menu";

void drawMenu(Editor &editor, int item)
{
    Editor::Overlays &overlays = editor.overlays();
    if (item >= 0) {
        std::string label = std::string("Hide ") + status_item_name(static_cast<StatusItem>(item));
        if (ImGui::MenuItem(label.c_str())) {
            overlays.status_items &= ~status_item_bit(static_cast<StatusItem>(item));
        }
        ImGui::Separator();
    }
    for (unsigned i = 0; i < static_cast<unsigned>(StatusItem::Count); i++) {
        auto entry = static_cast<StatusItem>(i);
        bool on = (overlays.status_items & status_item_bit(entry)) != 0;
        if (ImGui::MenuItem(status_item_name(entry), nullptr, &on)) {
            overlays.status_items ^= status_item_bit(entry);
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Hide status bar")) {
        overlays.status = false;
    }
}

} // namespace

void status_bar_show(Editor &editor, float x, float y, float width)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, status_bar_height()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::px(10.0f), theme::px(4.0f)));
    constexpr ImGuiWindowFlags FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##statusbar", nullptr, FLAGS)) {
        ImGuiStorage *storage = ImGui::GetStateStorage();
        const ImGuiID menuItem = ImGui::GetID("menu_item");
        ImGui::PushStyleColor(ImGuiCol_Text, theme::rgb(theme::BODY));
        int clicked = drawItems(editor, editor.overlays().status_items);
        ImGui::PopStyleColor();
        /* a right click on an item, or else on the bar anywhere (the gaps, the empty end) */
        if (clicked >= 0 ||
            (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
            storage->SetInt(menuItem, clicked);
            ImGui::OpenPopup(MENU_ID);
        }
        if (ImGui::BeginPopup(MENU_ID)) {
            drawMenu(editor, storage->GetInt(menuItem, -1));
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
