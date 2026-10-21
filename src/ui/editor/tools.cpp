#include "editor/tools.h"

#include <algorithm>

#include "tinysprite/bucket.h"
#include "tinysprite/eyedropper.h"
#include "tinysprite/util.h"

namespace
{

/* Marks a stroke's bounding box dirty, `pad` cels beyond it. Mirrored copies land outside the box
 * of the new points, so symmetry re-composites everything. */
void markStroke(Editor &editor, int x0, int y0, int x1, int y1, int pad)
{
    if (symmetry_count(editor.options().symmetry) > 1) {
        editor.markAllDirty();
    } else {
        editor.markDirty(rect_grow(rect_from_corners(x0, y0, x1, y1), pad));
    }
}

/* The pencil and eraser paint a brush around every point, which reaches up to the brush size away.
 */
void markBrushStroke(Editor &editor, int x0, int y0, int x1, int y1)
{
    markStroke(editor, x0, y0, x1, y1, std::max(1, editor.options().brush_size));
}

ShapeKind shapeKind(ToolKind tool)
{
    switch (tool) {
    case ToolKind::Rect:
        return SHAPE_RECT;
    case ToolKind::Ellipse:
        return SHAPE_ELLIPSE;
    default:
        return SHAPE_LINE;
    }
}

void pickColor(Editor &editor, int cx, int cy)
{
    Rgba color;
    if (eyedropper_pick_sprite(editor.sprite(), editor.frame(), cx, cy, &color) == 0) {
        editor.setForeground(color);
    }
}

int clampX(const Editor &editor, int x)
{
    return clamp_int(x, 0, sprite_width(editor.sprite()) - 1);
}

int clampY(const Editor &editor, int y)
{
    return clamp_int(y, 0, sprite_height(editor.sprite()) - 1);
}

/* The eyedropper: samples a color while the button is held. */
class PickDrag : public Drag
{
  public:
    PickDrag(Editor &editor, int cx, int cy) : Drag(editor, cx, cy)
    {
        pickColor(editor, cx, cy);
    }

    bool update(int cx, int cy) override
    {
        pickColor(editor_, cx, cy);
        return true;
    }
    void finish() override
    {
    }
    void cancel() override
    {
    }
};

/* Pencil and eraser stroke. */
class PencilDrag : public Drag
{
  public:
    PencilDrag(Editor &editor, int cx, int cy, PencilStroke *stroke)
        : Drag(editor, cx, cy), stroke_(stroke)
    {
        markBrushStroke(editor, cx, cy, cx, cy);
    }

    bool update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return true;
        }
        if (pencil_stroke_extend(stroke_, cx, cy) != 0) {
            return false;
        }
        markBrushStroke(editor_, lastX_, lastY_, cx, cy);
        lastX_ = cx;
        lastY_ = cy;
        return true;
    }
    void finish() override
    {
        if (pencil_stroke_end(stroke_) != 0) {
            editor_.markAllDirty();
        }
    }
    void cancel() override
    {
        pencil_stroke_cancel(stroke_);
    }

  private:
    PencilStroke *stroke_;
};

/* Line, rectangle or ellipse. */
class ShapeDrag : public Drag
{
  public:
    ShapeDrag(Editor &editor, int cx, int cy, ShapeStroke *stroke)
        : Drag(editor, cx, cy), stroke_(stroke)
    {
        markStroke(editor, cx, cy, cx, cy, 1);
    }

    bool update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return true;
        }
        if (shape_stroke_update(stroke_, cx, cy) != 0) {
            return false;
        }
        /* The old and new shapes both lie in a box spanned by the anchor and the cursor. */
        markStroke(editor_, anchorX_, anchorY_, lastX_, lastY_, 1);
        markStroke(editor_, anchorX_, anchorY_, cx, cy, 1);
        lastX_ = cx;
        lastY_ = cy;
        return true;
    }
    void finish() override
    {
        if (shape_stroke_end(stroke_) != 0) {
            editor_.markAllDirty();
        }
    }
    void cancel() override
    {
        shape_stroke_cancel(stroke_);
    }

  private:
    ShapeStroke *stroke_;
};

/* Moving the selected pixels. */
class MoveDrag : public Drag
{
  public:
    MoveDrag(Editor &editor, int cx, int cy, MoveStroke *stroke)
        : Drag(editor, cx, cy), stroke_(stroke)
    {
        editor.setMoveOffset(0, 0);
    }

    bool update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return true;
        }
        markSelection(editor_.moveDx(), editor_.moveDy());
        editor_.setMoveOffset(cx - anchorX_, cy - anchorY_);
        if (move_stroke_update(stroke_, editor_.moveDx(), editor_.moveDy()) != 0) {
            return false;
        }
        markSelection(0, 0);
        markSelection(editor_.moveDx(), editor_.moveDy());
        lastX_ = cx;
        lastY_ = cy;
        return true;
    }

    /* The selection follows its pixels; the part that left the sprite is gone. */
    void finish() override
    {
        if (move_stroke_end(stroke_) != 0) {
            editor_.markAllDirty();
        } else {
            Rect moved = editor_.selection();
            moved.x += editor_.moveDx();
            moved.y += editor_.moveDy();
            editor_.setSelectionClipped(moved);
        }
        editor_.setMoveOffset(0, 0);
    }
    void cancel() override
    {
        move_stroke_cancel(stroke_);
    }

  private:
    /* The selection's rectangle shifted by (dx, dy). */
    void markSelection(int dx, int dy)
    {
        Rect r = editor_.selection();
        r.x += dx;
        r.y += dy;
        editor_.markDirty(r);
    }

    MoveStroke *stroke_;
};

/* Dragging out a new selection. */
class SelectDrag : public Drag
{
  public:
    /* The anchor is the clamped press point, so it is always inside the sprite and the clipped
     * rectangle is never empty. */
    SelectDrag(Editor &editor, int cx, int cy)
        : Drag(editor, clampX(editor, cx), clampY(editor, cy)), hadSelection_(editor.hasSelection())
    {
        /* Remember whether a selection existed: a plain click on empty canvas selects one cel, but
         * a click that merely dismisses an old selection must not start a new one. */
        updateSelection(cx, cy);
    }

    bool update(int cx, int cy) override
    {
        updateSelection(cx, cy);
        lastX_ = clampX(editor_, cx);
        lastY_ = clampY(editor_, cy);
        return true;
    }

    /* A click without dragging selects the single cel under it, unless it only dismisses an
     * existing selection. */
    void finish() override
    {
        if (lastX_ == anchorX_ && lastY_ == anchorY_ && hadSelection_) {
            editor_.dropSelection();
        }
    }
    void cancel() override
    {
        editor_.dropSelection();
    }

  private:
    /* The selection spans the anchor and (cx, cy). */
    void updateSelection(int cx, int cy)
    {
        Rect box = rect_from_corners(anchorX_, anchorY_, cx, cy);
        editor_.setSelection(
            rect_clip(box, sprite_width(editor_.sprite()), sprite_height(editor_.sprite())));
    }

    bool hadSelection_;
};

std::unique_ptr<Drag> beginSelect(Editor &editor, int cx, int cy)
{
    if (editor.hasSelection() && rect_contains(editor.selection(), cx, cy)) {
        Cel *cel = editor.editableCel();
        if (!cel) {
            return nullptr;
        }
        MoveStroke *stroke = move_stroke_begin(editor.history(), cel, editor.selection());
        if (!stroke) {
            return nullptr;
        }
        return std::make_unique<MoveDrag>(editor, cx, cy, stroke);
    }
    return std::make_unique<SelectDrag>(editor, cx, cy);
}

std::unique_ptr<Drag> beginDraw(Editor &editor, int cx, int cy)
{
    Cel *cel = editor.editableCel();
    if (!cel) {
        return nullptr;
    }
    bool erasing = editor.tool() == ToolKind::Eraser;
    Rgba color = editor.foreground();
    if (!erasing) {
        editor.recordRecent(color);
    }
    switch (editor.tool()) {
    case ToolKind::Pencil:
    case ToolKind::Eraser: {
        PencilOptions options = {editor.options().pixel_perfect, editor.options().symmetry,
                                 editor.options().brush_size, editor.options().brush_round};
        PencilStroke *stroke = pencil_stroke_begin(
            editor.history(), cel, erasing ? Rgba{0, 0, 0, 0} : color, options, cx, cy);
        return stroke ? std::make_unique<PencilDrag>(editor, cx, cy, stroke) : nullptr;
    }
    case ToolKind::Bucket:
        /* A single click: no drag. */
        if (bucket_fill(editor.history(), cel, cx, cy, color, editor.options().symmetry) == 0) {
            editor.markAllDirty();
        }
        return nullptr;
    case ToolKind::Line:
    case ToolKind::Rect:
    case ToolKind::Ellipse: {
        ShapeStyle style = {shapeKind(editor.tool()), editor.options().shape_filled, color,
                            editor.options().symmetry};
        ShapeStroke *stroke = shape_stroke_begin(editor.history(), cel, style, ToolPoint{cx, cy});
        return stroke ? std::make_unique<ShapeDrag>(editor, cx, cy, stroke) : nullptr;
    }
    case ToolKind::Eyedropper:
    case ToolKind::Select:
    case ToolKind::Count:
        break;
    }
    return nullptr;
}

} // namespace

std::unique_ptr<Drag> beginDrag(Editor &editor, int cx, int cy)
{
    switch (editor.tool()) {
    case ToolKind::Eyedropper:
        return std::make_unique<PickDrag>(editor, cx, cy);
    case ToolKind::Select:
        return beginSelect(editor, cx, cy);
    default:
        return beginDraw(editor, cx, cy);
    }
}
