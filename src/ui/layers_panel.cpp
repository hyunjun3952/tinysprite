#include "layers_panel.h"

#include <cstdio>

#include "icons.h"
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
    for (int i = editor_.sprite().layerCount() - 1; i >= 0; i--) {
        Layer *layer = editor_.sprite().layer(i);
        ImGui::PushID(i);
        bool visible = layer->visible();
        if (ImGui::Checkbox("##visible", &visible)) {
            editor_.setLayerVisible(i, visible);
        }
        ImGui::SameLine();
        const char *name = layer->name().c_str();
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
    Layer *layer = editor_.sprite().layer(index);
    if (!layer) {
        return;
    }
    /* A widget can edit on the same frame it activates, so keep the old value to remember it
     * once the activation shows up. */
    const std::string nameNow = layer->name();
    char name[LAYER_NAME_MAX + 1];
    std::snprintf(name, sizeof(name), "%s", nameNow.c_str());
    if (ImGui::InputText("Name", name, sizeof(name))) {
        layer->setName(name);
    }
    if (ImGui::IsItemActivated()) {
        nameBefore_ = nameNow;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitLayerName(index, nameBefore_);
    }

    const uint8_t opacityNow = layer->opacity();
    int opacity = opacityNow;
    if (ImGui::SliderInt("Opacity", &opacity, 0, 255)) {
        layer->setOpacity(static_cast<uint8_t>(opacity));
        editor_.markAllDirty();
    }
    if (ImGui::IsItemActivated()) {
        opacityBefore_ = opacityNow;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        editor_.commitLayerOpacity(index, opacityBefore_);
    }
    if (!layer->visible()) {
        ImGui::TextDisabled("Layer is hidden: tools cannot draw on it");
    }
}

void LayersPanel::buttons()
{
    using icons::Icon;
    const float size = LAYER_BUTTON_SIZE;
    int count = editor_.sprite().layerCount();
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
    Layer *layer = editor_.sprite().layer(active);
    Layer *below = editor_.sprite().layer(active - 1);
    ImGui::BeginDisabled(!layer || !below || !layer->visible() || !below->visible());
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
