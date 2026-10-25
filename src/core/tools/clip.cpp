#include <utility>

#include "tinysprite/selection.hpp"

#include <algorithm>

std::optional<Clip> Clip::copy(const Cel &cel, Rect area)
{
    Rect r = rect_clip(area, cel.width(), cel.height());
    if (rect_is_empty(r)) {
        return std::nullopt;
    }
    Clip clip(r.w, r.h);
    clip.pixels_.reserve(static_cast<size_t>(r.w) * static_cast<size_t>(r.h));
    for (int j = 0; j < r.h; j++) {
        for (int i = 0; i < r.w; i++) {
            clip.pixels_.push_back(cel.get(r.x + i, r.y + j));
        }
    }
    return clip;
}

std::optional<Clip> Clip::copy(const Cel &cel, const Mask &mask)
{
    std::optional<Clip> clip = copy(cel, mask.bounds());
    if (!clip) {
        return std::nullopt;
    }
    Rect r = rect_clip(mask.bounds(), cel.width(), cel.height());
    for (int j = 0; j < r.h; j++) {
        for (int i = 0; i < r.w; i++) {
            if (!mask.contains(r.x + i, r.y + j)) {
                clip->pixels_[static_cast<size_t>(j) * static_cast<size_t>(r.w) +
                              static_cast<size_t>(i)] = Rgba{};
            }
        }
    }
    return clip;
}

Rgba Clip::get(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return Rgba{};
    }
    return pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
}

void Clip::flipHorizontal()
{
    for (size_t y = 0; y < static_cast<size_t>(height_); y++) {
        auto row = pixels_.begin() + static_cast<std::ptrdiff_t>(y * static_cast<size_t>(width_));
        std::reverse(row, row + width_);
    }
}

void Clip::flipVertical()
{
    for (int y = 0; y < height_ / 2; y++) {
        auto top = pixels_.begin() + static_cast<std::ptrdiff_t>(y) * width_;
        auto bottom = pixels_.begin() + static_cast<std::ptrdiff_t>(height_ - 1 - y) * width_;
        std::swap_ranges(top, top + width_, bottom);
    }
}

void Clip::rotate(bool clockwise)
{
    /* The turned clip is height x width; each of its pixels comes from one source pixel. */
    int newWidth = height_;
    int newHeight = width_;
    std::vector<Rgba> turned;
    turned.reserve(pixels_.size());
    for (int y = 0; y < newHeight; y++) {
        for (int x = 0; x < newWidth; x++) {
            int sx = clockwise ? y : width_ - 1 - y;
            int sy = clockwise ? height_ - 1 - x : x;
            turned.push_back(get(sx, sy));
        }
    }
    pixels_ = std::move(turned);
    width_ = newWidth;
    height_ = newHeight;
}

void Clip::writeTo(History &history, const Cel &cel, int x, int y) const
{
    for (int j = 0; j < height_; j++) {
        for (int i = 0; i < width_; i++) {
            Rgba color = get(i, j);
            if (color.a != 0 && cel.inBounds(x + i, y + j)) {
                history.setPixel(x + i, y + j, color);
            }
        }
    }
}

bool clip_paste(History &history, Cel &cel, const Clip &clip, int x, int y, HistoryAction also)
{
    if (!history.beginStroke(cel)) {
        return false;
    }
    clip.writeTo(history, cel, x, y);
    return history.endStroke(std::move(also));
}

bool clip_replace(History &history, Cel &cel, Rect cleared, const Clip &clip, int x, int y,
                  HistoryAction also)
{
    if (!history.beginStroke(cel)) {
        return false;
    }
    Rect area = rect_clip(cleared, cel.width(), cel.height());
    for (int j = area.y; j < area.y + area.h; j++) {
        for (int i = area.x; i < area.x + area.w; i++) {
            history.setPixel(i, j, Rgba{});
        }
    }
    clip.writeTo(history, cel, x, y);
    return history.endStroke(std::move(also));
}

bool clip_replace(History &history, Cel &cel, const Mask &cleared, const Clip &clip, int x, int y,
                  HistoryAction also)
{
    if (!history.beginStroke(cel)) {
        return false;
    }
    cleared.forEach([&](int i, int j) { history.setPixel(i, j, Rgba{}); });
    clip.writeTo(history, cel, x, y);
    return history.endStroke(std::move(also));
}

bool clear_mask(History &history, Cel &cel, const Mask &mask, HistoryAction also)
{
    if (!history.beginStroke(cel)) {
        return false;
    }
    mask.forEach([&](int i, int j) { history.setPixel(i, j, Rgba{}); });
    return history.endStroke(std::move(also));
}
