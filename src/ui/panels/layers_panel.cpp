#include "panels/layers_panel.h"

#include <cstdio>
#include <cstring>

#include "icons/icons.h"
#include "imgui.h"

namespace
{

constexpr float LAYER_BUTTON_SIZE = 28.0f;

} // namespace

LayersPanel::LayersPanel(Editor &editor) : editor_(editor)
{
}

void LayersPanel::rows()
{
    for (int i = sprite_layer_count(editor_.sprite()) - 1; i >= 0; i--) {
        Layer *layer = sprite_layer(editor_.sprite(), i);
        ImGui::PushID(i);
        bool visible = layer_visible(layer);
        if (ImGui::Checkbox("##visible", &visible)) {
            editor_.setLayerVisible(i, visible);
        }
        ImGui::SameLine();
        const char *name = layer_name(layer);
        if (ImGui::Selectable(name[0] ? name : "(unnamed)", i == editor_.activeLayer())) {
            editor_.setActiveLayer(i);
        }
        ImGui::PopID();
    }
}

/* Name and opacity edit the layer live while the widget is active and record one undo step when
 * it is released, so typing a name or dragging the slider is not one step per frame. */
void LayersPanel::activeLayerDetails()
{
    int index = editor_.activeLayer();
    Layer *layer = sprite_layer(editor_.sprite(), index);
    if (!layer) {
        return;
    }
    /* A widget can edit on the same frame it activates, so keep the old value to remember it
     * once the activation shows up. */
    char name[LAYER_NAME_MAX + 1];
    std::snprintf(name, sizeof(name), "%s", layer_name(layer));
    char nameNow[LAYER_NAME_MAX + 1];
    std::memcpy(nameNow, name, sizeof(nameNow));
    if (ImGui::InputText("Name", name, sizeof(name))) {
        layer_set_name(layer, name);
    }
    if (ImGui::IsItemActivated()) {
        std::memcpy(nameBefore_, nameNow, sizeof(nameNow));
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitLayerName(index, nameBefore_);
    }

    int opacity = layer_opacity(layer);
    int opacityNow = opacity;
    if (ImGui::SliderInt("Opacity", &opacity, 0, 255)) {
        layer_set_opacity(layer, static_cast<uint8_t>(opacity));
        editor_.markAllDirty();
    }
    if (ImGui::IsItemActivated()) {
        opacityBefore_ = static_cast<uint8_t>(opacityNow);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitLayerOpacity(index, opacityBefore_);
    }
    if (!layer_visible(layer)) {
        ImGui::TextDisabled("Layer is hidden: tools cannot draw on it");
    }
}

void LayersPanel::buttons()
{
    using icons::Icon;
    const float size = LAYER_BUTTON_SIZE;
    int count = sprite_layer_count(editor_.sprite());
    int active = editor_.activeLayer();
    if (icons::button(Icon::Add, "##add", "Add layer", false, size)) {
        editor_.addLayer();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(count < 2);
    if (icons::button(Icon::Remove, "##remove", "Remove layer", false, size)) {
        editor_.removeLayer(active);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (icons::button(Icon::Duplicate, "##dup", "Duplicate layer", false, size)) {
        editor_.duplicateLayer(active);
    }
    ImGui::SameLine();
    Layer *layer = sprite_layer(editor_.sprite(), active);
    Layer *below = sprite_layer(editor_.sprite(), active - 1);
    ImGui::BeginDisabled(!layer || !below || !layer_visible(layer) || !layer_visible(below));
    if (icons::button(Icon::MergeDown, "##merge",
                      "Merge into the layer below (both must be visible)", false, size)) {
        editor_.mergeLayerDown(active);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(active >= count - 1);
    if (icons::button(Icon::MoveUp, "##up", "Move layer up", false, size)) {
        editor_.moveLayer(active, active + 1);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(active <= 0);
    if (icons::button(Icon::MoveDown, "##down", "Move layer down", false, size)) {
        editor_.moveLayer(active, active - 1);
    }
    ImGui::EndDisabled();
}

void LayersPanel::show()
{
    /* Layers cannot change under a tool that is mid-drag. */
    ImGui::BeginDisabled(editor_.stroking());
    rows();
    activeLayerDetails();
    buttons();
    ImGui::EndDisabled();
}
