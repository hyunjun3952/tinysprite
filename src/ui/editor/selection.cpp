/* Editor: the pixel selection (a rectangle, or a shape of any kind) and the clipboard. */

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
    mask_.reset();
}

void Editor::clearSelection()
{
    if (!stroking()) {
        dropSelection();
    }
}

void Editor::deleteSelection()
{
    Cel *cel = editableCel();
    if (!cel || !selection_) {
        return;
    }
    Rect r = *selection_;
    if (mask_) {
        if (clear_mask(history(), *cel, *mask_)) {
            markDirty(r);
        }
        return;
    }
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
    std::optional<Clip> clip = mask_ ? Clip::copy(*cel, *mask_) : Clip::copy(*cel, *selection_);
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
        done = clip_paste(
            history(), *cel, clip, x, y,
            selectionAction(selectionState(), SelectionState{clippedSelection(pasted), nullptr}));
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
    mask_.reset();
}

std::optional<Rect> Editor::clippedSelection(Rect r) const
{
    Rect clipped = rect_clip(r, sprite().width(), sprite().height());
    return rect_is_empty(clipped) ? std::nullopt : std::optional<Rect>(clipped);
}

void Editor::setSelectionState(const SelectionState &state)
{
    selection_ = state.rect;
    mask_ = state.rect ? state.mask : nullptr;
}

void Editor::setSelectionMask(Mask mask)
{
    if (mask.empty()) {
        dropSelection();
        return;
    }
    selection_ = mask.bounds();
    mask_ = std::make_shared<const Mask>(std::move(mask));
}

Mask Editor::selectionShape() const
{
    if (mask_) {
        return *mask_;
    }
    return selection_ ? Mask::full(*selection_) : Mask();
}

void Editor::applyPick(Mask picked, SelectMode mode)
{
    if (picked.empty()) {
        if (mode == SelectMode::Replace) {
            dropSelection();
        }
        return;
    }
    if (mode == SelectMode::Subtract) {
        if (selection_) {
            setSelectionMask(selectionShape().subtracted(picked));
        }
        return;
    }
    if (mode == SelectMode::Add && selection_) {
        picked = selectionShape().united(picked);
    }
    setSelectionMask(std::move(picked));
}

void Editor::selectRegion(int x, int y, SelectMode mode)
{
    const Cel *cel = activeCel();
    if (!cel) {
        return;
    }
    applyPick(mask_of_region(*cel, x, y, options_.fill()), mode);
}

void Editor::selectPolygon(const std::vector<ToolPoint> &points, SelectMode mode)
{
    std::vector<std::pair<int, int>> corners;
    corners.reserve(points.size());
    for (const ToolPoint &point : points) {
        corners.emplace_back(point.x, point.y);
    }
    applyPick(mask_of_polygon(sprite().width(), sprite().height(), corners), mode);
}

Editor::SelectionState Editor::selectionMovedBy(int dx, int dy) const
{
    if (!selection_) {
        return {};
    }
    if (mask_) {
        Mask moved = mask_->translated(dx, dy).clipped(spriteBounds());
        if (moved.empty()) {
            return {};
        }
        Rect bounds = moved.bounds();
        return {bounds, std::make_shared<const Mask>(std::move(moved))};
    }
    Rect moved = {selection_->x + dx, selection_->y + dy, selection_->w, selection_->h};
    return {clippedSelection(moved), nullptr};
}

HistoryAction Editor::selectionAction(std::optional<Rect> before, std::optional<Rect> after)
{
    return selectionAction(SelectionState{before, nullptr}, SelectionState{after, nullptr});
}

HistoryAction Editor::selectionAction(const SelectionState &before, const SelectionState &after)
{
    return HistoryAction{[this, before] { setSelectionState(before); },
                         [this, after] { setSelectionState(after); }};
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
    std::optional<Clip> clip = mask_ ? Clip::copy(*cel, *mask_) : Clip::copy(*cel, *selection_);
    if (!clip) {
        return std::nullopt;
    }
    return SelectionEdit{*cel, *selection_, std::move(*clip), mask_};
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
    if (edit->mask) {
        const Mask flipped = edit->mask->flipped(horizontal);
        if (sameClip(original, edit->clip) && flipped == *edit->mask) {
            return; /* nothing moved: no step, the document stays as saved */
        }
        const SelectionState after{flipped.bounds(), std::make_shared<const Mask>(flipped)};
        if (clip_replace(history(), edit->cel, *edit->mask, edit->clip, r.x, r.y,
                         selectionAction(selectionState(), after))) {
            markDirty(r);
            setSelectionState(after);
        }
        return;
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
    if (edit->mask) {
        const Mask rotated = edit->mask->rotated(clockwise).clipped(spriteBounds());
        if (r.w == r.h && sameClip(original, edit->clip) && rotated == *edit->mask) {
            return; /* a square of identical pixels turns into itself */
        }
        const SelectionState after =
            rotated.empty()
                ? SelectionState{}
                : SelectionState{rotated.bounds(), std::make_shared<const Mask>(rotated)};
        if (clip_replace(history(), edit->cel, *edit->mask, edit->clip, turned.x, turned.y,
                         selectionAction(selectionState(), after))) {
            markDirty(r);
            markDirty(turned);
            setSelectionState(after);
        }
        return;
    }
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
    const SelectionState had = selectionState();
    Rect before = had.rect.value_or(spriteBounds());
    Rect moved = {before.x + dx, before.y + dy, before.w, before.h};
    const SelectionState after = had.rect ? selectionMovedBy(dx, dy) : SelectionState{};
    std::unique_ptr<MoveStroke> stroke = mask_ ? MoveStroke::begin(history(), *cel, *mask_)
                                               : MoveStroke::begin(history(), *cel, before);
    if (!stroke) {
        return false;
    }
    stroke->update(dx, dy);
    /* With no selection (or one that stays put) the stroke is only worth a step if pixels moved. */
    bool selectionMoves = had.rect.has_value() != after.rect.has_value();
    if (had.rect && after.rect) {
        selectionMoves = had.rect->x != after.rect->x || had.rect->y != after.rect->y ||
                         had.rect->w != after.rect->w || had.rect->h != after.rect->h ||
                         (had.mask != nullptr);
    }
    if (!stroke->end(selectionMoves ? selectionAction(had, after) : HistoryAction{})) {
        return false;
    }
    markDirty(before);
    markDirty(moved);
    setSelectionState(after);
    return true;
}
