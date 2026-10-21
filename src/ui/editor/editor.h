#ifndef TINYSPRITE_UI_EDITOR_H
#define TINYSPRITE_UI_EDITOR_H

#include <SDL.h>
#include <stdbool.h>

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

/* What the mouse is doing between a tool's press and its release. */
typedef enum DragKind {
    DRAG_NONE,
    DRAG_PICK,   /* eyedropper: samples a color while held */
    DRAG_PENCIL, /* pencil and eraser stroke */
    DRAG_SHAPE,  /* line, rectangle or ellipse */
    DRAG_MOVE,   /* moving the selected pixels */
    DRAG_SELECT, /* dragging out a new selection */
} DragKind;

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
    bool temp_pick;       /* the eyedropper is active only while a modifier is held */
    ToolKind pick_return; /* the tool to go back to when the temporary pick ends */
    bool show_grid;
    int active_layer;
    int frame;

    /* Tool options: pixel_perfect / brush_* apply to pencil and eraser, shape_filled to the
     * rectangle and ellipse (symmetry also reaches shapes and the bucket). */
    int brush_size; /* 1..PENCIL_BRUSH_MAX */
    bool brush_round;
    bool pixel_perfect;
    Symmetry symmetry;
    bool shape_filled;

    Viewport view;
    bool needs_fit;           /* fit the sprite to the canvas on the next canvas frame */
    bool auto_fit;            /* the view is still the fitted one: refit when the canvas resizes */
    float canvas_w, canvas_h; /* canvas size as of the last canvas frame */

    /* Drag state. `stroke` holds the live stroke of the drag kind that owns one (the eyedropper
     * and a new selection own none), so at most one exists. */
    DragKind drag;
    union {
        PencilStroke *pencil;
        ShapeStroke *shape;
        MoveStroke *move;
    } stroke;
    int drag_x, drag_y;     /* cel under the mouse at the last drag step */
    int anchor_x, anchor_y; /* cel under the mouse at the press */

    /* Rectangular selection in cel coordinates, always inside the sprite. While a selection is
     * being moved (DRAG_MOVE) it stays at its origin and move_dx/dy hold the drag offset. */
    bool has_selection;
    Rect selection;
    bool had_selection; /* a selection existed when the current DRAG_SELECT began */
    int move_dx, move_dy;

    Clip *clipboard; /* last copied / cut selection, NULL until there is one */

    int hover_x, hover_y; /* cel under the mouse, valid when hovering is true */
    bool hovering;

    /* The layers panel edits a name or opacity live and records one undo step on release; these
     * hold the values from before that edit began. */
    struct {
        char name_before[LAYER_NAME_MAX + 1];
        uint8_t opacity_before;
    } layer_edit;

    SDL_Texture *texture;
    Rgba *pixels; /* composited frame, sprite_width x sprite_height */
    bool dirty;
    Rect dirty_rect;
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

/* While `on` is true the eyedropper is the active tool, and the previous tool comes back once it
 * turns false. Meant to be called every frame with the state of the modifier key; does nothing
 * while a tool is being dragged, so a pick that ends mid-drag is undone on a later call. */
void editor_temp_pick(Editor *editor, bool on);

/* Marks cels for re-compositing; `area` must not be empty. */
void editor_mark_dirty(Editor *editor, Rect area);
void editor_mark_all_dirty(Editor *editor);

/* Re-composites the dirty region and uploads it to the texture. */
void editor_update_texture(Editor *editor);

/* Zooms one step in (direction > 0) or out (< 0) around the canvas center. */
void editor_zoom_step(Editor *editor, int direction);

/* True while a tool is being dragged (undo / redo / layer changes are held off meanwhile). */
bool editor_stroking(const Editor *editor);

/* True when the mouse is over a cel of the sprite. */
bool editor_hover_in_sprite(const Editor *editor);

/* The cel of the active layer and frame; NULL if there is none. */
Cel *editor_active_cel(Editor *editor);

/* The active cel when a tool may change it; NULL while a tool is being dragged or when the
 * layer is hidden. */
Cel *editor_editable_cel(Editor *editor);

/* Undo / redo; they do nothing while a stroke is being drawn. */
void editor_undo(Editor *editor);
void editor_redo(Editor *editor);

/* Layer operations (all do nothing while a tool is being dragged). Each is one undo step. */
void editor_add_layer(Editor *editor);
void editor_remove_layer(Editor *editor, int index);
void editor_move_layer(Editor *editor, int from, int to);
void editor_set_layer_visible(Editor *editor, int index, bool visible);

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
void editor_set_selection_clipped(Editor *editor, Rect r);

/* Copy / cut put the selected pixels of the active layer on the clipboard (cut also clears them,
 * one undo step). They return true if something was copied; cut needs a visible layer. */
bool editor_copy_selection(Editor *editor);
bool editor_cut_selection(Editor *editor);

/* Pastes the clipboard as one undo step with its corner on the hovered cel, else on the selection
 * corner, else at (0, 0); the pasted area becomes the selection and the select tool is chosen so
 * it can be dragged into place. Does nothing without a clipboard or a visible layer. */
void editor_paste(Editor *editor);

/* Flips (horizontally or else vertically) or turns (clockwise or else counter-clockwise) the
 * selected pixels in place, one undo step. A rotated selection keeps its center; whatever lands
 * outside the sprite is lost. */
void editor_flip_selection(Editor *editor, bool horizontal);
void editor_rotate_selection(Editor *editor, bool clockwise);

#endif
