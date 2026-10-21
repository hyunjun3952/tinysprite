#include "editor/tools.h"

#include <string.h>

#include "tinysprite/bucket.h"
#include "tinysprite/eyedropper.h"
#include "tinysprite/util.h"

/* Marks a stroke's bounding box dirty, `pad` cels beyond it. Mirrored copies land outside the box
 * of the new points, so symmetry re-composites everything. */
static void mark_stroke(Editor *editor, int x0, int y0, int x1, int y1, int pad)
{
    if (symmetry_count(editor->symmetry) > 1) {
        editor_mark_all_dirty(editor);
    } else {
        editor_mark_dirty(editor, rect_grow(rect_from_corners(x0, y0, x1, y1), pad));
    }
}

/* The pencil and eraser paint a brush around every point, which reaches up to the brush size away.
 */
static void mark_brush_stroke(Editor *editor, int x0, int y0, int x1, int y1)
{
    mark_stroke(editor, x0, y0, x1, y1, max_int(1, editor->brush_size));
}

/* The selection's rectangle shifted by (dx, dy). */
static void mark_selection(Editor *editor, int dx, int dy)
{
    Rect r = editor->selection;
    r.x += dx;
    r.y += dy;
    editor_mark_dirty(editor, r);
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
        editor_set_color(editor, color);
    }
}

static int clamp_x(const Editor *editor, int x)
{
    return clamp_int(x, 0, sprite_width(editor->sprite) - 1);
}

static int clamp_y(const Editor *editor, int y)
{
    return clamp_int(y, 0, sprite_height(editor->sprite) - 1);
}

/* The selection spans the anchor and (cx, cy); the anchor is always inside the sprite, so the
 * clipped rectangle is never empty. */
static void update_selection(Editor *editor, int cx, int cy)
{
    Rect box = rect_from_corners(editor->anchor_x, editor->anchor_y, cx, cy);
    editor->selection = rect_clip(box, sprite_width(editor->sprite), sprite_height(editor->sprite));
}

static void press_select(Editor *editor, int cx, int cy)
{
    if (editor->has_selection && rect_contains(editor->selection, cx, cy)) {
        Cel *cel = editor_editable_cel(editor);
        if (!cel) {
            return;
        }
        editor->stroke.move = move_stroke_begin(editor->history, cel, editor->selection);
        if (!editor->stroke.move) {
            return;
        }
        editor->move_dx = 0;
        editor->move_dy = 0;
        editor->drag = DRAG_MOVE;
        return;
    }
    /* A new selection starts at the clamped press point. */
    editor->anchor_x = clamp_x(editor, cx);
    editor->anchor_y = clamp_y(editor, cy);
    editor->drag_x = editor->anchor_x;
    editor->drag_y = editor->anchor_y;
    /* Remember whether a selection existed: a plain click on empty canvas selects one cel, but a
     * click that merely dismisses an old selection must not start a new one. */
    editor->had_selection = editor->has_selection;
    editor->has_selection = true;
    update_selection(editor, cx, cy);
    editor->drag = DRAG_SELECT;
}

static void press_draw(Editor *editor, int cx, int cy)
{
    Cel *cel = editor_editable_cel(editor);
    if (!cel) {
        return;
    }
    bool erasing = editor->tool == TOOL_ERASER;
    Rgba color = editor_color(editor);
    if (!erasing) {
        recent_colors_push(&editor->recent, color);
    }
    switch (editor->tool) {
    case TOOL_PENCIL:
    case TOOL_ERASER: {
        PencilOptions options = {editor->pixel_perfect, editor->symmetry, editor->brush_size,
                                 editor->brush_round};
        editor->stroke.pencil = pencil_stroke_begin(
            editor->history, cel, erasing ? RGBA_TRANSPARENT : color, options, cx, cy);
        if (editor->stroke.pencil) {
            editor->drag = DRAG_PENCIL;
            mark_brush_stroke(editor, cx, cy, cx, cy);
        }
        break;
    }
    case TOOL_BUCKET:
        /* A single click: no drag state. */
        if (bucket_fill(editor->history, cel, cx, cy, color, editor->symmetry) == 0) {
            editor_mark_all_dirty(editor);
        }
        break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE: {
        ShapeStyle style = {shape_kind(editor->tool), editor->shape_filled, color,
                            editor->symmetry};
        editor->stroke.shape = shape_stroke_begin(editor->history, cel, style, (ToolPoint){cx, cy});
        if (editor->stroke.shape) {
            editor->drag = DRAG_SHAPE;
            mark_stroke(editor, cx, cy, cx, cy, 1);
        }
        break;
    }
    case TOOL_EYEDROPPER:
    case TOOL_SELECT:
    case TOOL_COUNT:
        break;
    }
}

void tools_press(Editor *editor, int cx, int cy)
{
    if (editor_stroking(editor)) {
        return;
    }
    editor->anchor_x = cx;
    editor->anchor_y = cy;
    editor->drag_x = cx;
    editor->drag_y = cy;

    switch (editor->tool) {
    case TOOL_EYEDROPPER:
        pick_color(editor, cx, cy);
        editor->drag = DRAG_PICK;
        break;
    case TOOL_SELECT:
        press_select(editor, cx, cy);
        break;
    default:
        press_draw(editor, cx, cy);
        break;
    }
}

/* Applies one drag step to the live stroke; false if it ran out of memory. */
static bool extend_stroke(Editor *editor, int cx, int cy)
{
    switch (editor->drag) {
    case DRAG_PENCIL:
        if (pencil_stroke_extend(editor->stroke.pencil, cx, cy) != 0) {
            return false;
        }
        mark_brush_stroke(editor, editor->drag_x, editor->drag_y, cx, cy);
        return true;
    case DRAG_SHAPE:
        if (shape_stroke_update(editor->stroke.shape, cx, cy) != 0) {
            return false;
        }
        /* The old and new shapes both lie in a box spanned by the anchor and the cursor. */
        mark_stroke(editor, editor->anchor_x, editor->anchor_y, editor->drag_x, editor->drag_y, 1);
        mark_stroke(editor, editor->anchor_x, editor->anchor_y, cx, cy, 1);
        return true;
    case DRAG_MOVE:
        mark_selection(editor, editor->move_dx, editor->move_dy);
        editor->move_dx = cx - editor->anchor_x;
        editor->move_dy = cy - editor->anchor_y;
        if (move_stroke_update(editor->stroke.move, editor->move_dx, editor->move_dy) != 0) {
            return false;
        }
        mark_selection(editor, 0, 0);
        mark_selection(editor, editor->move_dx, editor->move_dy);
        return true;
    case DRAG_NONE:
    case DRAG_PICK:
    case DRAG_SELECT:
        break;
    }
    return true;
}

void tools_drag(Editor *editor, int cx, int cy)
{
    switch (editor->drag) {
    case DRAG_NONE:
        return;
    case DRAG_PICK:
        pick_color(editor, cx, cy);
        return;
    case DRAG_SELECT:
        update_selection(editor, cx, cy);
        editor->drag_x = clamp_x(editor, cx);
        editor->drag_y = clamp_y(editor, cy);
        return;
    case DRAG_PENCIL:
    case DRAG_SHAPE:
    case DRAG_MOVE:
        break;
    }
    if (cx == editor->drag_x && cy == editor->drag_y) {
        return;
    }
    if (!extend_stroke(editor, cx, cy)) {
        tools_cancel(editor);
        return;
    }
    editor->drag_x = cx;
    editor->drag_y = cy;
}

/* The selection follows its pixels; the part that left the sprite is gone. */
static void finish_move(Editor *editor)
{
    if (move_stroke_end(editor->stroke.move) != 0) {
        editor_mark_all_dirty(editor);
    } else {
        Rect moved = editor->selection;
        moved.x += editor->move_dx;
        moved.y += editor->move_dy;
        editor_set_selection_clipped(editor, moved);
    }
    editor->move_dx = 0;
    editor->move_dy = 0;
}

/* A click without dragging selects the single cel under it, unless it only dismisses an existing
 * selection. */
static void finish_select(Editor *editor)
{
    if (editor->drag_x == editor->anchor_x && editor->drag_y == editor->anchor_y &&
        editor->had_selection) {
        editor->has_selection = false;
    }
    editor->had_selection = false;
}

void tools_release(Editor *editor)
{
    switch (editor->drag) {
    case DRAG_PENCIL:
        if (pencil_stroke_end(editor->stroke.pencil) != 0) {
            editor_mark_all_dirty(editor);
        }
        break;
    case DRAG_SHAPE:
        if (shape_stroke_end(editor->stroke.shape) != 0) {
            editor_mark_all_dirty(editor);
        }
        break;
    case DRAG_MOVE:
        finish_move(editor);
        break;
    case DRAG_SELECT:
        finish_select(editor);
        break;
    case DRAG_NONE:
    case DRAG_PICK:
        break;
    }
    memset(&editor->stroke, 0, sizeof(editor->stroke));
    editor->drag = DRAG_NONE;
}

void tools_cancel(Editor *editor)
{
    switch (editor->drag) {
    case DRAG_NONE:
        return;
    case DRAG_PENCIL:
        pencil_stroke_cancel(editor->stroke.pencil);
        break;
    case DRAG_SHAPE:
        shape_stroke_cancel(editor->stroke.shape);
        break;
    case DRAG_MOVE:
        move_stroke_cancel(editor->stroke.move);
        break;
    case DRAG_SELECT:
        editor->has_selection = false;
        break;
    case DRAG_PICK:
        break;
    }
    memset(&editor->stroke, 0, sizeof(editor->stroke));
    editor->move_dx = 0;
    editor->move_dy = 0;
    editor->drag = DRAG_NONE;
    editor_mark_all_dirty(editor);
}
