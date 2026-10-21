#include "tools_panel.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

#include "icons.h"
#include "theme.h"

namespace
{

constexpr float COLOR_SWATCH = 36.0f;
constexpr float COLOR_SWATCH_OFFSET = 18.0f; /* how far the background swatch peeks out */
constexpr float SWAP_BUTTON_GAP = 12.0f;
constexpr float COLOR_SECTION_GAP = 6.0f;

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

constexpr float SWATCH_SIZE = 18.0f;
constexpr int SWATCHES_PER_ROW = 9;
constexpr float SWATCH_GAP = 4.0f;

ImVec4 toImVec4(const float color[4])
{
    return ImVec4(color[0], color[1], color[2], color[3]);
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
        flags, ImVec2(SWATCH_SIZE, SWATCH_SIZE));
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
    ImGuiColorEditFlags pickerFlags = ImGuiColorEditFlags_PickerHueWheel |
                                      ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex;
    ImVec2 start = ImGui::GetCursorPos();
    ImVec2 size(COLOR_SWATCH, COLOR_SWATCH);

    /* Background first so the foreground, drawn later, wins the overlap. */
    ImGui::SetCursorPos(ImVec2(start.x + COLOR_SWATCH_OFFSET, start.y + COLOR_SWATCH_OFFSET));
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
        ImVec2(start.x + COLOR_SWATCH + COLOR_SWATCH_OFFSET + SWAP_BUTTON_GAP, start.y));
    if (theme::actionButton("Swap", "Swap foreground and background (X)", 0.0f)) {
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

static bool isClosedShape(ToolKind tool)
{
    switch (tool) {
    case ToolKind::Rect:
    case ToolKind::Ellipse:
    case ToolKind::Triangle:
    case ToolKind::Diamond:
    case ToolKind::Hexagon:
    case ToolKind::Star:
    case ToolKind::Cross:
        return true;
    default:
        return false;
    }
}

static bool isShapeTool(ToolKind tool)
{
    return tool == ToolKind::Line || isClosedShape(tool);
}

void ToolsPanel::toolGrid()
{
    constexpr int COLUMNS = 2;
    float width = theme::columnWidth(COLUMNS);
    int shown = 0;
    for (int tool = 0; tool < TOOL_COUNT; tool++) {
        if (isShapeTool(static_cast<ToolKind>(tool))) {
            continue; /* these live in the Shapes group */
        }
        if (shown++ % COLUMNS != 0) {
            ImGui::SameLine();
        }
        ImGui::PushID(tool);
        const ToolInfo &info = TOOLS[static_cast<size_t>(tool)];
        if (theme::choiceButton(info.label, info.hint,
                                editor_.tool() == static_cast<ToolKind>(tool), width)) {
            editor_.selectTool(static_cast<ToolKind>(tool));
        }
        ImGui::PopID();
    }
}

/* The shape tools as a row of small icon buttons. */
void ToolsPanel::shapeButtons()
{
    struct Shape {
        ToolKind tool;
        icons::Icon icon;
        const char *id;
    };
    static const Shape SHAPES[] = {{ToolKind::Line, icons::Icon::Line, "##shape_line"},
                                   {ToolKind::Rect, icons::Icon::Rect, "##shape_rect"},
                                   {ToolKind::Ellipse, icons::Icon::Ellipse, "##shape_ellipse"},
                                   {ToolKind::Triangle, icons::Icon::Triangle, "##shape_triangle"},
                                   {ToolKind::Diamond, icons::Icon::Diamond, "##shape_diamond"},
                                   {ToolKind::Hexagon, icons::Icon::Hexagon, "##shape_hexagon"},
                                   {ToolKind::Star, icons::Icon::Star, "##shape_star"},
                                   {ToolKind::Cross, icons::Icon::Cross, "##shape_cross"}};
    float side = ImGui::GetFrameHeight() + 8.0f;
    float gap = ImGui::GetStyle().ItemSpacing.x;
    /* As many buttons per row as fit the panel's width. */
    int perRow =
        std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + gap) / (side + gap)));
    for (const Shape &shape : SHAPES) {
        if ((&shape - SHAPES) % perRow != 0) {
            ImGui::SameLine();
        }
        const ToolInfo &info = TOOLS[static_cast<size_t>(shape.tool)];
        char tooltip[160];
        std::snprintf(tooltip, sizeof(tooltip), "%s (%s)\n%s", info.label, info.hint, info.tooltip);
        if (icons::choice(shape.icon, shape.id, tooltip, editor_.tool() == shape.tool,
                          ImVec2(side, side))) {
            editor_.selectTool(shape.tool);
        }
    }
}

/* Only what the active tool uses: the brush for the pencil and eraser, fill for the closed shapes.
 * Symmetry has its own section because it reaches the bucket and every shape as well. */
void ToolsPanel::toolOptions()
{
    Editor::ToolOptions &options = editor_.options();
    ToolKind tool = editor_.tool();
    bool brush = tool == ToolKind::Pencil || tool == ToolKind::Eraser;
    bool closedShape = isClosedShape(tool);
    if (brush) {
        /* The name on the left, the slider filling the rest of the row. */
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Size");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##brush_size", &options.brush_size, 1, PENCIL_BRUSH_MAX, "%d px");
        theme::tip("Brush width in pixels. + and - (or [ and ]) change it by 1.");
        ImGui::Checkbox("Round brush", &options.brush_round);
        theme::tip("Stamp a round brush instead of a square one (matters above 1 px).");
        ImGui::Checkbox("Pixel-perfect", &options.pixel_perfect);
        theme::tip("Drop the extra corner pixel of diagonal strokes so lines stay 1 px thin.\n"
                   "Only for a 1 px brush.");
    } else if (closedShape) {
        ImGui::Checkbox("Filled", &options.shape_filled);
        theme::tip("Fill the shape instead of drawing only its outline.");
    } else {
        ImGui::TextDisabled("No options for this tool");
    }
}

void ToolsPanel::symmetryOptions()
{
    Symmetry &symmetry = editor_.options().symmetry;
    ImGui::Checkbox("Mirror X", &symmetry.mirror_x);
    theme::tip("Also draw the mirror image across the vertical center line.\n"
               "Applies to pencil, eraser, shapes and bucket.");
    ImGui::SameLine();
    ImGui::Checkbox("Mirror Y", &symmetry.mirror_y);
    theme::tip("Also draw the mirror image across the horizontal center line.\n"
               "Applies to pencil, eraser, shapes and bucket.");
}

void ToolsPanel::selectionButtons()
{
    bool enabled = editor_.hasSelection() && !editor_.stroking();
    float width = theme::columnWidth(2);
    if (theme::actionButton("Flip H", "Flip the selection horizontally", width, enabled)) {
        editor_.flipSelection(true);
    }
    ImGui::SameLine();
    if (theme::actionButton("Flip V", "Flip the selection vertically", width, enabled)) {
        editor_.flipSelection(false);
    }
    if (theme::actionButton("Rotate CW", "Rotate the selection clockwise", width, enabled)) {
        editor_.rotateSelection(true);
    }
    ImGui::SameLine();
    if (theme::actionButton("Rotate CCW", "Rotate the selection counterclockwise", width,
                            enabled)) {
        editor_.rotateSelection(false);
    }
    if (!editor_.hasSelection()) {
        ImGui::TextDisabled("Select an area first (M)");
    }
}

void ToolsPanel::show(Panels &panels, float height)
{
    if (!panels.begin(PanelId::Tools, height)) {
        panels.end();
        return;
    }

    auto open = [&panels](const char *name) {
        return panels.sectionVisible(PanelId::Tools, name) && theme::section(name);
    };
    if (open(sections::TOOLS)) {
        toolGrid();
    }
    if (open(sections::SHAPES)) {
        shapeButtons();
    }
    if (open(sections::TOOL_OPTIONS)) {
        toolOptions();
    }
    if (open(sections::SYMMETRY)) {
        symmetryOptions();
    }
    if (open(sections::SELECTION)) {
        selectionButtons();
    }
    if (open(sections::COLOR)) {
        colorWidget();
        paletteSwatches();
    }
    if (open(sections::RECENT)) {
        recentColors();
    }

    resetDialog();
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
    if (ImGui::Button("Reset")) {
        editor_.paletteReset();
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Recently used colors appear here");
        ImGui::PopTextWrapPos();
        return;
    }
    /* Distinct from the palette swatches' IDs below. */
    ImGui::PushID("recent");
    for (int i = 0; i < editor_.recent().size(); i++) {
        ImGui::PushID(i);
        if (i % SWATCHES_PER_ROW != 0) {
            ImGui::SameLine(0.0f, SWATCH_GAP);
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
    for (int i = 0; i < count; i++) {
        ImGui::PushID(i);
        if (i % SWATCHES_PER_ROW != 0) {
            ImGui::SameLine(0.0f, SWATCH_GAP);
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
