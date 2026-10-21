#include "drag.h"
#include "editor.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "tinysprite/tools.h"

namespace
{

/* Marks a stroke's bounding box dirty, `pad` cels beyond it, together with its reflections. */
void markStroke(Editor &editor, int x0, int y0, int x1, int y1, int pad)
{
    Rect box = rect_grow(rect_from_corners(x0, y0, x1, y1), pad);
    const Symmetry &symmetry = editor.options().symmetry;
    int width = editor.sprite().width();
    int height = editor.sprite().height();
    for (int copy = 0; copy < symmetry_count(symmetry); copy++) {
        ToolPoint a = symmetry_copy(symmetry, width, height, {box.x, box.y}, copy);
        ToolPoint b =
            symmetry_copy(symmetry, width, height, {box.x + box.w - 1, box.y + box.h - 1}, copy);
        editor.markDirty(rect_from_corners(a.x, a.y, b.x, b.y));
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
        return ShapeKind::Rect;
    case ToolKind::Ellipse:
        return ShapeKind::Ellipse;
    case ToolKind::Triangle:
        return ShapeKind::Triangle;
    case ToolKind::Diamond:
        return ShapeKind::Diamond;
    case ToolKind::Hexagon:
        return ShapeKind::Hexagon;
    case ToolKind::Star:
        return ShapeKind::Star;
    case ToolKind::Cross:
        return ShapeKind::Cross;
    default:
        return ShapeKind::Line;
    }
}

void pickColor(Editor &editor, int cx, int cy)
{
    if (std::optional<Rgba> color =
            eyedropper_pick_sprite(editor.sprite(), editor.frame(), cx, cy)) {
        editor.setForeground(*color);
    }
}

int clampX(const Editor &editor, int x)
{
    return std::clamp(x, 0, editor.sprite().width() - 1);
}

int clampY(const Editor &editor, int y)
{
    return std::clamp(y, 0, editor.sprite().height() - 1);
}

/* The eyedropper: samples a color while the button is held. */
class PickDrag : public Drag
{
  public:
    PickDrag(Editor &editor, int cx, int cy) : Drag(editor, cx, cy)
    {
        pickColor(editor, cx, cy);
    }

    void update(int cx, int cy) override
    {
        pickColor(editor_, cx, cy);
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
    PencilDrag(Editor &editor, int cx, int cy, std::unique_ptr<PencilStroke> stroke)
        : Drag(editor, cx, cy), stroke_(std::move(stroke))
    {
        markBrushStroke(editor, cx, cy, cx, cy);
    }

    void update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return;
        }
        stroke_->extend(cx, cy);
        markBrushStroke(editor_, lastX_, lastY_, cx, cy);
        lastX_ = cx;
        lastY_ = cy;
    }
    void finish() override
    {
        if (!stroke_->end()) {
            editor_.markAllDirty();
        }
        editor_.setLastDrawPoint(lastX_, lastY_);
    }
    void cancel() override
    {
        stroke_->cancel();
    }

  private:
    std::unique_ptr<PencilStroke> stroke_;
};

/* Line, rectangle or ellipse. */
class ShapeDrag : public Drag
{
  public:
    ShapeDrag(Editor &editor, int cx, int cy, std::unique_ptr<ShapeStroke> stroke)
        : Drag(editor, cx, cy), stroke_(std::move(stroke))
    {
        markStroke(editor, cx, cy, cx, cy, 1);
    }

    void update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return;
        }
        stroke_->update(cx, cy);
        /* The old and new shapes both lie in a box spanned by the anchor and the cursor. */
        markStroke(editor_, anchorX_, anchorY_, lastX_, lastY_, 1);
        markStroke(editor_, anchorX_, anchorY_, cx, cy, 1);
        lastX_ = cx;
        lastY_ = cy;
    }
    void finish() override
    {
        if (!stroke_->end()) {
            editor_.markAllDirty();
        }
    }
    void cancel() override
    {
        stroke_->cancel();
    }

  private:
    std::unique_ptr<ShapeStroke> stroke_;
};

/* Moving the selected pixels. */
class MoveDrag : public Drag
{
  public:
    MoveDrag(Editor &editor, int cx, int cy, std::unique_ptr<MoveStroke> stroke)
        : Drag(editor, cx, cy), stroke_(std::move(stroke))
    {
        editor.setMoveOffset(0, 0);
    }

    void update(int cx, int cy) override
    {
        if (cx == lastX_ && cy == lastY_) {
            return;
        }
        markSelection(editor_.moveDx(), editor_.moveDy());
        editor_.setMoveOffset(cx - anchorX_, cy - anchorY_);
        stroke_->update(editor_.moveDx(), editor_.moveDy());
        markSelection(0, 0);
        markSelection(editor_.moveDx(), editor_.moveDy());
        lastX_ = cx;
        lastY_ = cy;
    }

    /* The selection follows its pixels; the part that left the sprite is gone. */
    void finish() override
    {
        if (!stroke_->end()) {
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
        stroke_->cancel();
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

    std::unique_ptr<MoveStroke> stroke_;
};

/* Dragging out a new selection. */
class SelectDrag : public Drag
{
  public:
    /* The anchor is the clamped press point, so it is always inside the sprite and the clipped
     * rectangle is never empty. */
    SelectDrag(Editor &editor, int cx, int cy)
        : Drag(editor, clampX(editor, cx), clampY(editor, cy)),
          hadSelection_(editor.hasSelection()), outside_(cx != anchorX_ || cy != anchorY_)
    {
        /* Remember whether a selection existed: a plain click on empty canvas selects one cel, but
         * a click that merely dismisses an old selection must not start a new one. */
        updateSelection(cx, cy);
    }

    void update(int cx, int cy) override
    {
        updateSelection(cx, cy);
        lastX_ = clampX(editor_, cx);
        lastY_ = clampY(editor_, cy);
    }

    /* A click without dragging selects the single cel under it, unless it only dismisses an
     * existing selection. */
    void finish() override
    {
        if (lastX_ == anchorX_ && lastY_ == anchorY_ && (hadSelection_ || outside_)) {
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
        editor_.setSelection(rect_clip(box, editor_.sprite().width(), editor_.sprite().height()));
    }

    bool hadSelection_;
    bool outside_; /* pressed beside the sprite: a plain click there selects nothing */
};

std::unique_ptr<Drag> beginSelect(Editor &editor, int cx, int cy)
{
    if (editor.hasSelection() && rect_contains(editor.selection(), cx, cy)) {
        /* On a locked or hidden layer the pixels cannot move: drag out a new selection instead. */
        if (Cel *cel = editor.editableCel()) {
            std::unique_ptr<MoveStroke> stroke =
                MoveStroke::begin(editor.history(), *cel, editor.selection());
            if (!stroke) {
                return nullptr;
            }
            return std::make_unique<MoveDrag>(editor, cx, cy, std::move(stroke));
        }
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
    /* A color counts as used only when the press lands on the sprite. */
    bool inside = cel->inBounds(cx, cy);
    switch (editor.tool()) {
    case ToolKind::Pencil:
    case ToolKind::Eraser: {
        PencilOptions options = {editor.options().pixel_perfect, editor.options().symmetry,
                                 editor.options().brush_size, editor.options().brush_round};
        std::optional<ToolPoint> from = editor.lineFrom();
        std::unique_ptr<PencilStroke> stroke =
            PencilStroke::begin(editor.history(), *cel, erasing ? Rgba{} : color, options,
                                from ? from->x : cx, from ? from->y : cy);
        if (!stroke) {
            return nullptr;
        }
        if (!erasing && (inside || from)) {
            editor.recordRecent(color);
        }
        if (from) {
            stroke->extend(cx, cy);
            editor.markAllDirty();
        }
        return std::make_unique<PencilDrag>(editor, cx, cy, std::move(stroke));
    }
    case ToolKind::Bucket:
        /* A single click: no drag. */
        if (inside) {
            editor.recordRecent(color);
        }
        if (bucket_fill(editor.history(), *cel, cx, cy, color, editor.options().symmetry)) {
            editor.markAllDirty();
        }
        return nullptr;
    case ToolKind::Line:
    case ToolKind::Rect:
    case ToolKind::Ellipse:
    case ToolKind::Triangle:
    case ToolKind::Diamond:
    case ToolKind::Hexagon:
    case ToolKind::Star:
    case ToolKind::Cross: {
        ShapeStyle style = {shapeKind(editor.tool()), editor.options().shape_filled, color,
                            editor.options().symmetry};
        std::unique_ptr<ShapeStroke> stroke =
            ShapeStroke::begin(editor.history(), *cel, style, ToolPoint{cx, cy});
        if (!stroke) {
            return nullptr;
        }
        editor.recordRecent(color);
        return std::make_unique<ShapeDrag>(editor, cx, cy, std::move(stroke));
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
