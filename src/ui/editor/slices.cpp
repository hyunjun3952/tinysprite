/* Editor: the slices (named rectangles for the game). */

#include <algorithm>
#include <string>
#include <utility>

#include "editor.hpp"

int Editor::selectedSlice() const
{
    return selectedSlice_ >= 0 && selectedSlice_ < static_cast<int>(sprite_.slices().size())
               ? selectedSlice_
               : -1;
}

void Editor::selectSlice(int index)
{
    selectedSlice_ = index >= 0 && index < static_cast<int>(sprite_.slices().size()) ? index : -1;
}

namespace
{

/* "Slice N" for the smallest N that no slice has. */
std::string freshName(const Sprite &sprite)
{
    for (int n = 1;; n++) {
        const std::string name = "Slice " + std::to_string(n);
        const bool taken = std::any_of(sprite.slices().begin(), sprite.slices().end(),
                                       [&name](const Sprite::Slice &s) { return s.name == name; });
        if (!taken) {
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
    if (!slice_op_add(history_, sprite_, std::move(slice))) {
        return false;
    }
    selectedSlice_ = static_cast<int>(sprite_.slices().size()) - 1;
    return true;
}

bool Editor::updateSlice(int index, Sprite::Slice slice)
{
    return !sliceEditsBlocked() && slice_op_set(history_, sprite_, index, std::move(slice));
}

bool Editor::removeSlice(int index)
{
    if (sliceEditsBlocked() || !slice_op_remove(history_, sprite_, index)) {
        return false;
    }
    if (index < selectedSlice_) {
        selectedSlice_--;
    } else if (index == selectedSlice_) {
        selectedSlice_ = -1;
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

bool Editor::previewSlice(int index, Sprite::Slice slice)
{
    return sprite_.setSlice(index, std::move(slice));
}
