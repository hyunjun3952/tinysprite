#include "tools.h"

#include "tinysprite/bucket.h"
#include "tinysprite/eyedropper.h"

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int clamp_int(int value, int low, int high)
{
    return max_int(low, min_int(value, high));
}

/* Marks a stroke's bounding box dirty. Mirrored copies and pixel-perfect corrections land outside
 * the box of the new points, so those cases re-composite more. */
static void mark_stroke(Editor *editor, int x0, int y0, int x1, int y1)
{
    if (editor->mirror_x || editor->mirror_y) {
        editor_mark_all_dirty(editor);
    } else {
        editor_mark_dirty(editor, min_int(x0, x1) - 1, min_int(y0, y1) - 1, max_int(x0, x1) + 1,
                          max_int(y0, y1) + 1);
    }
}

static void mark_rect(Editor *editor, CompositeRect r, int dx, int dy)
{
    editor_mark_dirty(editor, r.x + dx, r.y + dy, r.x + dx + r.w - 1, r.y + dy + r.h - 1);
}

static int inside_selection(const Editor *editor, int x, int y)
{
    const CompositeRect *r = &editor->selection;
    return editor->has_selection && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

static ShapeKind shape_kind(ToolKind tool)
{
    switch (tool) {
    case TOOL_RECT:
        return SHAPE_RECT;
    case TOOL_ELLIPSE:
        return SHAPE_ELLIPSE;
    default:
        return SHAPE_LINE;
    }
}

static void pick_color(Editor *editor, int cx, int cy)
{
    Rgba color;
    if (eyedropper_pick_sprite(editor->sprite, editor->frame, cx, cy, &color) == 0) {
        editor->color[0] = (float)color.r / 255.0f;
        editor->color[1] = (float)color.g / 255.0f;
        editor->color[2] = (float)color.b / 255.0f;
        editor->color[3] = (float)color.a / 255.0f;
    }
}

static void update_selection(Editor *editor, int cx, int cy)
{
    int w = sprite_width(editor->sprite);
    int h = sprite_height(editor->sprite);
    int x0 = clamp_int(min_int(editor->anchor_x, cx), 0, w - 1);
    int y0 = clamp_int(min_int(editor->anchor_y, cy), 0, h - 1);
    int x1 = clamp_int(max_int(editor->anchor_x, cx), 0, w - 1);
    int y1 = clamp_int(max_int(editor->anchor_y, cy), 0, h - 1);
    editor->selection = (CompositeRect){x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

static void press_select(Editor *editor, int cx, int cy)
{
    if (inside_selection(editor, cx, cy)) {
        Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
        Cel *cel = editor_active_cel(editor);
        if (!cel || !layer_visible(layer)) {
            return;
        }
        const CompositeRect *r = &editor->selection;
        editor->move = move_stroke_begin(editor->history, cel, r->x, r->y, r->w, r->h);
        if (!editor->move) {
            return;
        }
        editor->move_dx = 0;
        editor->move_dy = 0;
    } else {
        editor->selecting = 1;
        editor->has_selection = 1;
        update_selection(editor, cx, cy);
    }
    editor->dragging = 1;
}

void tools_press(Editor *editor, int cx, int cy)
{
    if (editor->dragging) {
        return;
    }
    editor->anchor_x = cx;
    editor->anchor_y = cy;
    editor->drag_x = cx;
    editor->drag_y = cy;

    if (editor->tool == TOOL_EYEDROPPER) {
        pick_color(editor, cx, cy);
        editor->dragging = 1;
        return;
    }
    if (editor->tool == TOOL_SELECT) {
        if (!editor->has_selection || !inside_selection(editor, cx, cy)) {
            /* A new selection starts at the clamped press point. */
            editor->anchor_x = clamp_int(cx, 0, sprite_width(editor->sprite) - 1);
            editor->anchor_y = clamp_int(cy, 0, sprite_height(editor->sprite) - 1);
            editor->drag_x = editor->anchor_x;
            editor->drag_y = editor->anchor_y;
        }
        press_select(editor, cx, cy);
        return;
    }

    Layer *layer = sprite_layer(editor->sprite, editor->active_layer);
    Cel *cel = editor_active_cel(editor);
    if (!cel || !layer_visible(layer)) {
        return;
    }
    Rgba color = editor_color(editor);
    switch (editor->tool) {
    case TOOL_PENCIL:
    case TOOL_ERASER: {
        PencilOptions options = {editor->pixel_perfect, editor->mirror_x, editor->mirror_y};
        editor->stroke = pencil_stroke_begin_ex(editor->history, cel, color,
                                                editor->tool == TOOL_ERASER, options, cx, cy);
        if (editor->stroke) {
            editor->dragging = 1;
            mark_stroke(editor, cx, cy, cx, cy);
        }
        break;
    }
    case TOOL_BUCKET:
        /* A single click: no drag state. */
        if (bucket_fill(editor->history, cel, cx, cy, color) == 0) {
            editor_mark_all_dirty(editor);
        }
        break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
        editor->shape = shape_stroke_begin(editor->history, cel, shape_kind(editor->tool),
                                           editor->shape_filled, color, cx, cy);
        if (editor->shape) {
            editor->dragging = 1;
            mark_stroke(editor, cx, cy, cx, cy);
        }
        break;
    default:
        break;
    }
}

void tools_drag(Editor *editor, int cx, int cy)
{
    if (!editor->dragging) {
        return;
    }
    if (editor->tool == TOOL_EYEDROPPER) {
        pick_color(editor, cx, cy);
        return;
    }
    if (editor->selecting) {
        update_selection(editor, cx, cy);
        editor->drag_x = clamp_int(cx, 0, sprite_width(editor->sprite) - 1);
        editor->drag_y = clamp_int(cy, 0, sprite_height(editor->sprite) - 1);
        return;
    }
    if (cx == editor->drag_x && cy == editor->drag_y) {
        return;
    }

    if (editor->stroke) {
        pencil_stroke_extend(editor->stroke, cx, cy);
        mark_stroke(editor, editor->drag_x, editor->drag_y, cx, cy);
    } else if (editor->shape) {
        shape_stroke_update(editor->shape, cx, cy);
        /* The old and new shapes both lie in a box spanned by the anchor and the cursor. */
        mark_stroke(editor, editor->anchor_x, editor->anchor_y, editor->drag_x, editor->drag_y);
        mark_stroke(editor, editor->anchor_x, editor->anchor_y, cx, cy);
    } else if (editor->move) {
        mark_rect(editor, editor->selection, editor->move_dx, editor->move_dy);
        editor->move_dx = cx - editor->anchor_x;
        editor->move_dy = cy - editor->anchor_y;
        move_stroke_update(editor->move, editor->move_dx, editor->move_dy);
        mark_rect(editor, editor->selection, 0, 0);
        mark_rect(editor, editor->selection, editor->move_dx, editor->move_dy);
    }
    editor->drag_x = cx;
    editor->drag_y = cy;
}

static void finish_move(Editor *editor)
{
    move_stroke_end(editor->move);
    editor->move = NULL;

    /* The selection follows its pixels; the part that left the sprite is gone. */
    int w = sprite_width(editor->sprite);
    int h = sprite_height(editor->sprite);
    CompositeRect r = editor->selection;
    int x0 = max_int(r.x + editor->move_dx, 0);
    int y0 = max_int(r.y + editor->move_dy, 0);
    int x1 = min_int(r.x + editor->move_dx + r.w, w);
    int y1 = min_int(r.y + editor->move_dy + r.h, h);
    if (x0 < x1 && y0 < y1) {
        editor->selection = (CompositeRect){x0, y0, x1 - x0, y1 - y0};
    } else {
        editor->has_selection = 0;
    }
    editor->move_dx = 0;
    editor->move_dy = 0;
}

void tools_release(Editor *editor)
{
    if (!editor->dragging) {
        return;
    }
    if (editor->stroke) {
        pencil_stroke_end(editor->stroke);
        editor->stroke = NULL;
    } else if (editor->shape) {
        shape_stroke_end(editor->shape);
        editor->shape = NULL;
    } else if (editor->move) {
        finish_move(editor);
    } else if (editor->selecting) {
        editor->selecting = 0;
        /* A click without dragging deselects. */
        if (editor->drag_x == editor->anchor_x && editor->drag_y == editor->anchor_y) {
            editor->has_selection = 0;
        }
    }
    editor->dragging = 0;
}

void tools_cancel(Editor *editor)
{
    if (!editor->dragging) {
        return;
    }
    pencil_stroke_cancel(editor->stroke);
    shape_stroke_cancel(editor->shape);
    move_stroke_cancel(editor->move);
    if (editor->selecting) {
        editor->has_selection = 0;
    }
    editor->stroke = NULL;
    editor->shape = NULL;
    editor->move = NULL;
    editor->selecting = 0;
    editor->move_dx = 0;
    editor->move_dy = 0;
    editor->dragging = 0;
    editor_mark_all_dirty(editor);
}

const char *tools_name(ToolKind tool)
{
    static const char *const names[TOOL_COUNT] = {"Pencil", "Eraser", "Bucket",  "Picker",
                                                  "Line",   "Rect",   "Ellipse", "Select"};
    return (unsigned)tool < TOOL_COUNT ? names[tool] : "";
}
