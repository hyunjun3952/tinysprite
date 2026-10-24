#include <algorithm>
#include <cmath>

#include "frame_range.hpp"
#include "tinysprite/composite.hpp"
#include "tinysprite/io.hpp"
#include "tinysprite/pack.hpp"
#include "write_error.hpp"

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
    case SheetLayout::Packed: /* laid out by pack_rects, not on a grid */
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

/* The smallest rectangle holding every pixel that is not fully transparent; a frame with none gives
 * a 1 x 1 rectangle at the origin. */
Rect opaqueBounds(const std::vector<Rgba> &pixels, int width, int height)
{
    int x0 = width;
    int y0 = height;
    int x1 = -1;
    int y1 = -1;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]
                    .a > 0) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    }
    return x1 < 0 ? Rect{0, 0, 1, 1} : Rect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

/* The pixels of `area` of a frame that is `width` wide. */
std::vector<Rgba> cutOut(const std::vector<Rgba> &pixels, int width, Rect area)
{
    std::vector<Rgba> part(static_cast<size_t>(area.w) * static_cast<size_t>(area.h));
    for (int y = 0; y < area.h; y++) {
        for (int x = 0; x < area.w; x++) {
            part[static_cast<size_t>(y) * static_cast<size_t>(area.w) + static_cast<size_t>(x)] =
                pixels[static_cast<size_t>(area.y + y) * static_cast<size_t>(width) +
                       static_cast<size_t>(area.x + x)];
        }
    }
    return part;
}

/* The sheet of a Packed layout: every frame trimmed, the pieces packed. Nothing when they do not
 * fit on a sheet of IMAGE_EXPORT_MAX_SIZE. */
std::optional<SpriteSheet> renderPacked(const Sprite &sprite, const SheetOptions &options,
                                        int count)
{
    std::vector<std::vector<Rgba>> frames;
    std::vector<Rect> kept;  /* the trimmed part of each frame, in cels */
    std::vector<Rect> sizes; /* those pieces as drawn on the sheet, scaled */
    const Rect whole{0, 0, sprite.width(), sprite.height()};
    for (int i = 0; i < count; i++) {
        std::vector<Rgba> frame(static_cast<size_t>(sprite.width()) *
                                static_cast<size_t>(sprite.height()));
        composite_sprite(sprite, options.first + i, whole, frame.data());
        kept.push_back(opaqueBounds(frame, sprite.width(), sprite.height()));
        sizes.push_back(Rect{0, 0, kept.back().w * options.scale, kept.back().h * options.scale});
        frames.push_back(std::move(frame));
    }
    const std::optional<PackedRects> placed =
        pack_rects(sizes, options.spacing, IMAGE_EXPORT_MAX_SIZE);
    if (!placed) {
        return std::nullopt;
    }
    const PackedRects &packed = *placed;
    SpriteSheet sheet;
    sheet.width = packed.width;
    sheet.height = packed.height;
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height),
                        Rgba{});
    sheet.trimmed = true;
    for (int i = 0; i < count; i++) {
        const auto at = static_cast<size_t>(i);
        sheet.frames.push_back(packed.rects[at]);
        sheet.trims.push_back(
            Rect{kept[at].x * options.scale, kept[at].y * options.scale, sizes[at].w, sizes[at].h});
        blitScaled(sheet, cutOut(frames[at], sprite.width(), kept[at]), kept[at].w,
                   packed.rects[at], options.scale);
    }
    return sheet;
}

} // namespace

std::optional<SpriteSheet> spritesheet_render(const Sprite &sprite, const SheetOptions &options)
{
    int last = frame_range_end(sprite, options.first, options.last);
    if (last < 0 || options.scale < 1 || options.scale > IMAGE_EXPORT_MAX_SCALE ||
        options.spacing < 0 || options.spacing > IMAGE_EXPORT_MAX_SIZE || options.columns < 0) {
        return std::nullopt;
    }
    const int count = last - options.first + 1;
    if (options.layout == SheetLayout::Packed) {
        return renderPacked(sprite, options, count);
    }
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
    file_write_error_clear();
    std::optional<SpriteSheet> sheet = spritesheet_render(sprite, options);
    return sheet && image_write_rgba(path, sheet->width, sheet->height, sheet->pixels);
}
