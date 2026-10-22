#include "tools_panel.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <iterator>

#include "icons.hpp"
#include "theme.hpp"

namespace
{

constexpr float COLOR_SWATCH_BASE = 36.0f;        /* all four at scale 1; see the helpers below */
constexpr float COLOR_SWATCH_OFFSET_BASE = 18.0f; /* how far the background swatch peeks out */
constexpr float SWAP_BUTTON_GAP_BASE = 12.0f;
constexpr float SWAP_BUTTON_SIZE = 26.0f; /* a round button next to the color swatches */
constexpr float COLOR_SECTION_GAP_BASE = 6.0f;

/* Everything the UI shows for a tool. */
struct ToolInfo {
    const char *label;
    const char *hint; /* the shortcut letter, as shown at the button's right end */
    ImGuiKey key;
    const char *tooltip;
};

/* Indexed by ToolKind. */
const std::array<ToolInfo, TOOL_COUNT> TOOLS = {{
    {"Pencil", "B", ImGuiKey_B,
     "Draw pixels. Shift+click draws a straight line from where the last stroke ended."},
    {"Eraser", "E", ImGuiKey_E, "Erase pixels to transparent. Uses the brush size."},
    {"Bucket", "G", ImGuiKey_G, "Fill the connected area of one color with the foreground color."},
    {"Eyedropper", "I", ImGuiKey_I,
     "Pick the color under the cursor as the foreground color.\nHold the middle mouse button to "
     "use it with any tool."},
    {"Line", "L", ImGuiKey_L, "Drag to draw a straight line."},
    {"Rectangle", "R", ImGuiKey_R,
     "Drag to draw a rectangle (outline, or filled in Tool options)."},
    {"Ellipse", "O", ImGuiKey_O, "Drag to draw an ellipse (outline, or filled in Tool options)."},
    {"Select", "M", ImGuiKey_M,
     "Drag to select a rectangle. Drag inside the selection to move it.\n"
     "Drawing tools then only change pixels inside it."},
    {"Triangle", "T", ImGuiKey_T, "Drag to draw a triangle (outline, or filled in Tool options)."},
    {"Diamond", "D", ImGuiKey_D, "Drag to draw a diamond (outline, or filled in Tool options)."},
    {"Hexagon", "H", ImGuiKey_H, "Drag to draw a hexagon (outline, or filled in Tool options)."},
    {"Star", "S", ImGuiKey_S, "Drag to draw a five-pointed star (outline, or filled)."},
    {"Cross", "C", ImGuiKey_C, "Drag to draw a plus sign (outline, or filled in Tool options)."},
}};

constexpr float SWATCH_SIZE_BASE = 22.0f;
constexpr float SWATCH_GAP_BASE = 4.0f;

float swatchSize()
{
    return theme::px(SWATCH_SIZE_BASE);
}

float swatchGap()
{
    return theme::px(SWATCH_GAP_BASE);
}

/* As many swatches as fit in the panel's width, so a wider panel shows longer rows. */
int swatchesPerRow()
{
    float size = swatchSize() + swatchGap();
    return std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + swatchGap()) / size));
}

ImVec4 toImVec4(const float color[4])
{
    return {color[0], color[1], color[2], color[3]};
}

/* Draws one swatch and returns true when it was clicked. A selected swatch gets a border. */
bool swatch(Rgba color, bool selected)
{
    ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
    if (selected) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    bool clicked = ImGui::ColorButton(
        "##swatch",
        ImVec4(static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
               static_cast<float>(color.b) / 255.0f, static_cast<float>(color.a) / 255.0f),
        flags, ImVec2(swatchSize(), swatchSize()));
    if (selected) {
        ImGui::PopStyleVar();
    }
    char text[64];
    std::snprintf(text, sizeof(text), "#%02X%02X%02X%02X\nClick to use as the foreground color",
                  color.r, color.g, color.b, color.a);
    theme::tip(text);
    return clicked;
}

} // namespace

ToolsPanel::ToolsPanel(Editor &editor) : editor_(editor)
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
    /* ImGui's own RGB / HSV / Hex input row is replaced by theme::hex_field under the wheel. */
    ImGuiColorEditFlags pickerFlags = ImGuiColorEditFlags_PickerHueWheel |
                                      ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
                                      ImGuiColorEditFlags_NoSidePreview;
    ImVec2 start = ImGui::GetCursorPos();
    /* The sizes follow the UI scale, like the palette swatches under them. */
    const float swatch = theme::px(COLOR_SWATCH_BASE);
    const float offset = theme::px(COLOR_SWATCH_OFFSET_BASE);
    ImVec2 size(swatch, swatch);

    /* Background first so the foreground, drawn later, wins the overlap. */
    ImGui::SetCursorPos(ImVec2(start.x + offset, start.y + offset));
    if (ImGui::ColorButton("##bg_swatch", toImVec4(editor_.backgroundFloats()), buttonFlags,
                           size)) {
        ImGui::OpenPopup("bg_picker");
    }
    theme::tip("Background color. Click to edit; X swaps it with the foreground.");
    ImGui::SetCursorPos(start);
    if (ImGui::ColorButton("##fg_swatch", toImVec4(editor_.foregroundFloats()), buttonFlags,
                           size)) {
        ImGui::OpenPopup("fg_picker");
    }
    theme::tip("Foreground color: what the tools draw with. Click to edit.");

    ImGui::SetCursorPos(
        ImVec2(start.x + swatch + offset + theme::px(SWAP_BUTTON_GAP_BASE), start.y));
    if (icons::round_button(icons::Icon::Swap, "##swap_colors",
                            "Swap foreground and background (X)", theme::px(SWAP_BUTTON_SIZE))) {
        editor_.swapColors();
    }
    ImGui::SetCursorPos(
        ImVec2(start.x, start.y + swatch + offset + theme::px(COLOR_SECTION_GAP_BASE)));

    if (ImGui::BeginPopup("fg_picker")) {
        ImGui::Text("Foreground");
        ImGui::ColorPicker4("##fg_picker", editor_.foregroundFloats(), pickerFlags);
        ImGui::Spacing();
        theme::hex_field("fg_hex", editor_.foregroundFloats(), fgHex_);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("bg_picker")) {
        ImGui::Text("Background");
        ImGui::ColorPicker4("##bg_picker", editor_.backgroundFloats(), pickerFlags);
        ImGui::Spacing();
        theme::hex_field("bg_hex", editor_.backgroundFloats(), bgHex_);
        ImGui::EndPopup();
    }
}

static bool isShapeTool(ToolKind tool)
{
    return tool_traits(tool).group == ToolGroup::Shape;
}

static bool isClosedShape(ToolKind tool)
{
    return isShapeTool(tool) && shape_has_fill(tool_traits(tool).shape);
}

/* One tool as an icon button; its tooltip has the name, the shortcut and what it does. */
struct ToolIcon {
    ToolKind tool;
    icons::Icon icon;
    const char *id;
};

/* The tools as a row of icon buttons that wraps at the panel's width. A toolbar of pictures with
 * the name and shortcut in the tooltip, as Aseprite does, takes a fraction of the room of a text
 * button per tool. */
void ToolsPanel::iconRow(const ToolIcon *first, size_t count)
{
    float side = ImGui::GetFrameHeight() + theme::px(8.0f);
    float gap = ImGui::GetStyle().ItemSpacing.x;
    int perRow =
        std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + gap) / (side + gap)));
    for (size_t i = 0; i < count; i++) {
        const ToolIcon &entry = first[i];
        if (i % static_cast<size_t>(perRow) != 0) {
            ImGui::SameLine();
        }
        const ToolInfo &info = TOOLS[static_cast<size_t>(entry.tool)];
        char tooltip[160];
        std::snprintf(tooltip, sizeof(tooltip), "%s (%s)\n%s", info.label, info.hint, info.tooltip);
        if (icons::choice(entry.icon, entry.id, tooltip, editor_.tool() == entry.tool,
                          ImVec2(side, side))) {
            editor_.selectTool(entry.tool);
        }
    }
}

void ToolsPanel::toolGrid()
{
    static const ToolIcon TOOL_ICONS[] = {
        {ToolKind::Pencil, icons::Icon::Pencil, "##tool_pencil"},
        {ToolKind::Eraser, icons::Icon::Eraser, "##tool_eraser"},
        {ToolKind::Bucket, icons::Icon::Bucket, "##tool_bucket"},
        {ToolKind::Eyedropper, icons::Icon::Eyedropper, "##tool_eyedropper"},
        {ToolKind::Select, icons::Icon::Select, "##tool_select"}};
    iconRow(TOOL_ICONS, std::size(TOOL_ICONS));
}

void ToolsPanel::shapeButtons()
{
    static const ToolIcon SHAPES[] = {
        {ToolKind::Line, icons::Icon::Line, "##shape_line"},
        {ToolKind::Rect, icons::Icon::Rect, "##shape_rect"},
        {ToolKind::Ellipse, icons::Icon::Ellipse, "##shape_ellipse"},
        {ToolKind::Triangle, icons::Icon::Triangle, "##shape_triangle"},
        {ToolKind::Diamond, icons::Icon::Diamond, "##shape_diamond"},
        {ToolKind::Hexagon, icons::Icon::Hexagon, "##shape_hexagon"},
        {ToolKind::Star, icons::Icon::Star, "##shape_star"},
        {ToolKind::Cross, icons::Icon::Cross, "##shape_cross"}};
    iconRow(SHAPES, std::size(SHAPES));
}

/* ---- the options bar: the options of the active tool, in one row above the canvas ---- */

namespace
{

constexpr float BAR_GAP = 16.0f; /* between two groups of options */
constexpr float BAR_SLIDER_WIDTH = 140.0f;
constexpr float BAR_MIRROR_WIDTH = 190.0f; /* the room both mirror boxes need */

/* A new group of options on the same row, a gap after the last one. */
void barGap()
{
    ImGui::SameLine(0.0f, theme::px(BAR_GAP));
}

} // namespace

/* "Size" and a slider, the brush width of the pencil and eraser and the outline of a shape. */
void ToolsPanel::barSize(const char *tip)
{
    Editor::ToolOptions &options = editor_.options();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Size");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme::px(BAR_SLIDER_WIDTH));
    ImGui::SliderInt("##bar_size", &options.brush_size, 1, PENCIL_BRUSH_MAX, "%d px");
    theme::tip(tip);
}

void ToolsPanel::barBrush()
{
    Editor::ToolOptions &options = editor_.options();
    barGap();
    barSize("Brush width in pixels. + and - (or [ and ]) change it by 1.");
    barGap();
    ImGui::Checkbox("Round", &options.brush_round);
    theme::tip("Stamp a round brush instead of a square one (matters above 1 px).");
    barGap();
    ImGui::Checkbox("Pixel-perfect", &options.pixel_perfect);
    theme::tip("Drop the extra corner pixel of diagonal strokes so lines stay 1 px thin.\n"
               "Only for a 1 px brush.");
}

void ToolsPanel::barShape(ToolKind tool)
{
    Editor::ToolOptions &options = editor_.options();
    const bool closed = isClosedShape(tool);
    if (closed) {
        barGap();
        ImGui::Checkbox("Filled", &options.shape_filled);
        theme::tip("Fill the shape instead of drawing only its outline.");
    }
    if (!closed || !options.shape_filled) {
        barGap();
        barSize("Outline width in pixels. + and - (or [ and ]) change it by 1.");
    }
}

/* Flip and turn the selected pixels (the Edit menu has the same actions). */
void ToolsPanel::barSelection()
{
    const bool enabled = editor_.hasSelection() && !editor_.stroking();
    barGap();
    if (theme::actionButton("Flip H", "Flip the selection horizontally", 0.0f, enabled)) {
        editor_.flipSelection(true);
    }
    ImGui::SameLine();
    if (theme::actionButton("Flip V", "Flip the selection vertically", 0.0f, enabled)) {
        editor_.flipSelection(false);
    }
    ImGui::SameLine();
    if (theme::actionButton("Rotate CW", "Rotate the selection clockwise", 0.0f, enabled)) {
        editor_.rotateSelection(true);
    }
    ImGui::SameLine();
    if (theme::actionButton("Rotate CCW", "Rotate the selection counterclockwise", 0.0f, enabled)) {
        editor_.rotateSelection(false);
    }
    if (!editor_.hasSelection()) {
        ImGui::SameLine();
        ImGui::TextDisabled("Drag on the canvas to select an area");
    }
}

/* Mirror X / Y apply to the tools that draw: pencil, eraser, shapes and bucket. They sit at the
 * right end of the bar when there is room for them. */
void ToolsPanel::barSymmetry()
{
    Symmetry &symmetry = editor_.options().symmetry;
    if (ImGui::GetContentRegionAvail().x < theme::px(BAR_MIRROR_WIDTH)) {
        return;
    }
    barGap();
    ImGui::Checkbox("Mirror X", &symmetry.mirror_x);
    theme::tip("Also draw the mirror image across the vertical center line.\n"
               "Applies to pencil, eraser, shapes and bucket.");
    ImGui::SameLine();
    ImGui::Checkbox("Mirror Y", &symmetry.mirror_y);
    theme::tip("Also draw the mirror image across the horizontal center line.\n"
               "Applies to pencil, eraser, shapes and bucket.");
}

void ToolsPanel::contextBar(float x, float y, float width, float height)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::px(12.0f), theme::px(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##options_bar", nullptr, FLAGS)) {
        const ToolKind tool = editor_.tool();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TOOLS[static_cast<size_t>(tool)].label);
        const ToolGroup group = tool_traits(tool).group;
        if (group == ToolGroup::Brush) {
            barBrush();
        } else if (group == ToolGroup::Shape) {
            barShape(tool);
        } else if (tool == ToolKind::Select || editor_.hasSelection()) {
            barSelection();
        }
        if (group == ToolGroup::Brush || group == ToolGroup::Shape || group == ToolGroup::Fill) {
            barSymmetry();
        }
        /* a hairline under the bar, like the one between the sections */
        ImVec2 at = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(at.x, at.y + height - 1.0f), ImVec2(at.x + width, at.y + height - 1.0f),
            ImGui::ColorConvertFloat4ToU32(theme::rgb(theme::HAIRLINE, 0.6f)));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

/* ---- the tool bar (the narrow left panel) and the color block of the right panel ---- */

/* The foreground and background colors, the palette, and the recent colors under it. */
void ToolsPanel::colorBlock(const Panels &panels)
{
    if (panels.sectionVisible(PanelId::Layers, sections::COLOR) && theme::section("Color")) {
        colorWidget();
        paletteSwatches();
        if (editor_.recent().size() > 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("Recent");
            recentColors();
        }
    }
    resetDialog();
}

void ToolsPanel::show(Panels &panels, float height)
{
    if (!panels.begin(PanelId::Tools, height)) {
        panels.end();
        return;
    }
    if (panels.sectionVisible(PanelId::Tools, sections::TOOLS)) {
        toolGrid();
    }
    if (panels.sectionVisible(PanelId::Tools, sections::SHAPES)) {
        if (panels.sectionVisible(PanelId::Tools, sections::TOOLS)) {
            ImGui::Separator();
        }
        shapeButtons();
    }
    panels.end();
}

/* "Reset palette?": Reset replaces the palette with the defaults, Cancel (or Escape) keeps it. The
 * default button is Cancel, so a stray Enter does not destroy anything. */
void ToolsPanel::resetDialog()
{
    constexpr const char *TITLE = "Reset palette";
    if (openReset_) {
        ImGui::OpenPopup(TITLE);
        openReset_ = false;
    }
    dialogOpen_ = ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (!dialogOpen_) {
        return;
    }
    ImGui::TextUnformatted("Reset the palette to the default colors?");
    ImGui::TextDisabled("Colors you added are lost. This cannot be undone.");
    bool close = false;
    /* Cancel is drawn last, so it is the button that gets the keyboard focus below */
    const int clicked = theme::dialog_buttons({"Reset", "Cancel"}, 0, true);
    if (clicked == 0) {
        editor_.paletteReset();
        close = true;
    }
    if (clicked == 1 || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(-1); /* the Cancel button */
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

/* The recent-colors row and the sprite palette swatches. Clicking a swatch makes it the foreground
 * color. */
void ToolsPanel::recentColors()
{
    if (editor_.recent().size() == 0) {
        return;
    }
    /* Distinct from the palette swatches' IDs below. */
    ImGui::PushID("recent");
    const int perRow = swatchesPerRow();
    for (int i = 0; i < editor_.recent().size(); i++) {
        ImGui::PushID(i);
        if (i % perRow != 0) {
            ImGui::SameLine(0.0f, swatchGap());
        }
        if (swatch(editor_.recent()[i], false)) {
            editor_.setForeground(editor_.recent()[i]);
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

void ToolsPanel::paletteSwatches()
{
    const Palette &palette = editor_.sprite().palette();
    int count = palette.count();
    ImGui::PushID("palette");
    const int perRow = swatchesPerRow();
    for (int i = 0; i < count; i++) {
        ImGui::PushID(i);
        if (i % perRow != 0) {
            ImGui::SameLine(0.0f, swatchGap());
        }
        if (swatch(palette.get(i), i == editor_.paletteSelected())) {
            editor_.palettePick(i);
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImVec2 size(theme::columnWidth(3), ImGui::GetFrameHeight() + 6.0f);
    if (icons::button(icons::Icon::Plus, "##palette_add", "Add the foreground color to the palette",
                      size)) {
        editor_.paletteAdd();
    }
    ImGui::SameLine();
    if (icons::button(icons::Icon::Minus, "##palette_remove", "Remove the selected swatch", size,
                      editor_.paletteSelected() >= 0)) {
        editor_.paletteRemove(editor_.paletteSelected());
    }
    ImGui::SameLine();
    if (icons::button(icons::Icon::Reset, "##palette_reset",
                      "Reset the palette to the default colors (cannot be undone)", size)) {
        openReset_ = true; /* asks first: it throws the palette's colors away */
    }
}
