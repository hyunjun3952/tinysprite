#include "editor/internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

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

std::optional<Sprite> blankSprite(int width, int height)
{
    try {
        Sprite sprite(width, height);
        sprite.palette() = defaultPalette();
        return sprite;
    } catch (const std::invalid_argument &) {
        return std::nullopt;
    }
}

int indexOfLayer(const Sprite &sprite, const Layer *layer)
{
    for (int i = 0; i < sprite.layerCount(); i++) {
        if (sprite.layer(i) == layer) {
            return i;
        }
    }
    return -1;
}

} // namespace editor_detail
