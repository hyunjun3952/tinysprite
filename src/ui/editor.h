#ifndef TINYSPRITE_UI_EDITOR_H
#define TINYSPRITE_UI_EDITOR_H

#include <SDL.h>

#include "tinysprite/clip.h"
#include "tinysprite/composite.h"
#include "tinysprite/history.h"
#include "tinysprite/move.h"
#include "tinysprite/pencil.h"
#include "tinysprite/recent_colors.h"
#include "tinysprite/shape.h"
#include "tinysprite/sprite.h"
#include "tinysprite/viewport.h"

typedef enum ToolKind {
    TOOL_PENCIL,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_EYEDROPPER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_SELECT,
    TOOL_COUNT,
} ToolKind;

/* Everything the UI edits: the document, its undo history, the tool state and the CPU-composited
 * texture the canvas displays. Created once in main and passed to each panel. */
typedef struct Editor {
    Sprite *sprite;
    History *history;
    ToolKind tool;
    float color[4];       /* foreground color, straight alpha, 0..1 */
    float bg_color[4];    /* background color, swapped with the foreground by editor_swap_colors */
    RecentColors recent;  /* colors drawn with lately, newest first */
    int palette_selected; /* swatch of the sprite palette last picked or added, -1 for none */
    int temp_pick;        /* the eyedropper is active only while a modifier is held */
    ToolKind pick_return; /* the tool to go back to when the temporary pick ends */
    int show_grid;
    int active_layer;
    int frame;

    /* Tool options: pixel_perfect / mirror_* / brush_* apply to pencil and eraser, shape_filled to
     * the rectangle and ellipse (mirror_* also reaches shapes and the bucket). */
    int brush_size; /* 1..PENCIL_BRUSH_MAX */
    int brush_round;
    int pixel_perfect;
    int mirror_x, mirror_y;
    int shape_filled;

    Viewport view;
    int needs_fit;            /* fit the sprite to the canvas on the next canvas frame */
    int auto_fit;             /* the view is still the fitted one: refit when the canvas resizes */
    float canvas_w, canvas_h; /* canvas size as of the last canvas frame */

    /* Drag state: dragging is non-zero from a tool's press until its release or cancel. At most
     * one of the stroke pointers is set (the eyedropper and a new selection use none). */
    int dragging;
    PencilStroke *stroke;
    ShapeStroke *shape;
    MoveStroke *move;
    int drag_x, drag_y;     /* cel under the mouse at the last drag step */
    int anchor_x, anchor_y; /* cel under the mouse at the press */

    /* Rectangular selection in cel coordinates, always inside the sprite. While a selection is
     * being moved (move != NULL) it stays at its origin and move_dx/dy hold the drag offset. */
    int has_selection;
    CompositeRect selection;
    int selecting;     /* dragging out a new selection */
    int had_selection; /* a selection existed when the current selecting drag began */
    int move_dx, move_dy;

    Clip *clipboard; /* last copied / cut selection, NULL until there is one */

    int hover_x, hover_y; /* cel under the mouse, valid when hovering is non-zero */
    int hovering;

    SDL_Texture *texture;
    Rgba *pixels; /* composited frame, sprite_width x sprite_height */
    int dirty;
    CompositeRect dirty_rect;
} Editor;

/* Returns NULL on failure. */
Editor *editor_create(SDL_Renderer *renderer, int width, int height);
void editor_destroy(Editor *editor);

Rgba editor_color(const Editor *editor);

void editor_set_color(Editor *editor, Rgba color);

/* Sprite palette helpers (the palette is not part of undo). Add puts the foreground color in the
 * palette unless it is already there and returns its index, or -1 if the palette is full. Pick
 * makes a swatch the foreground color. Remove deletes a swatch; the selection stays on the same
 * color, or is cleared when that color was removed. Out-of-range indexes are ignored. */
int editor_palette_add(Editor *editor);
void editor_palette_pick(Editor *editor, int index);
void editor_palette_remove(Editor *editor, int index);

/* Exchanges the foreground and background colors. */
void editor_swap_colors(Editor *editor);

/* Switches tools (does nothing while a tool is being dragged) and ends any temporary pick. */
void editor_select_tool(Editor *editor, ToolKind tool);

/* While `on` is non-zero the eyedropper is the active tool, and the previous tool comes back once
 * it turns zero. Meant to be called every frame with the state of the modifier key; does nothing
 * while a tool is being dragged, so a pick that ends mid-drag is undone on a later call. */
void editor_temp_pick(Editor *editor, int on);

/* Marks cels in the inclusive box (x0, y0)-(x1, y1) for re-compositing. */
void editor_mark_dirty(Editor *editor, int x0, int y0, int x1, int y1);
void editor_mark_all_dirty(Editor *editor);

/* Re-composites the dirty region and uploads it to the texture. */
void editor_update_texture(Editor *editor);

/* Zooms one step in (direction > 0) or out (< 0) around the canvas center. */
void editor_zoom_step(Editor *editor, int direction);

/* Non-zero while a tool is being dragged (undo / redo / layer changes are held off meanwhile). */
int editor_stroking(const Editor *editor);

/* The cel of the active layer and frame; NULL if there is none. */
Cel *editor_active_cel(Editor *editor);

/* Undo / redo; they do nothing while a stroke is being drawn. */
void editor_undo(Editor *editor);
void editor_redo(Editor *editor);

/* Layer operations (all do nothing while a tool is being dragged). Each is one undo step. */
void editor_add_layer(Editor *editor);
void editor_remove_layer(Editor *editor, int index);
void editor_move_layer(Editor *editor, int from, int to);
void editor_set_layer_visible(Editor *editor, int index, int visible);

/* Duplicate puts a copy above the layer and makes it active; merge draws the layer onto the one
 * below it (both must be visible). If the merged layer was active the one below becomes active,
 * otherwise the active layer keeps pointing at the same layer. */
void editor_duplicate_layer(Editor *editor, int index);
void editor_merge_layer_down(Editor *editor, int index);

/* Records a name / opacity edit made live with layer_set_*: the layer currently holds the edited
 * value, `before_*` is what it held when the edit began. One undo step; nothing if unchanged. */
void editor_commit_layer_name(Editor *editor, int index, const char *before_name);
void editor_commit_layer_opacity(Editor *editor, int index, uint8_t before_opacity);

/* Selection helpers. Delete clears the selected pixels (one undo step). */
void editor_select_all(Editor *editor);
void editor_clear_selection(Editor *editor);
void editor_delete_selection(Editor *editor);

/* Makes `r`, clipped to the sprite, the selection; drops the selection if nothing is left. */
void editor_set_selection_clipped(Editor *editor, CompositeRect r);

/* Copy / cut put the selected pixels of the active layer on the clipboard (cut also clears them,
 * one undo step). They return non-zero if something was copied; cut needs a visible layer. */
int editor_copy_selection(Editor *editor);
int editor_cut_selection(Editor *editor);

/* Pastes the clipboard as one undo step with its corner on the hovered cel, else on the selection
 * corner, else at (0, 0); the pasted area becomes the selection and the select tool is chosen so
 * it can be dragged into place. Does nothing without a clipboard or a visible layer. */
void editor_paste(Editor *editor);

/* Flips (horizontal when non-zero, else vertical) or turns (clockwise when non-zero, else
 * counter-clockwise) the selected pixels in place, one undo step. A rotated selection keeps its
 * center; whatever lands outside the sprite is lost. */
void editor_flip_selection(Editor *editor, int horizontal);
void editor_rotate_selection(Editor *editor, int clockwise);

#endif
