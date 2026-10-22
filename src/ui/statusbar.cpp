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
    return static_cast<float>(std::clamp(left / FADE_SECONDS, 0.0, 1.0));
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
    ImVec4 border = toast->error ? ImVec4(0.9f, 0.35f, 0.35f, fade) : theme::rgb(theme::MUTE, fade);
    draw->AddRectFilled(min, max,
                        ImGui::ColorConvertFloat4ToU32(theme::rgb(0x1a1715, 0.95f * fade)),
                        theme::px(4.0f));
    draw->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(border), theme::px(4.0f));
    draw->AddText(ImVec2(min.x + pad, min.y + pad),
                  ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::INK, fade)),
                  toast->text.c_str());
}

std::string status_text(const Editor &editor)
{
    const Sprite &sprite = editor.sprite();
    const Layer *layer = sprite.layer(editor.activeLayer());
    char text[256];
    int len =
        std::snprintf(text, sizeof(text), "%dx%d   %s   Frame %d/%d   Layer: %s   Zoom: %g%%",
                      sprite.width(), sprite.height(), toolLabel(editor.tool()), editor.frame() + 1,
                      sprite.frameCount(), layer ? layer->name().c_str() : "-",
                      static_cast<double>(editor.view().viewport.zoom) * 100.0);
    std::string out = len > 0 ? text : "";
    if (editor.hasSelection()) {
        Rect r = editor.selection();
        std::snprintf(text, sizeof(text), "   Sel %dx%d at %d, %d", r.w, r.h, r.x, r.y);
        out += text;
    }
    if (editor.hoverInSprite()) {
        std::snprintf(text, sizeof(text), "   Cel: %d, %d", editor.hoverX(), editor.hoverY());
        out += text;
    }
    return out;
}

float status_bar_height()
{
    return theme::px(BAR_HEIGHT);
}

void status_bar_show(const Editor &editor, float x, float y, float width)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, status_bar_height()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::px(10.0f), theme::px(4.0f)));
    constexpr ImGuiWindowFlags FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##statusbar", nullptr, FLAGS)) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::rgb(theme::BODY));
        ImGui::TextUnformatted(status_text(editor).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
