#include "layers_panel.hpp"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

#include "icons.hpp"
#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr float OPACITY_COLUMN = 76.0f;
constexpr float PROPERTY_NAME_COLUMN = 72.0f; /* the names in the properties grid */
constexpr float LIST_MIN_HEIGHT = 96.0f;      /* the layer list never gets shorter than this */

/* What each BlendMode does, for the tooltips of the Blend combo and the layer rows. */
constexpr const char *BLEND_HINTS[BLEND_MODE_COUNT] = {
    "Covers the layers under it.",
    "Darkens: colors multiply with the ones under. White changes nothing. Good for shadows.",
    "Lightens: the opposite of Multiply. Black changes nothing.",
    "Multiply on dark backdrops, Screen on light ones: raises contrast.",
    "Keeps the darker of each color channel.",
    "Keeps the lighter of each color channel.",
    "Adds the colors up (linear dodge). Good for glows and light.",
    "Subtracts the colors: the bigger minus the smaller channel.",
};

/* A three letter tag for the layer rows (Normal shows none). */
constexpr const char *BLEND_TAGS[BLEND_MODE_COUNT] = {"",    "Mul", "Scr", "Ovr",
                                                      "Drk", "Lgt", "Add", "Dif"};
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
    ImGui::GetForegroundDrawList()->AddLine(
        ImVec2(rowMin.x, y), ImVec2(rowMax.x, y),
        ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::ACCENT)), DROP_LINE_THICKNESS);
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

/* Selects on click (Shift: a range, Ctrl: add or remove one layer, neither: only this one), and
 * with several rows selected marks the one the tools draw on with a bar. The row is a Selectable
 * that spans all columns; the other cells sit on top of it. */
void LayersPanel::rowSelectable(int index, const RowContext &ctx)
{
    if (ImGui::Selectable("##row", editor_.layerSelected(index),
                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                          ImVec2(0.0f, ctx.rowHeight))) {
        const ImGuiIO &io = ImGui::GetIO();
        LayerPick pick = LayerPick::Only;
        if (io.KeyShift) {
            pick = LayerPick::Range;
        } else if (io.KeyCtrl) {
            pick = LayerPick::Toggle;
        }
        editor_.pickLayer(index, pick);
    }
    /* A right click acts on the row under it, as in a file manager: it becomes the selection unless
     * it is already part of it (then the actions apply to the whole selection). */
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !editor_.layerSelected(index)) {
        editor_.pickLayer(index, LayerPick::Only);
    }
    if (ctx.multiple && index == editor_.activeLayer()) {
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImVec2(ImGui::GetItemRectMin().x + ACTIVE_BAR, ImGui::GetItemRectMax().y),
            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::ACCENT)));
    }
}

/* Drag a row onto another to move the layer there. The drop is only recorded in `drop`: the list
 * must not change while it is being drawn. */
void LayersPanel::rowDragDrop(int index, const Layer &layer, const RowContext &ctx, Drop &drop)
{
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("LAYER_ROW", &index, sizeof(index));
        if (wholeSelection(index, ctx)) {
            ImGui::Text("%d layers", static_cast<int>(editor_.selectedLayers().size()));
        } else {
            ImGui::TextUnformatted(layer.name().c_str());
        }
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        rowDropTarget(index, drop);
        ImGui::EndDragDropTarget();
    }
}

/* The row as a drop target: draws where the dragged layer would land and reports the drop. */
void LayersPanel::rowDropTarget(int index, Drop &drop)
{
    /* Looked at while hovering (before the drop), to draw where the layer would land. */
    constexpr ImGuiDragDropFlags FLAGS =
        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("LAYER_ROW", FLAGS)) {
        int from = *static_cast<const int *>(payload->Data);
        /* Dropping a selection onto one of its own layers moves nothing. */
        bool onItself =
            from == index || (editor_.layerSelected(from) && editor_.layerSelected(index));
        if (!onItself) {
            drawDropLine(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), from < index);
            if (payload->IsDelivery()) {
                drop = {from, index};
            }
        }
    }
}

/* The row's tooltip, and the double click that starts renaming. */
void LayersPanel::rowHintAndRename(int index, const Layer &layer)
{
    char hint[LAYER_NAME_MAX + 192];
    char blend[48] = "";
    if (layer.blendMode() != BlendMode::Normal) {
        std::snprintf(blend, sizeof(blend), "Blend: %s\n", blend_mode_name(layer.blendMode()));
    }
    std::snprintf(hint, sizeof(hint),
                  "%s\n%sClick to make this the active layer (the one the tools draw on).\n"
                  "Double-click to rename it.",
                  layer.name().c_str(), blend);
    theme::tip(hint);
    /* A double click renames, unless Ctrl / Shift are held: those clicks change the selection,
     * and two quick ones on the same row must not collapse it. */
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
        editor_.setActiveLayer(index);
        renaming_ = index;
        renameBefore_ = layer.name();
        renameFocus_ = true;
    }
}

/* Whether a toggle on this row applies to every selected layer: the row is part of a
 * multi-selection. */
bool LayersPanel::wholeSelection(int index, const RowContext &ctx) const
{
    return ctx.multiple && editor_.layerSelected(index);
}

/* The eye toggle. With the row inside a multi-selection it applies to the whole selection. */
void LayersPanel::visibilityToggle(int index, const Layer &layer, const RowContext &ctx)
{
    centerInRow(ctx.rowHeight, ctx.frameHeight);
    if (!icons::toggle(icons::Icon::Eye, icons::Icon::EyeOff, "##visible",
                       layer.visible() ? "Visible. Click to hide the layer."
                                       : "Hidden. Click to show the layer (tools cannot draw "
                                         "on a hidden layer).",
                       layer.visible(), ctx.frameHeight)) {
        return;
    }
    bool visible = !layer.visible();
    if (wholeSelection(index, ctx)) {
        editor_.setSelectedLayersVisible(visible);
    } else {
        editor_.setLayerVisible(index, visible);
    }
}

/* The lock toggle, applied to the whole selection like the eye. */
void LayersPanel::lockToggle(int index, const Layer &layer, const RowContext &ctx)
{
    centerInRow(ctx.rowHeight, ctx.frameHeight);
    if (!icons::toggle(icons::Icon::Lock, icons::Icon::Unlock, "##lock",
                       layer.locked() ? "Locked: tools cannot change it. Click to unlock."
                                      : "Unlocked. Click to lock the layer (it stays visible, "
                                        "tools cannot change it).",
                       layer.locked(), ctx.frameHeight)) {
        return;
    }
    bool locked = !layer.locked();
    if (wholeSelection(index, ctx)) {
        editor_.setSelectedLayersLocked(locked);
    } else {
        editor_.setLayerLocked(index, locked);
    }
}

/* The name (dimmed while hidden), or its text field while it is being renamed. */
void LayersPanel::nameCell(int index, Layer &layer, const RowContext &ctx)
{
    centerInRow(ctx.rowHeight, ctx.frameHeight);
    if (renaming_ == index) {
        renameField(index, layer);
        return;
    }
    ImGui::AlignTextToFramePadding();
    const char *name = layer.name().c_str();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::rgb(layer.visible() ? theme::INK : theme::MUTE));
    ImGui::TextUnformatted(name[0] != '\0' ? name : "(unnamed)");
    ImGui::PopStyleColor();
}

void LayersPanel::row(int index, const RowContext &ctx, Drop &drop)
{
    Layer &layer = *editor_.sprite().layer(index);
    ImGui::PushID(index);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, ctx.rowHeight);
    ImGui::TableSetColumnIndex(0);
    rowSelectable(index, ctx);
    rowDragDrop(index, layer, ctx, drop);
    rowHintAndRename(index, layer);
    ImGui::SameLine(0.0f, EDGE_MARGIN);
    visibilityToggle(index, layer, ctx);

    ImGui::TableSetColumnIndex(1);
    thumbnail(layer, ctx.rowHeight);

    ImGui::TableSetColumnIndex(2);
    nameCell(index, layer, ctx);

    ImGui::TableSetColumnIndex(3);
    if (layer.opacity() != 255 || layer.blendMode() != BlendMode::Normal) {
        centerInRow(ctx.rowHeight, ctx.frameHeight);
        ImGui::AlignTextToFramePadding();
        if (layer.blendMode() != BlendMode::Normal) {
            ImGui::TextDisabled("%s", BLEND_TAGS[static_cast<int>(layer.blendMode())]);
            ImGui::SameLine(0.0f, 4.0f);
        }
        if (layer.opacity() != 255) {
            ImGui::TextDisabled("%d%%", (layer.opacity() * 100 + 127) / 255);
        }
    }

    ImGui::TableSetColumnIndex(4);
    lockToggle(index, layer, ctx);
    ImGui::PopID();
}

/* One row per layer, top layer first: a visibility checkbox, the thumbnail, the name (dimmed while
 * hidden), the opacity when it is not full, and the lock toggle. The whole row selects the layer;
 * the checkbox and the toggle sit on top of it. */
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
    RowContext ctx;
    ctx.frameHeight = ImGui::GetFrameHeight();
    ctx.rowHeight = static_cast<float>(THUMB_HEIGHT) + 8.0f;
    ctx.multiple = editor_.selectedLayers().size() > 1;
    ImGui::TableSetupColumn("##visible", ImGuiTableColumnFlags_WidthFixed,
                            ctx.frameHeight + EDGE_MARGIN);
    ImGui::TableSetupColumn("##thumbnail", ImGuiTableColumnFlags_WidthFixed, THUMB_COLUMN);
    ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##opacity", ImGuiTableColumnFlags_WidthFixed, OPACITY_COLUMN);
    ImGui::TableSetupColumn("##lock", ImGuiTableColumnFlags_WidthFixed,
                            ctx.frameHeight + EDGE_MARGIN);
    Drop drop; /* applied after the loop: the list must not change under it */
    for (int i = editor_.sprite().layerCount() - 1; i >= 0; i--) {
        row(i, ctx, drop);
    }
    ImGui::EndTable();
    if (drop.from >= 0) {
        bool wasSelected = editor_.layerSelected(drop.from);
        editor_.moveSelectedLayers(drop.from, drop.to);
        if (!wasSelected) {
            /* the dragged layer becomes the selection */
            editor_.pickLayer(drop.to, LayerPick::Only);
        }
    }
}

/* The blend mode of the selected layers. The combo shows the active layer's mode, or "Mixed" when
 * the selection holds different ones; picking a mode sets it on all of them as one undo step. */
void LayersPanel::propertyBlend(const Layer &layer)
{
    bool mixed = false;
    for (int i : editor_.selectedLayers()) {
        mixed = mixed || editor_.sprite().layer(i)->blendMode() != layer.blendMode();
    }
    propertyLabel("Blend");
    if (theme::begin_combo("##blend", mixed ? "Mixed" : blend_mode_name(layer.blendMode()))) {
        for (int m = 0; m < BLEND_MODE_COUNT; m++) {
            auto mode = static_cast<BlendMode>(m);
            if (ImGui::Selectable(blend_mode_name(mode), !mixed && mode == layer.blendMode())) {
                editor_.setSelectedLayersBlendMode(mode);
            }
            theme::tip(BLEND_HINTS[m]);
        }
        ImGui::EndCombo();
    }
    theme::tip("How the layer's colors combine with the layers under it. Normal just covers them.");
}

/* The opacity edits the layer live while the slider is active and records one undo step when it is
 * released, so dragging it is not one step per frame. (The name is edited in the list.) */
void LayersPanel::propertyOpacity(const Layer &layer)
{
    const uint8_t opacityNow = layer.opacity();
    /* read before the slider can change anything: the drag may start on this very frame */
    std::vector<std::pair<int, uint8_t>> selectedNow = editor_.selectedLayerOpacities();
    /* A slider that fills the value column and shows a percentage. */
    int percent = (opacityNow * 100 + 127) / 255;
    propertyLabel("Opacity");
    if (theme::slider_int("##opacity", &percent, 0, 100, "%")) {
        editor_.setSelectedLayersOpacityLive(static_cast<uint8_t>((percent * 255 + 50) / 100));
    }
    theme::tip("How much the layer shows through: 0% is invisible, 100% is fully opaque.");
    if (ImGui::IsItemActivated()) {
        opacityBefore_ = std::move(selectedNow);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitSelectedLayersOpacity(opacityBefore_);
    }
}

/* One row of the properties grid: the name in the left column, and the widget that follows fills
 * the right one. */
void LayersPanel::propertyLabel(const char *name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", name);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

/* A row with a name and a read-only value. */
void LayersPanel::propertyValue(const char *name, const std::string &value)
{
    propertyLabel(name);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value.c_str());
}

/* Facts about the sprite that cannot be edited here: its size, its frames (and how many cels of
 * this layer are shared with another frame) and the palette. */
void LayersPanel::propertyInfo(const Layer &layer)
{
    const Sprite &sprite = editor_.sprite();
    const int frames = sprite.frameCount();
    int linked = 0;
    for (int f = 0; f < frames; f++) {
        if (layer.celLinked(f)) {
            linked++;
        }
    }
    propertyValue("Size",
                  std::to_string(sprite.width()) + " x " + std::to_string(sprite.height()) + " px");
    propertyValue("Frames", std::to_string(frames) + "  (" + std::to_string(linked) + " linked)");
    theme::tip("A linked cel is shared with another frame: drawing on one changes them all.");
    propertyValue("Palette", std::to_string(sprite.palette().count()) + " colors");
}

/* The grid of the section: the layer's blend and opacity, then the facts about the sprite. */
void LayersPanel::properties()
{
    const Layer *layer = editor_.sprite().layer(editor_.activeLayer());
    if (!layer) {
        return;
    }
    if (editor_.selectedLayers().size() > 1) {
        ImGui::TextDisabled("%d layers selected: opacity and blend apply to all",
                            static_cast<int>(editor_.selectedLayers().size()));
    }
    constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##properties", 2, FLAGS)) {
        return;
    }
    ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed,
                            theme::px(PROPERTY_NAME_COLUMN));
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
    propertyBlend(*layer);
    propertyOpacity(*layer);
    propertyInfo(*layer);
    ImGui::EndTable();
}

bool LayersPanel::focused() const
{
    return focusFrame_ == ImGui::GetFrameCount();
}

void LayersPanel::askToDelete()
{
    if (!editor_.canRemoveSelectedLayers()) {
        return; /* at least one layer must stay */
    }
    deleteCount_ = static_cast<int>(editor_.selectedLayers().size());
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
    dialogOpen_ = theme::begin_modal(TITLE);
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
    const int clicked = theme::dialog_buttons({"Cancel", "Delete"}, 1, true);
    if (clicked == 1 || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        editor_.removeSelectedLayers();
        close = true;
    }
    if (clicked == 0 || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    theme::end_modal();
}

/* The actions have no buttons: say where they are, in the room under the last row. */
void LayersPanel::emptyListHint()
{
    const char *text = "Right-click for layer actions";
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float y = ImGui::GetWindowHeight() - line * 1.6f;
    if (y < ImGui::GetCursorPosY() + line) {
        return; /* the rows fill the list */
    }
    const float width = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - width) / 2.0f, y));
    ImGui::TextDisabled("%s", text);
}

/* The rows in a scrolling area that takes all the height the properties below leave, so the list
 * grows with the window and the properties sit at the bottom of the panel. */
void LayersPanel::list()
{
    const float height =
        std::max(theme::px(LIST_MIN_HEIGHT), ImGui::GetContentRegionAvail().y - below_);
    /* A recessed area, so the room under the last row reads as part of the list. */
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::rgb(theme::WORKSPACE, 0.55f));
    if (ImGui::BeginChild("##layer_list", ImVec2(0.0f, height), ImGuiChildFlags_None)) {
        rows();
        emptyListHint();
        if (contextMenu_ &&
            ImGui::BeginPopupContextWindow("##layer_menu", ImGuiPopupFlags_MouseButtonRight)) {
            contextMenu_();
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void LayersPanel::show(const Panels &panels)
{
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        focusFrame_ = ImGui::GetFrameCount();
    }
    /* Layers cannot change under a tool that is mid-drag. */
    ImGui::BeginDisabled(editor_.stroking());
    float belowStart = 0.0f;
    if (panels.sectionVisible(PanelId::Layers, sections::LAYERS) &&
        theme::section(sections::LAYERS)) {
        list();
        belowStart = ImGui::GetCursorPosY();
    }
    if (panels.sectionVisible(PanelId::Layers, sections::PROPERTIES) &&
        theme::section(sections::PROPERTIES)) {
        properties();
    }
    if (belowStart > 0.0f) {
        /* what sat under the list this frame (the properties); the next frame sizes the list by it
         */
        below_ = ImGui::GetCursorPosY() - belowStart + ImGui::GetStyle().ItemSpacing.y;
    }
    ImGui::EndDisabled();
    deleteDialog();
}
