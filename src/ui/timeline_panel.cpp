#include "timeline_panel.hpp"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

#include "icons.hpp"
#include "theme.hpp"

namespace
{

/* Base sizes at UI scale 1; the cellWidth() family gives them at the current scale. */
constexpr float CELL_WIDTH_BASE = 30.0f;
constexpr float CELL_HEIGHT_BASE = 22.0f;
constexpr float NAME_COLUMN_BASE = 120.0f;
constexpr float DOT_RADIUS_BASE = 4.0f;

float cellWidth()
{
    return theme::px(CELL_WIDTH_BASE);
}
float cellHeight()
{
    return theme::px(CELL_HEIGHT_BASE);
}
float nameColumn()
{
    return theme::px(NAME_COLUMN_BASE);
}
float dotRadius()
{
    return theme::px(DOT_RADIUS_BASE);
}

const char *const MODE_NAMES[] = {"Forward", "Reverse", "Ping-pong"};

/* A combo of the play modes; true when `mode` was changed. */
bool modeCombo(const char *id, PlayMode &mode, float width)
{
    ImGui::SetNextItemWidth(width);
    bool changed = false;
    int current = static_cast<int>(mode);
    if (theme::begin_combo(id, MODE_NAMES[current])) {
        for (int i = 0; i < static_cast<int>(std::size(MODE_NAMES)); i++) {
            if (ImGui::Selectable(MODE_NAMES[i], i == current)) {
                mode = static_cast<PlayMode>(i);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

/* The tag whose band shows on `frame`: the selected tag wins where tags overlap, else the first
 * one; -1 for a frame outside every tag. */
int tagShownAt(const std::vector<Sprite::Tag> &tags, int frame, int selected)
{
    int shown = -1;
    for (int t = 0; t < static_cast<int>(tags.size()); t++) {
        const Sprite::Tag &tag = tags[static_cast<size_t>(t)];
        if (frame >= tag.from && frame <= tag.to && (shown < 0 || t == selected)) {
            shown = t;
        }
    }
    return shown;
}

} // namespace

void TimelinePanel::setEditor(Editor &editor)
{
    editor_ = &editor;
    filledValid_ = false;
    tagNameActive_ = false;
    cells_.clear();
    headers_.clear();
    tagCells_.clear();
    drawnFrames_ = 0;
}

TimelinePanel::TimelinePanel(Editor &editor) : editor_(&editor)
{
}

/* A cel counts as filled when any pixel is not fully transparent. The scan is repeated only when
 * the document's revision changed. */
void TimelinePanel::refreshFilled()
{
    const Sprite &sprite = editor_->sprite();
    int layers = sprite.layerCount();
    int frames = sprite.frameCount();
    if (filledValid_ && filledRevision_ == editor_->revision() && filledLayers_ == layers &&
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
    filledRevision_ = editor_->revision();
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
    const Sprite &sprite = editor_->sprite();
    int current = editor_->frame();
    int count = sprite.frameCount();
    ImVec2 iconSize(theme::px(30.0f), ImGui::GetFrameHeight());

    if (theme::actionButton(editor_->playing() ? "Stop" : "Play",
                            "Play the animation (Enter); any edit stops it", theme::px(54.0f))) {
        editor_->togglePlayback();
    }
    ImGui::SameLine();
    if (theme::actionButton("<", "Previous frame (,)", theme::px(26.0f), current > 0)) {
        editor_->setFrame(current - 1);
    }
    ImGui::SameLine();
    if (theme::actionButton(">", "Next frame (.)", theme::px(26.0f), current + 1 < count)) {
        editor_->setFrame(current + 1);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d / %d", current + 1, count);
    ImGui::SameLine();
    if (icons::button(Icon::Plus, "##newframe", "New blank frame after this one", iconSize)) {
        editor_->addFrame();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Copy, "##dupframe", "Duplicate this frame", iconSize)) {
        editor_->duplicateFrame();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Trash, "##delframe", "Delete this frame", iconSize, count > 1)) {
        editor_->removeFrame(current);
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Duration");
    ImGui::SameLine();
    int ms = sprite.frameDuration(current);
    ImGui::SetNextItemWidth(theme::px(88.0f));
    if (theme::input_int("##ms", &ms, 0, 0, " ms", ImGuiInputTextFlags_EnterReturnsTrue) ||
        ImGui::IsItemDeactivatedAfterEdit()) {
        editor_->setFrameDuration(current, std::clamp(ms, 1, FRAME_DURATION_MAX));
    }
    if (ImGui::IsItemActivated()) {
        /* the field belongs to the frame shown now: keep it from moving on */
        editor_->stopPlayback();
    }
    theme::tip("How long this frame is shown, in milliseconds.");

    ImGui::SameLine();
    PlayMode mode = editor_->playMode();
    if (modeCombo("##mode", mode, theme::px(100.0f))) {
        editor_->setPlayMode(mode);
    }
    theme::tip("Direction of playback.");
}

/* The tooltip of an onion field. While onion skin is off the field is greyed out: it then shows
 * the "not allowed" cursor and says why, so it does not look broken. */
static void onionTip(const char *text, bool on)
{
    if (on) {
        theme::tip(text);
    } else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
        ImGui::SetTooltip("Turn on Onion skin to change this.");
    }
}

/* The second toolbar row: the onion skin switch and how many ghosts it shows. */
void TimelinePanel::onionControls()
{
    bool on = editor_->onionEnabled();
    OnionSkin skin = editor_->onionSkin();
    bool changed = ImGui::Checkbox("Onion skin", &on);
    theme::tip("Show the neighbouring frames faintly under this one (not while playing).");
    ImGui::BeginDisabled(!on);
    /* Each label stands before its field, so the reading order matches the layout. */
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Before");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(78.0f));
    changed |= ImGui::DragInt("##before", &skin.before, 0.1f, 0, ONION_MAX_FRAMES,
                              theme::int_format(skin.before == 1 ? " frame" : " frames").c_str());
    onionTip("Previous frames shown, tinted red.", on);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("After");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(78.0f));
    changed |= ImGui::DragInt("##after", &skin.after, 0.1f, 0, ONION_MAX_FRAMES,
                              theme::int_format(skin.after == 1 ? " frame" : " frames").c_str());
    onionTip("Following frames shown, tinted blue.", on);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Strength");
    ImGui::SameLine();
    int percent = (skin.opacity * 100 + 127) / 255;
    ImGui::SetNextItemWidth(theme::px(110.0f));
    if (theme::slider_int("##strength", &percent, 5, 100, "%")) {
        skin.opacity = static_cast<uint8_t>((percent * 255 + 50) / 100);
        changed = true;
    }
    onionTip("How strongly the nearest ghost shows; farther ones fade out.", on);
    ImGui::EndDisabled();
    if (changed) {
        editor_->setOnion(on, skin);
    }
}

/* The tag's name field; true when an edit was committed (Enter or leaving the field). */
bool TimelinePanel::tagNameField(Sprite::Tag &tag)
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(120.0f));
    if (!tagNameActive_) {
        std::snprintf(tagName_, sizeof(tagName_), "%s", tag.name.c_str());
    }
    bool entered = ImGui::InputText("##tagname", tagName_, sizeof(tagName_),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    tagNameActive_ = ImGui::IsItemActive();
    bool committed = entered || ImGui::IsItemDeactivatedAfterEdit();
    if (committed) {
        tag.name = tagName_;
    }
    theme::tip("The tag's name.");
    return committed;
}

/* The third toolbar row: a button that tags the current frame and, for the selected tag, its
 * name, range and direction. */
void TimelinePanel::tagControls()
{
    const Sprite &sprite = editor_->sprite();
    int frames = sprite.frameCount();
    if (theme::actionButton("+ Tag", "Tag the current frame; then widen it with From / To.",
                            theme::px(54.0f))) {
        editor_->addTag();
    }
    int selected = editor_->selectedTag();
    if (selected < 0 || selected >= static_cast<int>(sprite.tags().size())) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Click a tag band to edit it or to play just its frames");
        tagNameActive_ = false;
        return;
    }
    Sprite::Tag tag = sprite.tags()[static_cast<size_t>(selected)];
    bool changed = false;

    changed |= tagNameField(tag);

    auto frameField = [&](const char *id, int &value) {
        int shown = value + 1;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(theme::px(60.0f));
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
    changed |= modeCombo("##tagdir", tag.direction, theme::px(100.0f));
    theme::tip("The direction this tag plays in.");

    ImGui::SameLine();
    if (theme::actionButton("Delete tag", "Remove the tag (the frames stay).", theme::px(80.0f))) {
        editor_->removeTag(selected);
        return;
    }
    if (changed) {
        editor_->editTag(selected, tag);
    }
}

/* One frame's slice of tag `shown`: the band, its outline when selected, the name on the tag's
 * first frame and a tooltip. `min` / `max` is the frame's cell. */
void TimelinePanel::tagBand(int frame, int shown, ImVec2 min, ImVec2 max)
{
    const Sprite::Tag &tag = editor_->sprite().tags()[static_cast<size_t>(shown)];
    ImU32 color = theme::TAG_COLORS[static_cast<size_t>(shown) % std::size(theme::TAG_COLORS)];
    float gap = 2.0f * ImGui::GetStyle().CellPadding.x; /* between two columns */
    ImDrawList *draw = ImGui::GetWindowDrawList();
    /* extend the band over the column gap so it reads as one piece */
    float right = max.x + (frame < tag.to ? gap : 0.0f);
    draw->AddRectFilled(min, ImVec2(right, max.y), color);
    if (shown == editor_->selectedTag()) {
        draw->AddRect(min, ImVec2(right, max.y), ImGui::GetColorU32(theme::rgb(theme::INK)));
    }
    if (frame == tag.from) {
        /* the table clips drawing to the cell, so the name has to fit the first one */
        draw->PushClipRect(min, max, true);
        std::string label = theme::ellipsize(tag.name, max.x - min.x - 3.0f);
        draw->AddText(ImVec2(min.x + 3.0f, min.y + 1.0f), theme::TAG_TEXT, label.c_str());
        draw->PopClipRect();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s: frames %d-%d", tag.name.c_str(), tag.from + 1, tag.to + 1);
    }
}

/* The tag row of the grid: a band over the frames of each tag, its name on the first frame. */
void TimelinePanel::tagRow(int frames)
{
    const std::vector<Sprite::Tag> &tags = editor_->sprite().tags();
    tagCells_.assign(static_cast<size_t>(frames), Rect{});
    ImGui::TableNextRow(0, cellHeight() - theme::px(4.0f));
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Tags");
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetColumnIndex(f + 1);
        int shown = tagShownAt(tags, f, editor_->selectedTag());
        ImGui::PushID(f);
        ImVec2 min = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##tag", ImVec2(cellWidth(), cellHeight() - theme::px(4.0f))) &&
            shown >= 0) {
            editor_->selectTag(shown == editor_->selectedTag() ? -1 : shown);
        }
        ImVec2 max = ImGui::GetItemRectMax();
        tagCells_[static_cast<size_t>(f)] = Rect{min, max};
        if (shown >= 0) {
            tagBand(f, shown, min, max);
        }
        ImGui::PopID();
    }
}

/* The frame number as a drag source, and as a target a frame can be dropped on. */
void TimelinePanel::frameDragDrop(int f, FrameMove &move)
{
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("TS_FRAME", &f, sizeof(f));
        ImGui::Text("Frame %d", f + 1);
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TS_FRAME")) {
            move = FrameMove{*static_cast<const int *>(payload->Data), f};
        }
        ImGui::EndDragDropTarget();
    }
}

/* The frame number row: click selects the frame, dragging a number onto another moves the frame
 * (the pair is left in `move` for the caller to apply once the table is closed). */
void TimelinePanel::frameHeaders(int frames, FrameMove &move)
{
    const Sprite &sprite = editor_->sprite();
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ImGui::TableSetColumnIndex(0);
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetColumnIndex(f + 1);
        ImGui::PushID(f);
        if (ImGui::Selectable(std::to_string(f + 1).c_str(), f == editor_->frame(), 0,
                              ImVec2(cellWidth() - theme::px(8.0f), 0.0f))) {
            editor_->setFrame(f);
        }
        headers_[static_cast<size_t>(f)] = Rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
        frameDragDrop(f, move);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !ImGui::IsMouseDown(0)) {
            ImGui::SetTooltip("Frame %d: %d ms (drag to move)", f + 1, sprite.frameDuration(f));
        }
        ImGui::PopID();
    }
}

/* The right-click menu of the cel of `layer` at frame `f`. */
void TimelinePanel::celMenu(int layer, int f)
{
    if (!ImGui::BeginPopupContextItem("##celmenu")) {
        return;
    }
    if (ImGui::MenuItem("Link to previous frame", nullptr, false, f > 0)) {
        editor_->linkCel(layer, f - 1, f);
    }
    if (ImGui::MenuItem("Link to next frame", nullptr, false,
                        f + 1 < editor_->sprite().frameCount())) {
        editor_->linkCel(layer, f + 1, f);
    }
    if (ImGui::MenuItem("Unlink", nullptr, false, editor_->sprite().layer(layer)->celLinked(f))) {
        editor_->unlinkCel(layer, f);
    }
    ImGui::EndPopup();
}

/* One layer's row: its name, then a cel per frame. */
void TimelinePanel::layerRow(int l, int frames)
{
    const Layer &layer = *editor_->sprite().layer(l);
    int current = editor_->frame();
    ImGui::TableNextRow(0, cellHeight());
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    std::string shown = theme::ellipsize(layer.name(), ImGui::GetContentRegionAvail().x);
    if (layer.visible()) {
        ImGui::TextUnformatted(shown.c_str());
    } else {
        ImGui::TextColored(theme::rgb(theme::MUTE), "%s", shown.c_str());
    }
    if (shown != layer.name()) {
        theme::tip(layer.name().c_str());
    }
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetColumnIndex(f + 1);
        if (f == current) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                   ImGui::GetColorU32(theme::rgb(theme::HOVER)));
        }
        ImGui::PushID(l * frames + f);
        bool active = l == editor_->activeLayer() && f == current;
        if (ImGui::Selectable("##cel", active, 0,
                              ImVec2(cellWidth() - theme::px(8.0f), cellHeight()))) {
            editor_->setFrame(f);
            editor_->setActiveLayer(l);
        }
        celMenu(l, f);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        cells_[static_cast<size_t>(l) * static_cast<size_t>(frames) + static_cast<size_t>(f)] =
            Rect{min, max};
        celMarks(l, f, active, Rect{min, max});
        ImGui::PopID();
    }
}

/* What a cel shows: a dot when something is painted on it, a bar when it shares its pixels with
 * another frame. */
void TimelinePanel::celMarks(int l, int f, bool active, Rect cell)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    if (filled(l, f)) {
        ImU32 ink = ImGui::GetColorU32(active ? theme::rgb(theme::INK) : theme::rgb(theme::BODY));
        draw->AddCircleFilled(
            ImVec2((cell.min.x + cell.max.x) / 2.0f, (cell.min.y + cell.max.y) / 2.0f), dotRadius(),
            ink);
    }
    if (editor_->sprite().layer(l)->celLinked(f)) {
        draw->AddRectFilled(ImVec2(cell.min.x + 3.0f, cell.max.y - 4.0f),
                            ImVec2(cell.max.x - 3.0f, cell.max.y - 2.0f),
                            ImGui::GetColorU32(theme::rgb(theme::INK)));
    }
}

void TimelinePanel::grid()
{
    const Sprite &sprite = editor_->sprite();
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
    ImGui::TableSetupColumn("##layer", ImGuiTableColumnFlags_WidthFixed, nameColumn());
    for (int f = 0; f < frames; f++) {
        ImGui::TableSetupColumn(("##f" + std::to_string(f)).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, cellWidth());
    }

    FrameMove move;
    tagRow(frames);
    frameHeaders(frames, move);
    for (int l = layers - 1; l >= 0; l--) {
        layerRow(l, frames);
    }
    ImGui::EndTable();
    if (move.from >= 0 && move.from != move.to) {
        editor_->moveFrame(move.from, move.to);
    }
}

TimelineSize timeline_drag(float requested, float previous, float minHeight, float maxHeight,
                           float hideBelow)
{
    if (requested < hideBelow) {
        return {false, previous};
    }
    return {true, std::clamp(requested, minHeight, std::max(minHeight, maxHeight))};
}

void TimelinePanel::show(float x, float y, float width, float height)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    /* NoFocusOnAppearing: the strip appears under the mouse while its edge is being dragged, and a
     * window that takes the focus then ends that drag. */
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_HorizontalScrollbar; /* a narrow window */
    if (ImGui::Begin("Timeline", nullptr, FLAGS)) {
        ImGui::BeginDisabled(editor_->stroking());
        toolbar();
        onionControls();
        tagControls();
        grid();
        ImGui::EndDisabled();
    }
    ImGui::End();
}
