#include "panels/palette_panel.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

enum {
    SWATCH_SIZE = 18,
    SWATCHES_PER_ROW = 9,
};

static ImVec4 to_imvec4(Rgba color)
{
    return (ImVec4){(float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f,
                    (float)color.a / 255.0f};
}

/* Draws one swatch and returns non-zero when it was clicked. A selected swatch gets a border. */
static int swatch(Rgba color, int selected)
{
    ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
    if (selected) {
        igPushStyleVar_Float(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    int clicked =
        igColorButton("##swatch", to_imvec4(color), flags, (ImVec2){SWATCH_SIZE, SWATCH_SIZE});
    if (selected) {
        igPopStyleVar(1);
    }
    return clicked;
}

static void recent_row(Editor *editor)
{
    igSeparatorText("Recent");
    if (editor->recent.count == 0) {
        igPushTextWrapPos(0.0f);
        igTextDisabled("Recently used colors appear here");
        igPopTextWrapPos();
        return;
    }
    for (int i = 0; i < editor->recent.count; i++) {
        igPushID_Int(i);
        if (i % SWATCHES_PER_ROW != 0) {
            igSameLine(0.0f, 4.0f);
        }
        if (swatch(editor->recent.colors[i], 0)) {
            editor_set_color(editor, editor->recent.colors[i]);
        }
        igPopID();
    }
}

static void palette_swatches(Editor *editor)
{
    Palette *palette = sprite_palette(editor->sprite);
    int count = palette_count(palette);
    igSeparatorText("Palette");
    for (int i = 0; i < count; i++) {
        igPushID_Int(1000 + i);
        if (i % SWATCHES_PER_ROW != 0) {
            igSameLine(0.0f, 4.0f);
        }
        if (swatch(palette_get(palette, i), i == editor->palette_selected)) {
            editor_palette_pick(editor, i);
        }
        igPopID();
    }
    if (igButton("Add", (ImVec2){0.0f, 0.0f})) {
        editor_palette_add(editor);
    }
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(editor->palette_selected < 0);
    if (igButton("Remove", (ImVec2){0.0f, 0.0f})) {
        editor_palette_remove(editor, editor->palette_selected);
    }
    igEndDisabled();
}

void palette_panel_show(Editor *editor)
{
    recent_row(editor);
    palette_swatches(editor);
}
