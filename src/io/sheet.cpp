#include <algorithm>
#include <cmath>

#include "frame_range.h"
#include "image_write.h"
#include "tinysprite/composite.h"
#include "tinysprite/io.h"

namespace
{

struct Grid {
    int columns;
    int rows;
};

/* How many columns and rows lay `count` frames out in the given layout. */
Grid sheetGrid(const SheetOptions &options, int count)
{
    switch (options.layout) {
    case SheetLayout::Horizontal:
        return {count, 1};
    case SheetLayout::Vertical:
        return {1, count};
    case SheetLayout::Grid:
        break;
    }
    int columns = options.columns > 0
                      ? std::min(options.columns, count)
                      : static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    return {columns, (count + columns - 1) / columns};
}

/* Copies the `source` pixels into the sheet at `cell`, every pixel enlarged `scale` times. */
void blitScaled(SpriteSheet &sheet, const std::vector<Rgba> &source, int sourceWidth, Rect cell,
                int scale)
{
    for (int y = 0; y < cell.h; y++) {
        for (int x = 0; x < cell.w; x++) {
            sheet.pixels[static_cast<size_t>(cell.y + y) * static_cast<size_t>(sheet.width) +
                         static_cast<size_t>(cell.x + x)] =
                source[static_cast<size_t>(y / scale) * static_cast<size_t>(sourceWidth) +
                       static_cast<size_t>(x / scale)];
        }
    }
}

} // namespace

std::optional<SpriteSheet> spritesheet_render(const Sprite &sprite, const SheetOptions &options)
{
    int last = frame_range_end(sprite, options.first, options.last);
    if (last < 0 || options.scale < 1 || options.scale > IMAGE_EXPORT_MAX_SCALE ||
        options.spacing < 0 || options.columns < 0) {
        return std::nullopt;
    }
    const int count = last - options.first + 1;
    const int cellW = sprite.width() * options.scale;
    const int cellH = sprite.height() * options.scale;
    const Grid grid = sheetGrid(options, count);
    /* long long: a long strip of big frames must not overflow before the size check */
    long long width = static_cast<long long>(grid.columns) * cellW +
                      static_cast<long long>(grid.columns - 1) * options.spacing;
    long long height = static_cast<long long>(grid.rows) * cellH +
                       static_cast<long long>(grid.rows - 1) * options.spacing;
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
        Rect cell{(i % grid.columns) * (cellW + options.spacing),
                  (i / grid.columns) * (cellH + options.spacing), cellW, cellH};
        sheet.frames.push_back(cell);
        composite_sprite(sprite, options.first + i, Rect{0, 0, sprite.width(), sprite.height()},
                         frame.data());
        blitScaled(sheet, frame, sprite.width(), cell, options.scale);
    }
    return sheet;
}

bool spritesheet_export(const Sprite &sprite, const std::string &path, const SheetOptions &options)
{
    std::optional<SpriteSheet> sheet = spritesheet_render(sprite, options);
    return sheet && image_write_rgba(path, sheet->width, sheet->height, sheet->pixels);
}
