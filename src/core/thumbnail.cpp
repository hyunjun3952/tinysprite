#include "tinysprite/thumbnail.h"

#include <algorithm>
#include <cmath>

void thumbnail_size(int width, int height, int maxWidth, int maxHeight, int &outWidth,
                    int &outHeight)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    float scale = std::min(static_cast<float>(maxWidth) / static_cast<float>(width),
                           static_cast<float>(maxHeight) / static_cast<float>(height));
    if (scale >= 1.0f) {
        int factor = static_cast<int>(std::floor(scale));
        outWidth = width * factor;
        outHeight = height * factor;
        return;
    }
    outWidth = std::clamp(static_cast<int>(std::lround(static_cast<float>(width) * scale)), 1,
                          std::max(maxWidth, 1));
    outHeight = std::clamp(static_cast<int>(std::lround(static_cast<float>(height) * scale)), 1,
                           std::max(maxHeight, 1));
}

std::vector<Rgba> thumbnail_pixels(const Cel &cel, int thumbWidth, int thumbHeight)
{
    thumbWidth = std::max(thumbWidth, 1);
    thumbHeight = std::max(thumbHeight, 1);
    std::vector<Rgba> pixels(static_cast<size_t>(thumbWidth) * static_cast<size_t>(thumbHeight));
    for (int y = 0; y < thumbHeight; y++) {
        int sy = (2 * y + 1) * cel.height() / (2 * thumbHeight);
        for (int x = 0; x < thumbWidth; x++) {
            int sx = (2 * x + 1) * cel.width() / (2 * thumbWidth);
            pixels[static_cast<size_t>(y) * static_cast<size_t>(thumbWidth) +
                   static_cast<size_t>(x)] = cel.get(sx, sy);
        }
    }
    return pixels;
}
