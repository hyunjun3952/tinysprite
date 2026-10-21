#include "timeline_panel.h"

#include <algorithm>
#include <string>

#include "icons.h"
#include "theme.h"

namespace
{

constexpr float CELL_WIDTH = 30.0f;
constexpr float CELL_HEIGHT = 22.0f;
constexpr float NAME_COLUMN = 120.0f;
constexpr float DOT_RADIUS = 4.0f;

const char *const MODE_NAMES[] = {"Forward", "Reverse", "Ping-pong"};

} // namespace

TimelinePanel::TimelinePanel(Editor &editor) : editor_(editor)
{
}

/* A cel counts as filled when any pixel is not fully transparent. The scan is repeated only when
 * the document's revision changed. */
void TimelinePanel::refreshFilled()
{
    const Sprite &sprite = editor_.sprite();
    int layers = sprite.layerCount();
    int frames = sprite.frameCount();
    if (filledValid_ && filledRevision_ == editor_.revision() && filledLayers_ == layers &&
        filledFrames_ == frames) {
        return;
    }
    filled_.assign(static_cast<size_t>(layers) * static_cast<size_t>(frames), 0);
    size_t pixels = static_cast<size_t>(sprite.width()) * static_cast<size_t>(sprite.height());
    for (int l = 0; l < layers; l++) {
        for (int f = 0; f < frames; f++) {
            const Rgba *p = sprite.layer(l)->cel(f)->pixels();
            bool any = std::any_of(p, p + pixels, [](const Rgba &c) { return c.a != 0; });
            filled_[static_cast<size_t>(l) * static_cast<size_t>(frames) + static_cast<size_t>(f)] =
                any ? 1 : 0;
        }
    }
    filledLayers_ = layers;
    filledFrames_ = frames;
    filledRevision_ = editor_.revision();
    filledValid_ = true;
}

bool TimelinePanel::filled(int layer, int frame) const
{
    return filled_[static_cast<size_t>(layer) * static_cast<size_t>(filledFrames_) +
                   static_cast<size_t>(frame)] != 0;
}

TimelinePanel::Rect TimelinePanel::cellRect(int layer, int frame) const
{
    if (layer < 0 || frame < 0 || frame >= drawnFrames_) {
        return Rect{};
    }
    size_t index =
        static_cast<size_t>(layer) * static_cast<size_t>(drawnFrames_) + static_cast<size_t>(frame);
    return index < cells_.size() ? cells_[index] : Rect{};
}

TimelinePanel::Rect TimelinePanel::headerRect(int frame) const
{
    return frame >= 0 && static_cast<size_t>(frame) < headers_.size()
               ? headers_[static_cast<size_t>(frame)]
               : Rect{};
}

void TimelinePanel::toolbar()
{
    using icons::Icon;
    const Sprite &sprite = editor_.sprite();
    int current = editor_.frame();
    int count = sprite.frameCount();
    ImVec2 iconSize(30.0f, ImGui::GetFrameHeight());

    if (theme::actionButton(editor_.playing() ? "Stop" : "Play",
                            "Play the animation (Enter); any edit stops it", 54.0f)) {
        editor_.togglePlayback();
    }
    ImGui::SameLine();
    if (theme::actionButton("<", "Previous frame (,)", 26.0f, current > 0)) {
        editor_.setFrame(current - 1);
    }
    ImGui::SameLine();
    if (theme::actionButton(">", "Next frame (.)", 26.0f, current + 1 < count)) {
        editor_.setFrame(current + 1);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d / %d", current + 1, count);
    ImGui::SameLine();
    if (icons::button(Icon::Plus, "##newframe", "New blank frame after this one", iconSize)) {
        editor_.addFrame();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Copy, "##dupframe", "Duplicate this frame", iconSize)) {
        editor_.duplicateFrame();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Trash, "##delframe", "Delete this frame", iconSize, count > 1)) {
        editor_.removeFrame(current);
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Duration");
    ImGui::SameLine();
    int ms = sprite.frameDuration(current);
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("##ms", &ms, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) ||
        ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.setFrameDuration(current, std::clamp(ms, 1, FRAME_DURATION_MAX));
    }
    theme::tip("How long this frame is shown, in milliseconds.");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    int mode = static_cast<int>(editor_.playMode());
    if (ImGui::BeginCombo("##mode", MODE_NAMES[mode])) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::Selectable(MODE_NAMES[i], i == mode)) {
                editor_.setPlayMode(static_cast<PlayMode>(i));
            }
        }
        ImGui::EndCombo();
    }
    theme::tip("Direction of playback.");
}

void TimelinePanel::grid()
{
    const Sprite &sprite = editor_.sprite();
    int frames = sprite.frameCount();
    int layers = sprite.layerCount();
    refreshFilled();
    cells_.assign(static_cast<size_t>(layers) * static_cast<size_t>(frames), Rect{});
    headers_.assign(static_cast<size_t>(frames), Rect{});
    drawnFrames_ = frames;

    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("cels", frames + 1, FLAGS)) {
        return;
    }
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("##layer", ImGuiTableColumnFlags_WidthFixed, NAME_COLUMN);
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetupColumn(("##f" + std::to_string(f)).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, CELL_WIDTH);
    }

    int current = editor_.frame();
    int moveFrom = -1;
    int moveTo = -1;

    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ImGui::TableSetColumnIndex(0);
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetColumnIndex(f + 1);
        ImGui::PushID(f);
        std::string label = std::to_string(f + 1);
        if (ImGui::Selectable(label.c_str(), f == current, 0, ImVec2(CELL_WIDTH - 8.0f, 0.0f))) {
            editor_.setFrame(f);
        }
        headers_[static_cast<size_t>(f)] = Rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("TS_FRAME", &f, sizeof(f));
            ImGui::Text("Frame %d", f + 1);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TS_FRAME")) {
                moveFrom = *static_cast<const int *>(payload->Data);
                moveTo = f;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !ImGui::IsMouseDown(0)) {
            ImGui::SetTooltip("Frame %d: %d ms (drag to move)", f + 1, sprite.frameDuration(f));
        }
        ImGui::PopID();
    }

    for (int l = layers - 1; l >= 0; l--) {
        const Layer &layer = *sprite.layer(l);
        ImGui::TableNextRow(0, CELL_HEIGHT);
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        if (!layer.visible()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::rgb(theme::MUTE));
        }
        ImGui::TextUnformatted(layer.name().c_str());
        if (!layer.visible()) {
            ImGui::PopStyleColor();
        }
        for (int f = 0; f < frames; f++) {
            ImGui::TableSetColumnIndex(f + 1);
            if (f == current) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                       ImGui::GetColorU32(theme::rgb(theme::HOVER)));
            }
            ImGui::PushID(l * frames + f);
            bool active = l == editor_.activeLayer() && f == current;
            if (ImGui::Selectable("##cel", active, 0, ImVec2(CELL_WIDTH - 8.0f, CELL_HEIGHT))) {
                editor_.setFrame(f);
                editor_.setActiveLayer(l);
            }
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            cells_[static_cast<size_t>(l) * static_cast<size_t>(frames) + static_cast<size_t>(f)] =
                Rect{min, max};
            if (filled(l, f)) {
                ImU32 ink =
                    ImGui::GetColorU32(active ? theme::rgb(theme::INK) : theme::rgb(theme::BODY));
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2((min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f), DOT_RADIUS, ink);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndTable();
    if (moveFrom >= 0 && moveFrom != moveTo) {
        editor_.moveFrame(moveFrom, moveTo);
    }
}

void TimelinePanel::show(float x, float y, float width)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, HEIGHT));
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Timeline", nullptr, FLAGS)) {
        ImGui::BeginDisabled(editor_.stroking());
        toolbar();
        grid();
        ImGui::EndDisabled();
    }
    ImGui::End();
}
