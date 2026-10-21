#include "panels/palette_panel.h"

#include "imgui.h"
#include "ui_common.h"

namespace
{

constexpr float SWATCH_SIZE = 18.0f;
constexpr int SWATCHES_PER_ROW = 9;
constexpr float SWATCH_GAP = 4.0f;

/* Draws one swatch and returns true when it was clicked. A selected swatch gets a border. */
bool swatch(Rgba color, bool selected)
{
    ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
    if (selected) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    bool clicked = ImGui::ColorButton("##swatch", imvec4FromRgba(color), flags,
                                      ImVec2(SWATCH_SIZE, SWATCH_SIZE));
    if (selected) {
        ImGui::PopStyleVar();
    }
    return clicked;
}

} // namespace

PalettePanel::PalettePanel(Editor &editor) : editor_(editor)
{
}

void PalettePanel::recentRow()
{
    ImGui::SeparatorText("Recent");
    if (editor_.recent.count == 0) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Recently used colors appear here");
        ImGui::PopTextWrapPos();
        return;
    }
    /* Distinct from the palette swatches' IDs below. */
    ImGui::PushID("recent");
    for (int i = 0; i < editor_.recent.count; i++) {
        ImGui::PushID(i);
        if (i % SWATCHES_PER_ROW != 0) {
            ImGui::SameLine(0.0f, SWATCH_GAP);
        }
        if (swatch(editor_.recent.colors[i], false)) {
            editor_.setForeground(editor_.recent.colors[i]);
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

void PalettePanel::paletteSwatches()
{
    Palette *palette = sprite_palette(editor_.sprite());
    int count = palette_count(palette);
    ImGui::SeparatorText("Palette");
    ImGui::PushID("palette");
    for (int i = 0; i < count; i++) {
        ImGui::PushID(i);
        if (i % SWATCHES_PER_ROW != 0) {
            ImGui::SameLine(0.0f, SWATCH_GAP);
        }
        if (swatch(palette_get(palette, i), i == editor_.palette_selected)) {
            editor_.palettePick(i);
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    if (ImGui::Button("Add")) {
        editor_.paletteAdd();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(editor_.palette_selected < 0);
    if (ImGui::Button("Remove")) {
        editor_.paletteRemove(editor_.palette_selected);
    }
    ImGui::EndDisabled();
}

void PalettePanel::show()
{
    recentRow();
    paletteSwatches();
}
