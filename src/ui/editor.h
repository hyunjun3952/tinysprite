#ifndef TINYSPRITE_UI_EDITOR_H
#define TINYSPRITE_UI_EDITOR_H

#include <SDL.h>

#include "tinysprite/composite.h"
#include "tinysprite/history.h"
#include "tinysprite/pencil.h"
#include "tinysprite/sprite.h"
#include "tinysprite/viewport.h"

typedef enum ToolKind {
    TOOL_PENCIL,
    TOOL_ERASER,
} ToolKind;

/* Everything the UI edits: the document, its undo history, the tool state and the CPU-composited
 * texture the canvas displays. Created once in main and passed to each panel. */
typedef struct Editor {
    Sprite *sprite;
    History *history;
    ToolKind tool;
    float color[4]; /* foreground color, straight alpha, 0..1 */
    int show_grid;
    int active_layer;
    int frame;

    Viewport view;
    int needs_fit; /* fit the sprite to the canvas on the next canvas frame */

    PencilStroke *stroke; /* non-NULL while the left mouse button draws */
    int stroke_x, stroke_y;

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

/* Marks cels in the inclusive box (x0, y0)-(x1, y1) for re-compositing. */
void editor_mark_dirty(Editor *editor, int x0, int y0, int x1, int y1);
void editor_mark_all_dirty(Editor *editor);

/* Re-composites the dirty region and uploads it to the texture. */
void editor_update_texture(Editor *editor);

/* Undo / redo; they do nothing while a stroke is being drawn. */
void editor_undo(Editor *editor);
void editor_redo(Editor *editor);

#endif
