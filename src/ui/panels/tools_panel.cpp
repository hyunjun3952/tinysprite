#include "panels/tools_panel.h"

#include <array>

#include "icons/icons.h"
#include "ui_common.h"

namespace
{

constexpr int TOOLS_PER_ROW = 4;
constexpr float TOOL_BUTTON_SIZE = 40.0f;
constexpr float SMALL_BUTTON_SIZE = 28.0f;
constexpr float COLOR_SWATCH = 36.0f;
constexpr float COLOR_SWATCH_OFFSET = 18.0f; /* how far the background swatch peeks out */
constexpr float SWAP_BUTTON_GAP = 12.0f;
constexpr float COLOR_SECTION_GAP = 6.0f;
constexpr float MIRROR_Y_COLUMN = 125.0f; /* x of the "Mirror Y" checkbox, beside "Mirror X" */

/* Everything the UI shows for a tool. */
struct ToolInfo {
    const char *label;
    icons::Icon icon;
    ImGuiKey key;
};

/* Indexed by ToolKind. */
const std::array<ToolInfo, TOOL_COUNT> TOOLS = {{
    {"Pencil (B)", icons::Icon::Pencil, ImGuiKey_B},
    {"Eraser (E)", icons::Icon::Eraser, ImGuiKey_E},
    {"Bucket (G)", icons::Icon::Bucket, ImGuiKey_G},
    {"Eyedropper (I)", icons::Icon::Eyedropper, ImGuiKey_I},
    {"Line (L)", icons::Icon::Line, ImGuiKey_L},
    {"Rect (R)", icons::Icon::Rect, ImGuiKey_R},
    {"Ellipse (O)", icons::Icon::Ellipse, ImGuiKey_O},
    {"Select (M)", icons::Icon::Select, ImGuiKey_M},
}};

} // namespace

ToolsPanel::ToolsPanel(Editor &editor) : editor_(editor), palette_(editor)
{
}

ImGuiKey ToolsPanel::shortcutKey(ToolKind tool)
{
    return TOOLS[static_cast<size_t>(tool)].key;
}

/* Foreground swatch drawn over the background one (click either to open its picker), with a swap
 * button beside them. */
void ToolsPanel::colorWidget()
{
    ImGuiColorEditFlags buttonFlags =
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
    ImGuiColorEditFlags pickerFlags = ImGuiColorEditFlags_PickerHueWheel |
                                      ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex;
    ImVec2 start = ImGui::GetCursorPos();
    ImVec2 size(COLOR_SWATCH, COLOR_SWATCH);

    /* Background first so the foreground, drawn later, wins the overlap. */
    ImGui::SetCursorPos(ImVec2(start.x + COLOR_SWATCH_OFFSET, start.y + COLOR_SWATCH_OFFSET));
    if (ImGui::ColorButton("##bg_swatch", imvec4FromFloats(editor_.backgroundFloats()), buttonFlags,
                           size)) {
        ImGui::OpenPopup("bg_picker");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Background color");
    }
    ImGui::SetCursorPos(start);
    if (ImGui::ColorButton("##fg_swatch", imvec4FromFloats(editor_.foregroundFloats()), buttonFlags,
                           size)) {
        ImGui::OpenPopup("fg_picker");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Foreground color");
    }

    ImGui::SetCursorPos(
        ImVec2(start.x + COLOR_SWATCH + COLOR_SWATCH_OFFSET + SWAP_BUTTON_GAP, start.y));
    if (icons::button(icons::Icon::Swap, "##swap", "Swap colors (X)", false, SMALL_BUTTON_SIZE)) {
        editor_.swapColors();
    }
    ImGui::SetCursorPos(
        ImVec2(start.x, start.y + COLOR_SWATCH + COLOR_SWATCH_OFFSET + COLOR_SECTION_GAP));

    if (ImGui::BeginPopup("fg_picker")) {
        ImGui::Text("Foreground");
        ImGui::ColorPicker4("##fg_picker", editor_.foregroundFloats(), pickerFlags);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("bg_picker")) {
        ImGui::Text("Background");
        ImGui::ColorPicker4("##bg_picker", editor_.backgroundFloats(), pickerFlags);
        ImGui::EndPopup();
    }
}

void ToolsPanel::selectionButtons()
{
    using icons::Icon;
    ImGui::BeginDisabled(!editor_.hasSelection() || editor_.stroking());
    const float size = SMALL_BUTTON_SIZE;
    if (icons::button(Icon::FlipH, "##flip_h", "Flip horizontally", false, size)) {
        editor_.flipSelection(true);
    }
    ImGui::SameLine();
    if (icons::button(Icon::FlipV, "##flip_v", "Flip vertically", false, size)) {
        editor_.flipSelection(false);
    }
    ImGui::SameLine();
    if (icons::button(Icon::RotateCw, "##rot_cw", "Rotate clockwise", false, size)) {
        editor_.rotateSelection(true);
    }
    ImGui::SameLine();
    if (icons::button(Icon::RotateCcw, "##rot_ccw", "Rotate counterclockwise", false, size)) {
        editor_.rotateSelection(false);
    }
    ImGui::EndDisabled();
}

void ToolsPanel::show(Panels &panels, float height)
{
    if (!panels.begin(PanelId::Tools, height)) {
        panels.end();
        return;
    }

    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (tool % TOOLS_PER_ROW != 0) {
            ImGui::SameLine();
        }
        ImGui::PushID(tool);
        const ToolInfo &info = TOOLS[static_cast<size_t>(tool)];
        if (icons::button(info.icon, "##tool", info.label,
                          editor_.tool() == static_cast<ToolKind>(tool), TOOL_BUTTON_SIZE)) {
            editor_.selectTool(static_cast<ToolKind>(tool));
        }
        ImGui::PopID();
    }

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("Pixel-perfect", &editor_.options().pixel_perfect);
    ImGui::Checkbox("Mirror X", &editor_.options().symmetry.mirror_x);
    ImGui::SameLine(MIRROR_Y_COLUMN);
    ImGui::Checkbox("Mirror Y", &editor_.options().symmetry.mirror_y);
    ImGui::SliderInt("Brush", &editor_.options().brush_size, 1, PENCIL_BRUSH_MAX, "%d px");
    ImGui::Checkbox("Round brush", &editor_.options().brush_round);
    ImGui::Checkbox("Filled shapes", &editor_.options().shape_filled);

    ImGui::SeparatorText("Selection");
    selectionButtons();

    ImGui::SeparatorText("Color");
    colorWidget();

    palette_.show();

    panels.end();
}
