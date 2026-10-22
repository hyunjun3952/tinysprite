#include "tinysprite/selection.hpp"

#include <functional>
#include <utility>

MoveStroke::MoveStroke(History &history, Cel &cel, Rect area, Clip pixels)
    : LiveStroke(history, cel), area_(area), pixels_(std::move(pixels))
{
}

std::unique_ptr<MoveStroke> MoveStroke::begin(History &history, Cel &cel, Rect selection)
{
    Rect area = rect_clip(selection, cel.width(), cel.height());
    std::optional<Clip> pixels = Clip::copy(cel, area);
    if (history.strokeActive() || !pixels) {
        return nullptr;
    }
    return std::unique_ptr<MoveStroke>(new MoveStroke(history, cel, area, std::move(*pixels)));
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
    auto eachCell = [this](const std::function<void(int, int)> &visit) {
        for (int j = 0; j < area_.h; j++) {
            for (int i = 0; i < area_.w; i++) {
                visit(i, j);
            }
        }
    };
    eachCell([&](int i, int j) { put(area_.x + i, area_.y + j, Rgba{}); });
    eachCell([&](int i, int j) {
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
