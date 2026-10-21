#include "timeline_panel.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
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

/* Muted band colors for the tags, cycled by index (ABGR). */
constexpr ImU32 TAG_COLORS[] = {0xff5a78c8U, 0xff6aa860U, 0xffc08a50U,
                                0xff5aaac0U, 0xffa069b8U, 0xff6e8ec0U};

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

TimelinePanel::Rect TimelinePanel::tagCellRect(int frame) const
{
    return frame >= 0 && static_cast<size_t>(frame) < tagCells_.size()
               ? tagCells_[static_cast<size_t>(frame)]
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

/* The second toolbar row: the onion skin switch and how many ghosts it shows. */
void TimelinePanel::onionControls()
{
    bool on = editor_.onionEnabled();
    OnionSkin skin = editor_.onionSkin();
    bool changed = ImGui::Checkbox("Onion skin", &on);
    theme::tip("Show the neighbouring frames faintly under this one (not while playing).");
    ImGui::BeginDisabled(!on);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    changed |= ImGui::DragInt("Before", &skin.before, 0.1f, 0, ONION_MAX_FRAMES);
    theme::tip("Previous frames shown, tinted red.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    changed |= ImGui::DragInt("After", &skin.after, 0.1f, 0, ONION_MAX_FRAMES);
    theme::tip("Following frames shown, tinted blue.");
    ImGui::SameLine();
    int percent = (skin.opacity * 100 + 127) / 255;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::SliderInt("Strength", &percent, 5, 100, "%d%%")) {
        skin.opacity = static_cast<uint8_t>((percent * 255 + 50) / 100);
        changed = true;
    }
    theme::tip("How strongly the nearest ghost shows; farther ones fade out.");
    ImGui::EndDisabled();
    if (changed) {
        editor_.setOnion(on, skin);
    }
}

/* The third toolbar row: a button that tags the current frame and, for the selected tag, its
 * name, range and direction. */
void TimelinePanel::tagControls()
{
    const Sprite &sprite = editor_.sprite();
    int frames = sprite.frameCount();
    if (theme::actionButton("+ Tag", "Tag the current frame; then widen it with From / To.",
                            54.0f)) {
        editor_.addTag();
    }
    int selected = editor_.selectedTag();
    if (selected < 0 || selected >= static_cast<int>(sprite.tags().size())) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Click a tag band to edit it or to play just its frames.");
        tagNameActive_ = false;
        return;
    }
    Sprite::Tag tag = sprite.tags()[static_cast<size_t>(selected)];
    bool changed = false;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (!tagNameActive_) {
        std::snprintf(tagName_, sizeof(tagName_), "%s", tag.name.c_str());
    }
    if (ImGui::InputText("##tagname", tagName_, sizeof(tagName_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        changed = true;
    }
    tagNameActive_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        changed = true;
    }
    if (changed) {
        tag.name = tagName_;
    }
    theme::tip("The tag's name.");

    auto frameField = [&](const char *id, int &value) {
        int shown = value + 1;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        bool edited = ImGui::InputInt(id, &shown, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) ||
                      ImGui::IsItemDeactivatedAfterEdit();
        if (edited) {
            value = std::clamp(shown, 1, frames) - 1;
        }
        return edited;
    };
    if (frameField("From##tag", tag.from)) {
        tag.to = std::max(tag.to, tag.from);
        changed = true;
    }
    if (frameField("To##tag", tag.to)) {
        tag.from = std::min(tag.from, tag.to);
        changed = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    int mode = static_cast<int>(tag.direction);
    if (ImGui::BeginCombo("##tagdir", MODE_NAMES[mode])) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::Selectable(MODE_NAMES[i], i == mode)) {
                tag.direction = static_cast<PlayMode>(i);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    theme::tip("The direction this tag plays in.");

    ImGui::SameLine();
    if (theme::actionButton("Delete tag", "Remove the tag (the frames stay).", 80.0f)) {
        editor_.removeTag(selected);
        return;
    }
    if (changed) {
        editor_.editTag(selected, tag);
    }
}

/* The tag row of the grid: a band over the frames of each tag, its name on the first frame. */
void TimelinePanel::tagRow(int frames)
{
    const Sprite &sprite = editor_.sprite();
    const std::vector<Sprite::Tag> &tags = sprite.tags();
    tagCells_.assign(static_cast<size_t>(frames), Rect{});
    ImGui::TableNextRow(0, CELL_HEIGHT - 4.0f);
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Tags");
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetColumnIndex(f + 1);
        /* the selected tag wins where tags overlap, else the first one */
        int shown = -1;
        for (int t = 0; t < static_cast<int>(tags.size()); t++) {
            const Sprite::Tag &tag = tags[static_cast<size_t>(t)];
            if (f >= tag.from && f <= tag.to && (shown < 0 || t == editor_.selectedTag())) {
                shown = t;
            }
        }
        ImGui::PushID(f);
        float gap = 2.0f * ImGui::GetStyle().CellPadding.x; /* between two columns */
        ImVec2 size(CELL_WIDTH, CELL_HEIGHT - 4.0f);
        ImVec2 min = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##tag", size) && shown >= 0) {
            editor_.selectTag(shown == editor_.selectedTag() ? -1 : shown);
        }
        ImVec2 max = ImGui::GetItemRectMax();
        tagCells_[static_cast<size_t>(f)] = Rect{min, max};
        if (shown >= 0) {
            const Sprite::Tag &tag = tags[static_cast<size_t>(shown)];
            ImU32 color = TAG_COLORS[static_cast<size_t>(shown) % std::size(TAG_COLORS)];
            bool selected = shown == editor_.selectedTag();
            ImDrawList *draw = ImGui::GetWindowDrawList();
            /* extend the band over the column gap so it reads as one piece */
            float right = max.x + (f < tag.to ? gap : 0.0f);
            draw->AddRectFilled(min, ImVec2(right, max.y), color);
            if (selected) {
                draw->AddRect(min, ImVec2(right, max.y),
                              ImGui::GetColorU32(theme::rgb(theme::INK)));
            }
            if (f == tag.from) {
                float width = static_cast<float>(tag.to - tag.from) * (CELL_WIDTH + gap);
                draw->PushClipRect(min, ImVec2(max.x + width, max.y), true);
                draw->AddText(ImVec2(min.x + 3.0f, min.y + 1.0f), 0xff1f1b19U, tag.name.c_str());
                draw->PopClipRect();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s: frames %d-%d", tag.name.c_str(), tag.from + 1, tag.to + 1);
            }
        }
        ImGui::PopID();
    }
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
    ImGui::TableSetupScrollFreeze(1, 2);
    ImGui::TableSetupColumn("##layer", ImGuiTableColumnFlags_WidthFixed, NAME_COLUMN);
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetupColumn(("##f" + std::to_string(f)).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, CELL_WIDTH);
    }

    int current = editor_.frame();
    tagRow(frames);
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
            if (ImGui::BeginPopupContextItem("##celmenu")) {
                if (ImGui::MenuItem("Link to previous frame", nullptr, false, f > 0)) {
                    editor_.linkCel(l, f - 1, f);
                }
                if (ImGui::MenuItem("Link to next frame", nullptr, false, f + 1 < frames)) {
                    editor_.linkCel(l, f + 1, f);
                }
                if (ImGui::MenuItem("Unlink", nullptr, false, layer.celLinked(f))) {
                    editor_.unlinkCel(l, f);
                }
                ImGui::EndPopup();
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
            if (layer.celLinked(f)) {
                /* a bar under the cel: it shares its pixels with another frame */
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(min.x + 3.0f, max.y - 4.0f), ImVec2(max.x - 3.0f, max.y - 2.0f),
                    ImGui::GetColorU32(theme::rgb(theme::INK)));
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
        onionControls();
        tagControls();
        grid();
        ImGui::EndDisabled();
    }
    ImGui::End();
}
