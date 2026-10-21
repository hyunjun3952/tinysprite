/* Editor: colors and the palette. */

#include "editor.h"

#include <algorithm>
#include <utility>

#include "editor_internal.h"

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
    Palette &palette = sprite().palette();
    Rgba current = foreground();
    int index = palette.find(current);
    if (index < 0) {
        index = palette.add(current);
        modifiedOutsideHistory_ = modifiedOutsideHistory_ || index >= 0;
    }
    if (index >= 0) {
        paletteSelected_ = index;
    }
    return index;
}

void Editor::paletteReset()
{
    setPalette(editor_detail::defaultPalette());
}

void Editor::setPalette(Palette palette)
{
    sprite().palette() = std::move(palette);
    paletteSelected_ = -1;
    modifiedOutsideHistory_ = true;
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
    if (!sprite().palette().remove(index)) {
        return;
    }
    modifiedOutsideHistory_ = true;
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
