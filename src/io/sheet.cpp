#include <algorithm>
#include <cmath>

#include "image_write.h"
#include "tinysprite/composite.h"
#include "tinysprite/io.h"

std::optional<SpriteSheet> spritesheet_render(const Sprite &sprite, const SheetOptions &options)
{
    int last = options.last < 0 ? sprite.frameCount() - 1 : options.last;
    if (options.first < 0 || last >= sprite.frameCount() || options.first > last ||
        options.scale < 1 || options.scale > IMAGE_EXPORT_MAX_SCALE || options.spacing < 0 ||
        options.columns < 0) {
        return std::nullopt;
    }
    const int count = last - options.first + 1;
    const int cellW = sprite.width() * options.scale;
    const int cellH = sprite.height() * options.scale;
    int columns = 1;
    int rows = 1;
    switch (options.layout) {
    case SheetLayout::Horizontal:
        columns = count;
        break;
    case SheetLayout::Vertical:
        rows = count;
        break;
    case SheetLayout::Grid:
        columns = options.columns > 0
                      ? std::min(options.columns, count)
                      : static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
        break;
    }
    if (options.layout != SheetLayout::Vertical) {
        rows = (count + columns - 1) / columns;
    }
    /* long long: a long strip of big frames must not overflow before the size check */
    long long width = static_cast<long long>(columns) * cellW +
                      static_cast<long long>(columns - 1) * options.spacing;
    long long height =
        static_cast<long long>(rows) * cellH + static_cast<long long>(rows - 1) * options.spacing;
    if (width > IMAGE_EXPORT_MAX_SIZE || height > IMAGE_EXPORT_MAX_SIZE) {
        return std::nullopt;
    }
    SpriteSheet sheet;
    sheet.width = static_cast<int>(width);
    sheet.height = static_cast<int>(height);
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height),
                        Rgba{});
    std::vector<Rgba> frame(static_cast<size_t>(sprite.width()) *
                            static_cast<size_t>(sprite.height()));
    for (int i = 0; i < count; i++) {
        int x0 = (i % columns) * (cellW + options.spacing);
        int y0 = (i / columns) * (cellH + options.spacing);
        sheet.frames.push_back(Rect{x0, y0, cellW, cellH});
        composite_sprite(sprite, options.first + i, Rect{0, 0, sprite.width(), sprite.height()},
                         frame.data());
        for (int y = 0; y < cellH; y++) {
            for (int x = 0; x < cellW; x++) {
                sheet.pixels[static_cast<size_t>(y0 + y) * static_cast<size_t>(sheet.width) +
                             static_cast<size_t>(x0 + x)] =
                    frame[static_cast<size_t>(y / options.scale) *
                              static_cast<size_t>(sprite.width()) +
                          static_cast<size_t>(x / options.scale)];
            }
        }
    }
    return sheet;
}

bool spritesheet_export(const Sprite &sprite, const std::string &path, const SheetOptions &options)
{
    std::optional<SpriteSheet> sheet = spritesheet_render(sprite, options);
    return sheet && image_write_rgba(path, sheet->width, sheet->height, sheet->pixels);
}
