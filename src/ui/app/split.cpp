/* App: the split view, two panes of the canvas area each showing a document. */

#include <algorithm>
#include <cmath>
#include <memory>

#include "app.hpp"
#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr float SASH_WIDTH = 5.0f; /* the gap between the panes, which is also the handle */
constexpr float RATIO_MIN = 0.15f; /* the smallest share a pane may keep */
constexpr float RATIO_MAX = 0.85f;

} // namespace

/* Shows two documents at once: the active one stays in the first pane and `rightDocument` (the next
 * tab when it is -1) goes into the second. When the view is split already this only changes the
 * direction, and puts `rightDocument` into the pane that is not active. False when there is no
 * second document or a tool is being dragged. */
bool App::splitView(int rightDocument, bool sideBySide)
{
    const int count = static_cast<int>(docs_.size());
    if (count < 2 || editor_->stroking()) {
        return false;
    }
    if (!split_) {
        int second = rightDocument;
        if (second < 0 || second >= count || second == active_) {
            second = (active_ + 1) % count;
        }
        pane_[0] = editor_;
        pane_[1] = docs_[static_cast<size_t>(second)].editor.get();
        activePane_ = 0;
        split_ = true;
        if (!canvas2_) {
            canvas2_ = std::make_unique<Canvas>(*pane_[1], "Canvas 2");
            canvas2_->setOnPress([this] { focusPane(1); });
        }
    } else if (rightDocument >= 0 && rightDocument < count && rightDocument != active_) {
        pane_[1 - activePane_] = docs_[static_cast<size_t>(rightDocument)].editor.get();
    }
    splitSideBySide_ = sideBySide;
    bindPanes();
    return true;
}

void App::unsplit()
{
    split_ = false;
    pane_[0] = editor_;
    pane_[1] = nullptr;
    activePane_ = 0;
    bindPanes();
}

/* A click in a pane: its document becomes the active one. */
void App::focusPane(int pane)
{
    if (!split_ || pane == activePane_ || pane_[pane] == nullptr) {
        return;
    }
    for (int i = 0; i < static_cast<int>(docs_.size()); i++) {
        if (docs_[static_cast<size_t>(i)].editor.get() == pane_[pane]) {
            activateDocument(i);
            return;
        }
    }
}

/* Points the canvases at the documents of their panes and the panels at the active one. */
void App::bindPanes()
{
    if (!canvas_) {
        return;
    }
    canvas_->setEditor(*pane_[0]);
    canvas_->setHighlighted(split_ && activePane_ == 0);
    if (split_ && canvas2_) {
        canvas2_->setEditor(*pane_[1]);
        canvas2_->setHighlighted(activePane_ == 1);
    }
    toolsPanel_->setEditor(*editor_);
    layersPanel_->setEditor(*editor_);
    timelinePanel_->setEditor(*editor_);
}

/* The canvas, or the two panes and the handle between them, over [x, x + width] x [y, y + height].
 */
void App::canvasArea(float x, float y, float width, float height)
{
    if (!split_) {
        canvas_->show(x, y, width, height);
        return;
    }
    const float gap = theme::px(SASH_WIDTH);
    const float total = (splitSideBySide_ ? width : height) - gap;
    const float first = std::floor(total * splitRatio_);
    if (splitSideBySide_) {
        canvas_->show(x, y, first, height);
        canvas2_->show(x + first + gap, y, total - first, height);
    } else {
        canvas_->show(x, y, width, first);
        canvas2_->show(x, y + first + gap, width, total - first);
    }
    canvas_->setHighlighted(activePane_ == 0);
    canvas2_->setHighlighted(activePane_ == 1);
    splitSash(x, y, width, height, gap);
}

/* The handle was dragged to `offset` pixels from the start of the area. */
void App::dragSash(float offset, float total, float thickness)
{
    splitRatio_ = std::clamp((offset - thickness / 2.0f) / total, RATIO_MIN, RATIO_MAX);
}

/* The handle itself, inside its window: the cursor, the drag, the double click and the fill. */
void App::sashHandle(ImVec2 size, ImVec2 origin, float total, float thickness)
{
    ImGui::InvisibleButton("##sash", size);
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (hovered || active) {
        ImGui::SetMouseCursor(splitSideBySide_ ? ImGuiMouseCursor_ResizeEW
                                               : ImGuiMouseCursor_ResizeNS);
    }
    if (active) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        dragSash(splitSideBySide_ ? mouse.x - origin.x : mouse.y - origin.y, total, thickness);
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        splitRatio_ = 0.5f;
    }
    const unsigned fill = active || hovered ? theme::MUTE : theme::HAIRLINE;
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                              ImGui::ColorConvertFloat4ToU32(theme::rgb(fill)));
}

/* The strip between the panes: drag it to change their sizes, double click to share the area
 * equally. */
void App::splitSash(float x, float y, float width, float height, float thickness)
{
    const float total = (splitSideBySide_ ? width : height) - thickness;
    const float first = std::floor(total * splitRatio_);
    const ImVec2 at = splitSideBySide_ ? ImVec2(x + first, y) : ImVec2(x, y + first);
    const ImVec2 size = splitSideBySide_ ? ImVec2(thickness, height) : ImVec2(width, thickness);
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowSize(size);
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    if (ImGui::Begin("##split_sash", nullptr, FLAGS)) {
        sashHandle(size, ImVec2(x, y), total, thickness);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}
