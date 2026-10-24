/* Editor: the pixel selection and the clipboard. */

#include "editor.hpp"

#include <memory>
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
    Rect pasted = {x, y, clip.width(), clip.height()};
    bool done = false;
    guarded([&] {
        done = clip_paste(history(), *cel, clip, x, y,
                          selectionAction(selection_, clippedSelection(pasted)));
    });
    if (!done) {
        return;
    }
    markDirty(pasted);
    selectTool(ToolKind::Select);
    setSelectionClipped(pasted);
}

void Editor::setSelectionClipped(Rect r)
{
    selection_ = clippedSelection(r);
}

std::optional<Rect> Editor::clippedSelection(Rect r) const
{
    Rect clipped = rect_clip(r, sprite().width(), sprite().height());
    return rect_is_empty(clipped) ? std::nullopt : std::optional<Rect>(clipped);
}

HistoryAction Editor::selectionAction(std::optional<Rect> before, std::optional<Rect> after)
{
    return HistoryAction{[this, before] { selection_ = before; },
                         [this, after] { selection_ = after; }};
}

namespace
{

/* Whether two clips hold the same pixels; an edit that leaves them so changes nothing. */
bool sameClip(const Clip &a, const Clip &b)
{
    if (a.width() != b.width() || a.height() != b.height()) {
        return false;
    }
    for (int y = 0; y < a.height(); y++) {
        for (int x = 0; x < a.width(); x++) {
            if (a.get(x, y) != b.get(x, y)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

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
    const Clip original = edit->clip;
    if (horizontal) {
        edit->clip.flipHorizontal();
    } else {
        edit->clip.flipVertical();
    }
    if (sameClip(original, edit->clip)) {
        return; /* nothing moved: no step, the document stays as saved */
    }
    if (clip_replace(history(), edit->cel, r, edit->clip, r.x, r.y, selectionAction(r, r))) {
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
    const Clip original = edit->clip;
    edit->clip.rotate(clockwise);
    /* The turned rectangle shares the old one's center. */
    Rect turned = {r.x + (r.w - r.h) / 2, r.y + (r.h - r.w) / 2, r.h, r.w};
    if (r.w == r.h && sameClip(original, edit->clip)) {
        return; /* a square of identical pixels turns into itself */
    }
    if (clip_replace(history(), edit->cel, r, edit->clip, turned.x, turned.y,
                     selectionAction(r, clippedSelection(turned)))) {
        markDirty(r);
        markDirty(turned);
        setSelectionClipped(turned);
    }
}

bool Editor::nudgeSelection(int dx, int dy)
{
    Cel *cel = editableCel();
    if (!cel || (dx == 0 && dy == 0)) {
        return false;
    }
    /* Without a selection the whole cel moves and there is still none afterwards. */
    std::optional<Rect> had = selection_;
    Rect before = had.value_or(spriteBounds());
    Rect moved = {before.x + dx, before.y + dy, before.w, before.h};
    std::optional<Rect> after = had ? clippedSelection(moved) : std::nullopt;
    std::unique_ptr<MoveStroke> stroke = MoveStroke::begin(history(), *cel, before);
    if (!stroke) {
        return false;
    }
    stroke->update(dx, dy);
    /* With no selection (or one that stays put) the stroke is only worth a step if pixels moved. */
    const bool selectionMoves =
        had.has_value() && after.has_value()
            ? (had->x != after->x || had->y != after->y || had->w != after->w || had->h != after->h)
            : had.has_value() != after.has_value();
    if (!stroke->end(selectionMoves ? selectionAction(had, after) : HistoryAction{})) {
        return false;
    }
    markDirty(before);
    markDirty(moved);
    selection_ = after;
    return true;
}
