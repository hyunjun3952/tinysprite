/* Editor: colors and the palette. */

#include "editor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "editor_internal.h"

namespace editor_detail
{

uint8_t toByte(float value)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
}

void setFloats(float out[4], Rgba color)
{
    out[0] = static_cast<float>(color.r) / 255.0f;
    out[1] = static_cast<float>(color.g) / 255.0f;
    out[2] = static_cast<float>(color.b) / 255.0f;
    out[3] = static_cast<float>(color.a) / 255.0f;
}

namespace
{

const Rgba DEFAULT_PALETTE[] = {
    {0, 0, 0, 255},      {255, 255, 255, 255}, {127, 127, 127, 255}, {195, 195, 195, 255},
    {136, 0, 21, 255},   {237, 28, 36, 255},   {255, 127, 39, 255},  {255, 242, 0, 255},
    {34, 177, 76, 255},  {0, 162, 232, 255},   {63, 72, 204, 255},   {163, 73, 164, 255},
    {185, 122, 87, 255}, {255, 174, 201, 255}, {255, 201, 14, 255},  {181, 230, 29, 255},
};

} // namespace

Palette defaultPalette()
{
    Palette palette;
    for (const Rgba &color : DEFAULT_PALETTE) {
        palette.add(color);
    }
    return palette;
}

} // namespace editor_detail

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
