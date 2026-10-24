/* Editor: colors and the palette. */

#include "editor.hpp"

#include <algorithm>
#include <utility>

#include "editor/detail.hpp"
#include "tinysprite/palette_ops.hpp"

Rgba Editor::foreground() const
{
    return Rgba{editor_detail::toByte(color_[0]), editor_detail::toByte(color_[1]),
                editor_detail::toByte(color_[2]), editor_detail::toByte(color_[3])};
}

void Editor::setForeground(Rgba rgba)
{
    editor_detail::setFloats(color_, rgba);
}

int Editor::paletteAdd()
{
    const Palette &palette = sprite().palette();
    Rgba current = foreground();
    int index = palette.find(current);
    if (index < 0) {
        Palette next = palette;
        index = next.add(current);
        if (index >= 0 && !palette_op_set(history(), sprite(), std::move(next))) {
            index = -1;
        }
    }
    if (index >= 0) {
        paletteSelected_ = index;
    }
    return index;
}

bool Editor::paletteCanAdd() const
{
    const Palette &palette = sprite().palette();
    return palette.find(foreground()) < 0 && palette.count() < PALETTE_MAX_COLORS;
}

bool Editor::paletteCanReplace() const
{
    return paletteSelected_ >= 0 && paletteSelected_ < sprite().palette().count() &&
           sprite().palette().find(foreground()) < 0;
}

bool Editor::paletteReplaceSelected()
{
    if (!paletteCanReplace()) {
        return false;
    }
    Palette next = sprite().palette();
    return next.set(paletteSelected_, foreground()) &&
           palette_op_set(history(), sprite(), std::move(next));
}

bool Editor::paletteIsDefault() const
{
    return sprite().palette() == editor_detail::defaultPalette();
}

void Editor::paletteReset()
{
    setPalette(editor_detail::defaultPalette());
}

void Editor::setPalette(Palette palette)
{
    if (palette_op_set(history(), sprite(), std::move(palette))) {
        paletteSelected_ = -1;
    }
}

void Editor::palettePick(int index)
{
    const Palette &palette = sprite().palette();
    if (index < 0 || index >= palette.count()) {
        return;
    }
    paletteSelected_ = index;
    setForeground(palette.get(index));
}

void Editor::paletteRemove(int index)
{
    Palette next = sprite().palette();
    if (!next.remove(index) || !palette_op_set(history(), sprite(), std::move(next))) {
        return;
    }
    if (paletteSelected_ == index) {
        paletteSelected_ = -1;
    } else if (paletteSelected_ > index) {
        paletteSelected_--;
    }
}

void Editor::swapColors()
{
    for (int i = 0; i < 4; i++) {
        std::swap(color_[i], bgColor_[i]);
    }
}
