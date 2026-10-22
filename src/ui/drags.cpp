#include "drag.hpp"
#include "editor.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "tinysprite/tools.hpp"

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

/* A drag that edits the cel through a live stroke: finishing ends the stroke (repainting
 * everything if it changed nothing to mark more precisely), cancelling reverts it. */
template <class Stroke> class StrokeDrag : public Drag
{
  public:
    StrokeDrag(Editor &editor, int cx, int cy, std::unique_ptr<Stroke> stroke)
        : Drag(editor, cx, cy), stroke_(std::move(stroke))
    {
    }

    void finish() override
    {
        bool changed = stroke_->end(endAction());
        if (!changed) {
            editor_.markAllDirty();
        }
        onFinish(changed);
    }
    void cancel() override
    {
        stroke_->cancel();
    }

  protected:
    /* Run along with the undo / redo of the stroke's step (see History::endStroke). */
    virtual HistoryAction endAction()
    {
        return HistoryAction{};
    }

    /* After the stroke ended; `changed` is false when it left the cel as it was. */
    virtual void onFinish(bool /*changed*/)
    {
    }

    std::unique_ptr<Stroke> stroke_;
};

/* Pencil and eraser stroke. */
class PencilDrag : public StrokeDrag<PencilStroke>
{
  public:
    PencilDrag(Editor &editor, int cx, int cy, std::unique_ptr<PencilStroke> stroke)
        : StrokeDrag(editor, cx, cy, std::move(stroke))
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

  protected:
    void onFinish(bool /*unused*/) override
    {
        editor_.setLastDrawPoint(lastX_, lastY_);
    }
};

/* Line, rectangle or ellipse. */
class ShapeDrag : public StrokeDrag<ShapeStroke>
{
  public:
    ShapeDrag(Editor &editor, int cx, int cy, std::unique_ptr<ShapeStroke> stroke, ShapeKind kind)
        : StrokeDrag(editor, cx, cy, std::move(stroke)), kind_(kind), endX_(cx), endY_(cy)
    {
        markStroke(editor, cx, cy, cx, cy, 1);
    }

    void update(int cx, int cy) override
    {
        /* Shift holds the shape to even proportions; the far corner is then not the cursor. */
        ToolPoint end =
            shift_ ? shape_constrain(kind_, ToolPoint{anchorX_, anchorY_}, ToolPoint{cx, cy})
                   : ToolPoint{cx, cy};
        lastX_ = cx;
        lastY_ = cy;
        if (end.x == endX_ && end.y == endY_) {
            return;
        }
        stroke_->update(end.x, end.y);
        /* The old and new shapes both lie in a box spanned by the anchor and the far corner. */
        int pad = 1 + std::max(1, editor_.options().brush_size); /* the brush widens the outline */
        markStroke(editor_, anchorX_, anchorY_, endX_, endY_, pad);
        markStroke(editor_, anchorX_, anchorY_, end.x, end.y, pad);
        endX_ = end.x;
        endY_ = end.y;
    }

  private:
    ShapeKind kind_;
    int endX_; /* the far corner of the shape as drawn */
    int endY_;
};

/* Moving the selected pixels. */
class MoveDrag : public StrokeDrag<MoveStroke>
{
  public:
    MoveDrag(Editor &editor, int cx, int cy, std::unique_ptr<MoveStroke> stroke)
        : StrokeDrag(editor, cx, cy, std::move(stroke))
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

  protected:
    /* Undoing the move puts the selection back where it was. */
    HistoryAction endAction() override
    {
        if (editor_.moveDx() == 0 && editor_.moveDy() == 0) {
            return HistoryAction{};
        }
        Rect before = editor_.selection();
        Rect moved = before;
        moved.x += editor_.moveDx();
        moved.y += editor_.moveDy();
        return editor_.selectionAction(before, editor_.clippedSelection(moved));
    }

    /* The selection follows its pixels; the part that left the sprite is gone. */
    void onFinish(bool changed) override
    {
        if (changed) {
            Rect moved = editor_.selection();
            moved.x += editor_.moveDx();
            moved.y += editor_.moveDy();
            editor_.setSelectionClipped(moved);
        }
        editor_.setMoveOffset(0, 0);
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

/* Dragging out a new slice: the rectangle shows as a preview and becomes a slice on release. A
 * click without dragging only deselects. */
class SliceCreateDrag : public Drag
{
  public:
    SliceCreateDrag(Editor &editor, int cx, int cy)
        : Drag(editor, clampX(editor, cx), clampY(editor, cy))
    {
        editor_.selectSlice(-1);
    }

    void update(int cx, int cy) override
    {
        lastX_ = clampX(editor_, cx);
        lastY_ = clampY(editor_, cy);
        editor_.setSlicePreview(rect_from_corners(anchorX_, anchorY_, lastX_, lastY_));
    }

    void finish() override
    {
        editor_.setSlicePreview(std::nullopt);
        if (lastX_ != anchorX_ || lastY_ != anchorY_) {
            editor_.addSlice(rect_from_corners(anchorX_, anchorY_, lastX_, lastY_));
        }
    }

    void cancel() override
    {
        editor_.setSlicePreview(std::nullopt);
    }
};

/* Moving a slice: it is shown following the mouse (kept inside the sprite) and the move is one undo
 * step when the button is released. */
class SliceMoveDrag : public Drag
{
  public:
    SliceMoveDrag(Editor &editor, int cx, int cy, int index)
        : Drag(editor, cx, cy), index_(index),
          original_(editor.sprite().slices()[static_cast<size_t>(index)])
    {
        editor_.selectSlice(index);
    }

    void update(int cx, int cy) override
    {
        const Rect &r = original_.rect;
        moved_ = r;
        moved_.x = std::clamp(r.x + cx - anchorX_, 0, editor_.sprite().width() - r.w);
        moved_.y = std::clamp(r.y + cy - anchorY_, 0, editor_.sprite().height() - r.h);
        editor_.setSliceMovePreview(index_, moved_);
    }

    void finish() override
    {
        editor_.setSliceMovePreview(index_, std::nullopt);
        if (moved_.x != original_.rect.x || moved_.y != original_.rect.y) {
            Sprite::Slice moved = original_;
            moved.rect = moved_;
            editor_.updateSlice(index_, moved);
        }
    }

    void cancel() override
    {
        editor_.setSliceMovePreview(index_, std::nullopt);
    }

  private:
    int index_;
    Sprite::Slice original_;
    Rect moved_ = {0, 0, 0, 0};
};

std::unique_ptr<Drag> beginSlice(Editor &editor, Cel * /*unused*/, int cx, int cy)
{
    const int hit = editor.sliceAt(cx, cy);
    if (hit >= 0) {
        return std::make_unique<SliceMoveDrag>(editor, cx, cy, hit);
    }
    return std::make_unique<SliceCreateDrag>(editor, cx, cy);
}

std::unique_ptr<Drag> beginPick(Editor &editor, Cel * /*unused*/, int cx, int cy)
{
    return std::make_unique<PickDrag>(editor, cx, cy);
}

std::unique_ptr<Drag> beginSelect(Editor &editor, Cel * /*unused*/, int cx, int cy)
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

std::unique_ptr<Drag> beginPencil(Editor &editor, Cel *cel, int cx, int cy)
{
    bool erasing = editor.tool() == ToolKind::Eraser;
    Rgba color = editor.foreground();
    PencilOptions options = editor.options().pencil();
    std::optional<ToolPoint> from = editor.lineFrom();
    std::unique_ptr<PencilStroke> stroke =
        PencilStroke::begin(editor.history(), *cel, erasing ? Rgba{} : color, options,
                            from ? from->x : cx, from ? from->y : cy);
    if (!stroke) {
        return nullptr;
    }
    /* A color counts as used only when the press lands on the sprite. */
    if (!erasing && (cel->inBounds(cx, cy) || from)) {
        editor.recordRecent(color);
    }
    if (from) {
        stroke->extend(cx, cy);
        editor.markAllDirty();
    }
    return std::make_unique<PencilDrag>(editor, cx, cy, std::move(stroke));
}

/* A single click: no drag. */
std::unique_ptr<Drag> fillBucket(Editor &editor, Cel *cel, int cx, int cy)
{
    Rgba color = editor.foreground();
    if (cel->inBounds(cx, cy)) {
        editor.recordRecent(color);
    }
    if (bucket_fill(editor.history(), *cel, cx, cy, color, editor.options().symmetry)) {
        editor.markAllDirty();
    }
    return nullptr;
}

std::unique_ptr<Drag> beginShape(Editor &editor, Cel *cel, int cx, int cy)
{
    Rgba color = editor.foreground();
    ShapeStyle style = {tool_traits(editor.tool()).shape, editor.options().shape_filled, color,
                        editor.options().symmetry};
    style.brush_size = editor.options().brush_size;
    style.brush_round = editor.options().brush_round;
    std::unique_ptr<ShapeStroke> stroke =
        ShapeStroke::begin(editor.history(), *cel, style, ToolPoint{cx, cy});
    if (!stroke) {
        return nullptr;
    }
    editor.recordRecent(color);
    return std::make_unique<ShapeDrag>(editor, cx, cy, std::move(stroke), style.kind);
}

/* Starts the drag for a press; `cel` is the cel to edit, null for the tools that do not draw. */
using DragFactory = std::unique_ptr<Drag> (*)(Editor &editor, Cel *cel, int cx, int cy);

/* In ToolGroup order. */
const std::array<DragFactory, static_cast<size_t>(ToolGroup::Count)> FACTORIES = {
    beginPencil, fillBucket, beginPick, beginShape, beginSelect, beginSlice,
};

} // namespace

std::unique_ptr<Drag> beginDrag(Editor &editor, int cx, int cy)
{
    ToolKind tool = editor.tool();
    Cel *cel = nullptr;
    if (tool_draws(tool)) {
        cel = editor.editableCel();
        if (!cel) {
            return nullptr;
        }
    }
    return FACTORIES[static_cast<size_t>(tool_traits(tool).group)](editor, cel, cx, cy);
}
