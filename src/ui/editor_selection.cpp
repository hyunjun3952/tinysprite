/* Editor: the pixel selection and the clipboard. */

#include "editor.h"

#include <optional>
#include <utility>

void Editor::selectAll()
{
    if (stroking()) {
        return;
    }
    selection_ = spriteBounds();
}

void Editor::clearSelection()
{
    if (!stroking()) {
        selection_.reset();
    }
}

void Editor::deleteSelection()
{
    Cel *cel = editableCel();
    if (!cel || !selection_) {
        return;
    }
    Rect r = *selection_;
    ShapeStyle erase{ShapeKind::Rect, true, Rgba{}, Symmetry{}};
    if (shape_stroke(history(), *cel, erase, ToolPoint{r.x, r.y},
                     ToolPoint{r.x + r.w - 1, r.y + r.h - 1})) {
        markDirty(r);
    }
}

bool Editor::copySelection()
{
    Cel *cel = activeCel();
    if (stroking() || !selection_ || !cel) {
        return false;
    }
    std::optional<Clip> clip = Clip::copy(*cel, *selection_);
    if (!clip) {
        return false;
    }
    clipboard_ = std::move(clip);
    return true;
}

bool Editor::cutSelection()
{
    if (!editableCel() || !copySelection()) {
        return false;
    }
    deleteSelection();
    return true;
}

void Editor::paste()
{
    Cel *cel = editableCel();
    if (!cel || !clipboard_) {
        return;
    }
    const Clip &clip = *clipboard_;
    int x = 0;
    int y = 0;
    if (hoverInSprite()) {
        x = hoverX_;
        y = hoverY_;
    } else if (selection_) {
        x = selection_->x;
        y = selection_->y;
    }
    if (!clip_paste(history(), *cel, clip, x, y)) {
        return;
    }
    Rect pasted = {x, y, clip.width(), clip.height()};
    markDirty(pasted);
    selectTool(ToolKind::Select);
    setSelectionClipped(pasted);
}

void Editor::setSelectionClipped(Rect r)
{
    Rect clipped = rect_clip(r, sprite().width(), sprite().height());
    selection_ = rect_is_empty(clipped) ? std::nullopt : std::optional<Rect>(clipped);
}

std::optional<Editor::SelectionEdit> Editor::beginSelectionEdit()
{
    Cel *cel = editableCel();
    if (!cel || !selection_) {
        return std::nullopt;
    }
    std::optional<Clip> clip = Clip::copy(*cel, *selection_);
    if (!clip) {
        return std::nullopt;
    }
    return SelectionEdit{*cel, *selection_, std::move(*clip)};
}

void Editor::flipSelection(bool horizontal)
{
    std::optional<SelectionEdit> edit = beginSelectionEdit();
    if (!edit) {
        return;
    }
    Rect r = edit->rect;
    if (horizontal) {
        edit->clip.flipHorizontal();
    } else {
        edit->clip.flipVertical();
    }
    if (clip_replace(history(), edit->cel, r, edit->clip, r.x, r.y)) {
        markDirty(r);
    }
}

void Editor::rotateSelection(bool clockwise)
{
    std::optional<SelectionEdit> edit = beginSelectionEdit();
    if (!edit) {
        return;
    }
    Rect r = edit->rect;
    edit->clip.rotate(clockwise);
    /* The turned rectangle shares the old one's center. */
    Rect turned = {r.x + (r.w - r.h) / 2, r.y + (r.h - r.w) / 2, r.h, r.w};
    if (clip_replace(history(), edit->cel, r, edit->clip, turned.x, turned.y)) {
        markDirty(r);
        markDirty(turned);
        setSelectionClipped(turned);
    }
}
