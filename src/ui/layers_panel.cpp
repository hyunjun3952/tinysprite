#include "layers_panel.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

#include "icons.h"
#include "imgui.h"
#include "theme.h"

namespace
{

constexpr float OPACITY_COLUMN = 42.0f;
constexpr float EDGE_MARGIN = 8.0f; /* keeps the eye and the lock off the panel edges */
constexpr int THUMB_WIDTH = 36;     /* the box a layer's thumbnail is fitted into */
constexpr int THUMB_HEIGHT = 24;
constexpr float THUMB_COLUMN = static_cast<float>(THUMB_WIDTH) + 6.0f;
constexpr int THUMB_CHECKER = 4;
constexpr float DROP_LINE_THICKNESS = 2.0f;
constexpr float ACTIVE_BAR = 3.0f;

/* Moves the cursor down so an item of `itemHeight` sits in the middle of a row `rowHeight` tall. */
void centerInRow(float rowHeight, float itemHeight)
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowHeight - itemHeight) / 2.0f);
}

/* The line that shows where a dragged layer would land, across the row it is over. The list is top
 * layer first, so a layer coming from below ends above this row and one coming from above ends
 * below it. */
void drawDropLine(ImVec2 rowMin, ImVec2 rowMax, bool fromBelow)
{
    float y = fromBelow ? rowMin.y : rowMax.y;
    ImGui::GetForegroundDrawList()->AddLine(ImVec2(rowMin.x, y), ImVec2(rowMax.x, y),
                                            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::INK)),
                                            DROP_LINE_THICKNESS);
}

} // namespace

LayersPanel::LayersPanel(Editor &editor) : editor_(editor), thumbs_(editor.renderer())
{
}

/* The name field that replaces a row's name while it is renamed: the layer shows the text live,
 * and the edit is recorded as one undo step when the field loses focus (Enter, a click elsewhere;
 * Escape puts the old name back first, so nothing is recorded). */
void LayersPanel::renameField(int index, Layer &layer)
{
    char name[LAYER_NAME_MAX + 1];
    std::snprintf(name, sizeof(name), "%s", layer.name().c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (renameFocus_) {
        ImGui::SetKeyboardFocusHere();
        renameFocus_ = false;
    }
    if (ImGui::InputText("##rename", name, sizeof(name), ImGuiInputTextFlags_AutoSelectAll)) {
        layer.setName(name);
    }
    if (ImGui::IsItemDeactivated()) {
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            editor_.commitLayerName(index, renameBefore_);
        }
        renaming_ = -1;
    }
}

/* What is painted on the layer, over a checkerboard so transparency shows, in the middle of its
 * cell (dimmed while the layer is hidden). */
void LayersPanel::thumbnail(const Layer &layer, float rowHeight)
{
    LayerThumbnails::Thumb thumb =
        thumbs_.get(layer, editor_.frame(), THUMB_WIDTH, THUMB_HEIGHT, editor_.revision());
    ImVec2 cell = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(THUMB_COLUMN, rowHeight));
    if (!thumb.texture) {
        return;
    }
    ImVec2 min(std::floor(cell.x + (static_cast<float>(THUMB_WIDTH - thumb.width)) / 2.0f),
               std::floor(cell.y + (rowHeight - static_cast<float>(thumb.height)) / 2.0f));
    ImVec2 max(min.x + static_cast<float>(thumb.width), min.y + static_cast<float>(thumb.height));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    float alpha = layer.visible() ? 1.0f : 0.4f;
    draw->PushClipRect(min, max, true);
    draw->AddRectFilled(min, max, IM_COL32(170, 170, 170, 255));
    for (int y = 0; y < thumb.height; y += THUMB_CHECKER) {
        for (int x = (y / THUMB_CHECKER) % 2 == 0 ? THUMB_CHECKER : 0; x < thumb.width;
             x += 2 * THUMB_CHECKER) {
            ImVec2 p(min.x + static_cast<float>(x), min.y + static_cast<float>(y));
            draw->AddRectFilled(p,
                                ImVec2(p.x + static_cast<float>(THUMB_CHECKER),
                                       p.y + static_cast<float>(THUMB_CHECKER)),
                                IM_COL32(120, 120, 120, 255));
        }
    }
    draw->PopClipRect();
    if (!layer.visible()) {
        /* Fade the checkerboard too: cover it with the panel color. */
        draw->AddRectFilled(min, max,
                            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::CANVAS, 0.6f)));
    }
    draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(thumb.texture)), min, max,
                   ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                   IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
    draw->AddRect(ImVec2(min.x - 1.0f, min.y - 1.0f), ImVec2(max.x + 1.0f, max.y + 1.0f),
                  ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::HAIRLINE)));
}

/* One row per layer, top layer first: a visibility checkbox, the name (dimmed while hidden), the
 * opacity when it is not full, and the lock toggle. The whole row selects the layer; the checkbox
 * and the toggle sit on top of it. */
void LayersPanel::rows()
{
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("layers", 5, FLAGS)) {
        return;
    }
    if (renaming_ >= editor_.sprite().layerCount()) {
        renaming_ = -1; /* the layer is gone (undo, delete) */
    }
    thumbs_.prune(editor_.sprite());
    const float frameHeight = ImGui::GetFrameHeight();
    const float rowHeight = static_cast<float>(THUMB_HEIGHT) + 8.0f;
    ImGui::TableSetupColumn("##visible", ImGuiTableColumnFlags_WidthFixed,
                            frameHeight + EDGE_MARGIN);
    ImGui::TableSetupColumn("##thumbnail", ImGuiTableColumnFlags_WidthFixed, THUMB_COLUMN);
    ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##opacity", ImGuiTableColumnFlags_WidthFixed, OPACITY_COLUMN);
    ImGui::TableSetupColumn("##lock", ImGuiTableColumnFlags_WidthFixed, frameHeight + EDGE_MARGIN);
    const bool multiple = editor_.selectedLayers().size() > 1;
    int moveFrom = -1; /* a drop is applied after the loop: the list must not change under it */
    int moveTo = -1;
    for (int i = editor_.sprite().layerCount() - 1; i >= 0; i--) {
        Layer *layer = editor_.sprite().layer(i);
        ImGui::PushID(i);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable("##row", editor_.layerSelected(i),
                              ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowOverlap,
                              ImVec2(0.0f, rowHeight))) {
            /* Shift selects a range, Ctrl adds or removes one layer, neither selects only this. */
            const ImGuiIO &io = ImGui::GetIO();
            editor_.pickLayer(i, io.KeyShift  ? LayerPick::Range
                                 : io.KeyCtrl ? LayerPick::Toggle
                                              : LayerPick::Only);
        }
        if (multiple && i == editor_.activeLayer()) {
            /* With several rows selected, a bar marks the one the tools draw on. */
            ImGui::GetForegroundDrawList()->AddRectFilled(
                ImGui::GetItemRectMin(),
                ImVec2(ImGui::GetItemRectMin().x + ACTIVE_BAR, ImGui::GetItemRectMax().y),
                ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::INK)));
        }
        /* Drag a row onto another to move the layer there. */
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("LAYER_ROW", &i, sizeof(i));
            if (multiple && editor_.layerSelected(i)) {
                ImGui::Text("%d layers", static_cast<int>(editor_.selectedLayers().size()));
            } else {
                ImGui::TextUnformatted(layer->name().c_str());
            }
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            /* Looked at while hovering (before the drop), to draw where the layer would land. */
            constexpr ImGuiDragDropFlags FLAGS = ImGuiDragDropFlags_AcceptBeforeDelivery |
                                                 ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("LAYER_ROW", FLAGS)) {
                int from = *static_cast<const int *>(payload->Data);
                /* Dropping a selection onto one of its own layers moves nothing. */
                bool onItself =
                    from == i || (editor_.layerSelected(from) && editor_.layerSelected(i));
                if (!onItself) {
                    drawDropLine(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), from < i);
                    if (payload->IsDelivery()) {
                        moveFrom = from;
                        moveTo = i;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        char hint[LAYER_NAME_MAX + 128];
        std::snprintf(hint, sizeof(hint),
                      "%s\nClick to make this the active layer (the one the tools draw on).\n"
                      "Double-click to rename it.",
                      layer->name().c_str());
        theme::tip(hint);
        /* A double click renames, unless Ctrl / Shift are held: those clicks change the selection,
         * and two quick ones on the same row must not collapse it. */
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
            editor_.setActiveLayer(i);
            renaming_ = i;
            renameBefore_ = layer->name();
            renameFocus_ = true;
        }
        ImGui::SameLine(0.0f, EDGE_MARGIN);
        centerInRow(rowHeight, frameHeight);
        if (icons::toggle(icons::Icon::Eye, icons::Icon::EyeOff, "##visible",
                          layer->visible() ? "Visible. Click to hide the layer."
                                           : "Hidden. Click to show the layer (tools cannot draw "
                                             "on a hidden layer).",
                          layer->visible(), frameHeight)) {
            bool visible = !layer->visible();
            if (multiple && editor_.layerSelected(i)) {
                editor_.setSelectedLayersVisible(visible);
            } else {
                editor_.setLayerVisible(i, visible);
            }
        }

        ImGui::TableSetColumnIndex(1);
        thumbnail(*layer, rowHeight);

        ImGui::TableSetColumnIndex(2);
        centerInRow(rowHeight, frameHeight);
        if (renaming_ == i) {
            renameField(i, *layer);
        } else {
            ImGui::AlignTextToFramePadding();
            const char *name = layer->name().c_str();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  theme::rgb(layer->visible() ? theme::INK : theme::MUTE));
            ImGui::TextUnformatted(name[0] ? name : "(unnamed)");
            ImGui::PopStyleColor();
        }

        ImGui::TableSetColumnIndex(3);
        if (layer->opacity() != 255) {
            centerInRow(rowHeight, frameHeight);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%d%%", (layer->opacity() * 100 + 127) / 255);
        }

        ImGui::TableSetColumnIndex(4);
        centerInRow(rowHeight, frameHeight);
        if (icons::toggle(icons::Icon::Lock, icons::Icon::Unlock, "##lock",
                          layer->locked() ? "Locked: tools cannot change it. Click to unlock."
                                          : "Unlocked. Click to lock the layer (it stays visible, "
                                            "tools cannot change it).",
                          layer->locked(), frameHeight)) {
            bool locked = !layer->locked();
            if (multiple && editor_.layerSelected(i)) {
                editor_.setSelectedLayersLocked(locked);
            } else {
                editor_.setLayerLocked(i, locked);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
    if (moveFrom >= 0) {
        bool wasSelected = editor_.layerSelected(moveFrom);
        editor_.moveSelectedLayers(moveFrom, moveTo);
        if (!wasSelected) {
            editor_.pickLayer(moveTo,
                              LayerPick::Only); /* the dragged layer becomes the selection */
        }
    }
}

/* The opacity edits the layer live while the slider is active and records one undo step when it is
 * released, so dragging it is not one step per frame. (The name is edited in the list.) */
void LayersPanel::activeLayerDetails()
{
    int index = editor_.activeLayer();
    Layer *layer = editor_.sprite().layer(index);
    if (!layer) {
        return;
    }
    const uint8_t opacityNow = layer->opacity();
    int opacity = opacityNow;
    if (ImGui::SliderInt("Opacity", &opacity, 0, 255)) {
        layer->setOpacity(static_cast<uint8_t>(opacity));
        editor_.markAllDirty();
    }
    theme::tip("How much the layer shows through: 0 is invisible, 255 is fully opaque.");
    if (ImGui::IsItemActivated()) {
        opacityBefore_ = opacityNow;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitLayerOpacity(index, opacityBefore_);
    }
}

/* Layer actions under the list: one icon button each, evenly spread (the tooltips name them).
 * Layers are reordered by dragging their rows. */
void LayersPanel::buttons()
{
    using icons::Icon;
    constexpr int COUNT = 4;
    ImVec2 size(theme::columnWidth(COUNT), ImGui::GetFrameHeight() + 6.0f);
    int count = editor_.sprite().layerCount();
    int selected = static_cast<int>(editor_.selectedLayers().size());

    if (icons::button(Icon::Plus, "##new", "New layer on top", size)) {
        editor_.addLayer();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Copy, "##copy", "Duplicate the selected layers", size)) {
        editor_.duplicateSelectedLayers();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Trash, "##delete", "Delete the selected layers (Delete key)", size,
                      count - selected >= 1)) {
        askToDelete();
    }
    ImGui::SameLine();
    if (icons::button(Icon::Merge, "##merge",
                      selected > 1
                          ? "Merge the selected layers into the lowest one (all visible, the "
                            "lowest unlocked)"
                          : "Merge into the layer below (both visible, the lower one "
                            "unlocked)",
                      size, editor_.canMergeLayers())) {
        editor_.mergeSelectedLayers();
    }
}

bool LayersPanel::focused() const
{
    return focusFrame_ == ImGui::GetFrameCount();
}

void LayersPanel::askToDelete()
{
    std::vector<int> selected = editor_.selectedLayers();
    if (static_cast<int>(selected.size()) >= editor_.sprite().layerCount() || editor_.stroking()) {
        return; /* at least one layer must stay */
    }
    deleteCount_ = static_cast<int>(selected.size());
    deleteName_ = editor_.sprite().layer(editor_.activeLayer())->name();
    openDelete_ = true;
}

/* "Delete layer?": Enter or the Delete button removes it (still undoable), Escape or Cancel keeps
 * it. */
void LayersPanel::deleteDialog()
{
    constexpr const char *TITLE = "Delete layer";
    if (openDelete_) {
        ImGui::OpenPopup(TITLE);
        openDelete_ = false;
    }
    dialogOpen_ = ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (!dialogOpen_) {
        return;
    }
    if (deleteCount_ > 1) {
        ImGui::Text("Delete these %d layers?", deleteCount_);
    } else {
        ImGui::Text("Delete the layer \"%s\"?", deleteName_.c_str());
    }
    ImGui::TextDisabled("Ctrl+Z brings it back.");
    bool close = false;
    if (ImGui::Button("Delete") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        editor_.removeSelectedLayers();
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void LayersPanel::show(const Panels &panels)
{
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        focusFrame_ = ImGui::GetFrameCount();
    }
    /* Layers cannot change under a tool that is mid-drag. */
    ImGui::BeginDisabled(editor_.stroking());
    auto open = [&panels](const char *name) {
        return panels.sectionVisible(PanelId::Layers, name) && theme::section(name);
    };
    if (open(sections::LAYERS)) {
        rows();
        buttons();
    }
    if (open(sections::PROPERTIES)) {
        activeLayerDetails();
    }
    ImGui::EndDisabled();
    deleteDialog();
}
