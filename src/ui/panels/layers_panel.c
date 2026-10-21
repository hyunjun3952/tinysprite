#include "panels/layers_panel.h"

#include <stdio.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "icons/icons.h"

static const ImVec2 AUTO_SIZE = {0.0f, 0.0f};

static void layer_rows(Editor *editor)
{
    for (int i = sprite_layer_count(editor->sprite) - 1; i >= 0; i--) {
        Layer *layer = sprite_layer(editor->sprite, i);
        igPushID_Int(i);
        bool visible = layer_visible(layer) != 0;
        if (igCheckbox("##visible", &visible)) {
            editor_set_layer_visible(editor, i, visible);
        }
        igSameLine(0.0f, -1.0f);
        const char *name = layer_name(layer);
        if (igSelectable_Bool(name[0] ? name : "(unnamed)", i == editor->active_layer, 0,
                              AUTO_SIZE)) {
            editor->active_layer = i;
        }
        igPopID();
    }
}

/* Name and opacity edit the layer live while the widget is active and record one undo step when
 * it is released, so typing a name or dragging the slider is not one step per frame. */
static void active_layer_details(Editor *editor)
{
    static char name_before[LAYER_NAME_MAX + 1];
    static int opacity_before;
    int index = editor->active_layer;
    Layer *layer = sprite_layer(editor->sprite, index);
    if (!layer) {
        return;
    }
    /* A widget can edit on the same frame it activates, so read the old value first. */
    char name[LAYER_NAME_MAX + 1];
    snprintf(name, sizeof(name), "%s", layer_name(layer));
    char name_now[LAYER_NAME_MAX + 1];
    snprintf(name_now, sizeof(name_now), "%s", name);
    if (igInputText("Name", name, sizeof(name), 0, NULL, NULL)) {
        layer_set_name(layer, name);
    }
    if (igIsItemActivated()) {
        snprintf(name_before, sizeof(name_before), "%s", name_now);
    }
    if (igIsItemDeactivatedAfterEdit()) {
        editor_commit_layer_name(editor, index, name_before);
    }

    int opacity = layer_opacity(layer);
    int opacity_now = opacity;
    if (igSliderInt("Opacity", &opacity, 0, 255, "%d", 0)) {
        layer_set_opacity(layer, (uint8_t)opacity);
        editor_mark_all_dirty(editor);
    }
    if (igIsItemActivated()) {
        opacity_before = opacity_now;
    }
    if (igIsItemDeactivatedAfterEdit()) {
        editor_commit_layer_opacity(editor, index, (uint8_t)opacity_before);
    }
    if (!layer_visible(layer)) {
        igTextDisabled("Layer is hidden: tools cannot draw on it");
    }
}

enum { LAYER_BUTTON_SIZE = 28 };

static void layer_buttons(Editor *editor)
{
    const float size = LAYER_BUTTON_SIZE;
    int count = sprite_layer_count(editor->sprite);
    int active = editor->active_layer;
    if (icon_button(ICON_ADD, "##add", "Add layer", 0, size)) {
        editor_add_layer(editor);
    }
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(count < 2);
    if (icon_button(ICON_REMOVE, "##remove", "Remove layer", 0, size)) {
        editor_remove_layer(editor, active);
    }
    igEndDisabled();
    igSameLine(0.0f, -1.0f);
    if (icon_button(ICON_DUPLICATE, "##dup", "Duplicate layer", 0, size)) {
        editor_duplicate_layer(editor, active);
    }
    igSameLine(0.0f, -1.0f);
    Layer *layer = sprite_layer(editor->sprite, active);
    Layer *below = sprite_layer(editor->sprite, active - 1);
    igBeginDisabled(!layer || !below || !layer_visible(layer) || !layer_visible(below));
    if (icon_button(ICON_MERGE_DOWN, "##merge", "Merge into the layer below (both must be visible)",
                    0, size)) {
        editor_merge_layer_down(editor, active);
    }
    igEndDisabled();
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(active >= count - 1);
    if (icon_button(ICON_MOVE_UP, "##up", "Move layer up", 0, size)) {
        editor_move_layer(editor, active, active + 1);
    }
    igEndDisabled();
    igSameLine(0.0f, -1.0f);
    igBeginDisabled(active <= 0);
    if (icon_button(ICON_MOVE_DOWN, "##down", "Move layer down", 0, size)) {
        editor_move_layer(editor, active, active - 1);
    }
    igEndDisabled();
}

void layers_panel_show(Editor *editor)
{
    /* Layers cannot change under a tool that is mid-drag. */
    igBeginDisabled(editor_stroking(editor));
    layer_rows(editor);
    active_layer_details(editor);
    layer_buttons(editor);
    igEndDisabled();
}
