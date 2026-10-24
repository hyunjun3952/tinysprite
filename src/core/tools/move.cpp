#include "tinysprite/selection.hpp"

#include <functional>
#include <utility>

MoveStroke::MoveStroke(History &history, Cel &cel, Rect area, Clip pixels, std::optional<Mask> mask)
    : LiveStroke(history, cel), area_(area), pixels_(std::move(pixels)), mask_(std::move(mask))
{
}

std::unique_ptr<MoveStroke> MoveStroke::begin(History &history, Cel &cel, Rect selection)
{
    Rect area = rect_clip(selection, cel.width(), cel.height());
    std::optional<Clip> pixels = Clip::copy(cel, area);
    if (history.strokeActive() || !pixels) {
        return nullptr;
    }
    return std::unique_ptr<MoveStroke>(
        new MoveStroke(history, cel, area, std::move(*pixels), std::nullopt));
}

std::unique_ptr<MoveStroke> MoveStroke::begin(History &history, Cel &cel, const Mask &selection)
{
    Rect area = rect_clip(selection.bounds(), cel.width(), cel.height());
    std::optional<Clip> pixels = Clip::copy(cel, selection);
    if (history.strokeActive() || !pixels) {
        return nullptr;
    }
    return std::unique_ptr<MoveStroke>(
        new MoveStroke(history, cel, area, std::move(*pixels), selection));
}

void MoveStroke::eachSelected(const std::function<void(int, int)> &visit) const
{
    for (int j = 0; j < area_.h; j++) {
        for (int i = 0; i < area_.w; i++) {
            if (!mask_ || mask_->contains(area_.x + i, area_.y + j)) {
                visit(i, j);
            }
        }
    }
}

void MoveStroke::update(int dx, int dy)
{
    restart();
    if (dx == 0 && dy == 0) {
        return;
    }
    /* Pixels outside the cel are lost, which is how a selection dragged off the edge gets
     * clipped. */
    auto put = [this](int x, int y, Rgba color) {
        if (cel_.inBounds(x, y)) {
            history_.setPixel(x, y, color);
        }
    };
    eachSelected([&](int i, int j) { put(area_.x + i, area_.y + j, Rgba{}); });
    eachSelected([&](int i, int j) {
        Rgba pixel = pixels_.get(i, j);
        if (pixel.a != 0) {
            put(area_.x + i + dx, area_.y + j + dy, pixel);
        }
    });
}
bool move_stroke(History &history, Cel &cel, Rect selection, int dx, int dy)
{
    std::unique_ptr<MoveStroke> stroke = MoveStroke::begin(history, cel, selection);
    if (!stroke) {
        return false;
    }
    stroke->update(dx, dy);
    return stroke->end();
}

bool move_stroke(History &history, Cel &cel, const Mask &selection, int dx, int dy)
{
    std::unique_ptr<MoveStroke> stroke = MoveStroke::begin(history, cel, selection);
    if (!stroke) {
        return false;
    }
    stroke->update(dx, dy);
    return stroke->end();
}
