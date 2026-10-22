/* Editor: the slices (named rectangles for the game). */

#include <algorithm>
#include <string>
#include <utility>

#include "editor.hpp"

int Editor::selectedSlice() const
{
    const std::vector<Sprite::Slice> &slices = sprite_.slices();
    for (int i = 0; i < static_cast<int>(slices.size()); i++) {
        if (!selectedSliceName_.empty() &&
            slices[static_cast<size_t>(i)].name == selectedSliceName_) {
            return i;
        }
    }
    return -1;
}

void Editor::selectSlice(int index)
{
    selectedSliceName_ = index >= 0 && index < static_cast<int>(sprite_.slices().size())
                             ? sprite_.slices()[static_cast<size_t>(index)].name
                             : std::string();
}

namespace
{

bool nameTaken(const Sprite &sprite, const std::string &name, int except)
{
    for (int i = 0; i < static_cast<int>(sprite.slices().size()); i++) {
        if (i != except && sprite.slices()[static_cast<size_t>(i)].name == name) {
            return true;
        }
    }
    return false;
}

/* `wanted`, or "wanted 2", "wanted 3", ... if another slice (not `except`) already has the name. */
std::string uniqueName(const Sprite &sprite, const std::string &wanted, int except)
{
    if (!nameTaken(sprite, wanted, except)) {
        return wanted;
    }
    for (int n = 2;; n++) {
        const std::string name = wanted + " " + std::to_string(n);
        if (!nameTaken(sprite, name, except)) {
            return name;
        }
    }
}

/* "Slice N" for the smallest N that no slice has. */
std::string freshName(const Sprite &sprite)
{
    for (int n = 1;; n++) {
        const std::string name = "Slice " + std::to_string(n);
        if (!nameTaken(sprite, name, -1)) {
            return name;
        }
    }
}

} // namespace

bool Editor::addSlice(Rect rect)
{
    rect = rect_clip(rect, sprite_.width(), sprite_.height());
    if (sliceEditsBlocked() || rect_is_empty(rect)) {
        return false;
    }
    Sprite::Slice slice;
    slice.name = freshName(sprite_);
    slice.rect = rect;
    const std::string name = slice.name;
    if (!slice_op_add(history_, sprite_, std::move(slice))) {
        return false;
    }
    selectedSliceName_ = name;
    return true;
}

bool Editor::updateSlice(int index, Sprite::Slice slice)
{
    if (sliceEditsBlocked() || index < 0 || index >= static_cast<int>(sprite_.slices().size())) {
        return false;
    }
    slice.name = uniqueName(sprite_, layer_name_truncate(slice.name, SLICE_NAME_MAX), index);
    const bool wasSelected = index == selectedSlice();
    const std::string name = slice.name;
    if (!slice_op_set(history_, sprite_, index, std::move(slice))) {
        return false;
    }
    if (wasSelected) {
        selectedSliceName_ = name; /* a renamed slice stays selected */
    }
    return true;
}

bool Editor::removeSlice(int index)
{
    if (sliceEditsBlocked() || !slice_op_remove(history_, sprite_, index)) {
        return false;
    }
    if (index >= 0 && selectedSlice() < 0) {
        selectedSliceName_.clear(); /* the selected one was the one removed */
    }
    return true;
}

int Editor::sliceAt(int cx, int cy) const
{
    for (int i = static_cast<int>(sprite_.slices().size()) - 1; i >= 0; i--) {
        if (rect_contains(sprite_.slices()[static_cast<size_t>(i)].rect, cx, cy)) {
            return i;
        }
    }
    return -1;
}

void Editor::setSliceMovePreview(int index, std::optional<Rect> rect)
{
    slicePreviewIndex_ = rect ? index : -1;
    slicePreviewMove_ = rect;
}
