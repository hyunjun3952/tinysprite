#include "tinysprite/filter_ops.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{

/* Calls `pixel(x, y)` for the cels of `area` (the whole cel without one); it returns the color to
 * write there, and a stroke records the ones that differ. Returns false while a stroke is open. */
template <class F> bool paint(History &history, Cel &cel, std::optional<Rect> area, F &&pixel)
{
    Rect r =
        area ? rect_clip(*area, cel.width(), cel.height()) : Rect{0, 0, cel.width(), cel.height()};
    if (!history.beginStroke(cel)) {
        return false;
    }
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            history.setPixel(x, y, pixel(x, y));
        }
    }
    history.endStroke();
    return true;
}

/* Whether (dx, dy) is a neighbour to look at: not the pixel itself, and a diagonal one only when
 * `diagonals`. */
bool isNeighbour(int dx, int dy, bool diagonals)
{
    const bool diagonal = dx != 0 && dy != 0;
    return (dx != 0 || dy != 0) && (diagonals || !diagonal);
}

bool touchesPixel(const Cel &cel, int x, int y, bool diagonals)
{
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (isNeighbour(dx, dy, diagonals) && cel.get(x + dx, y + dy).a != 0) {
                return true;
            }
        }
    }
    return false;
}

struct Hsl {
    double h; /* degrees, 0..360 */
    double s, l;
};

double hueOf(double r, double g, double b, double max, double delta)
{
    double h = 0;
    if (max == r) {
        h = std::fmod((g - b) / delta, 6.0);
    } else if (max == g) {
        h = (b - r) / delta + 2.0;
    } else {
        h = (r - g) / delta + 4.0;
    }
    return h < 0 ? h * 60.0 + 360.0 : h * 60.0;
}

Hsl toHsl(Rgba c)
{
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double max = std::max({r, g, b}), min = std::min({r, g, b});
    double delta = max - min;
    Hsl out{0, 0, (max + min) / 2};
    if (delta > 0) {
        out.s = delta / (1 - std::fabs(2 * out.l - 1));
        out.h = hueOf(r, g, b, max, delta);
    }
    return out;
}

uint8_t channel(double v)
{
    return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

Rgba fromHsl(Hsl c, uint8_t alpha)
{
    double chroma = (1 - std::fabs(2 * c.l - 1)) * c.s;
    double sector = c.h / 60.0;
    double x = chroma * (1 - std::fabs(std::fmod(sector, 2.0) - 1));
    double r = 0, g = 0, b = 0;
    switch (static_cast<int>(sector) % 6) {
    case 0:
        r = chroma, g = x;
        break;
    case 1:
        r = x, g = chroma;
        break;
    case 2:
        g = chroma, b = x;
        break;
    case 3:
        g = x, b = chroma;
        break;
    case 4:
        r = x, b = chroma;
        break;
    default:
        r = chroma, b = x;
        break;
    }
    double m = c.l - chroma / 2;
    return Rgba{channel(r + m), channel(g + m), channel(b + m), alpha};
}

Rgba adjusted(Rgba c, int hueDegrees, int brightnessPercent)
{
    if (c.a == 0) {
        return c;
    }
    Hsl hsl = toHsl(c);
    hsl.h = std::fmod(std::fmod(hsl.h + hueDegrees, 360.0) + 360.0, 360.0);
    hsl.l = std::clamp(hsl.l + brightnessPercent / 100.0, 0.0, 1.0);
    return fromHsl(hsl, c.a);
}

} // namespace

bool filter_outline(History &history, Cel &cel, Rgba color, bool diagonals,
                    std::optional<Rect> area)
{
    /* The outline is computed from a copy, so pixels it writes never count as neighbours. */
    const Cel source = cel;
    return paint(history, cel, area, [&](int x, int y) {
        Rgba now = source.get(x, y);
        return now.a == 0 && touchesPixel(source, x, y, diagonals) ? color : now;
    });
}

bool filter_replace_color(History &history, Cel &cel, Rgba from, Rgba to, std::optional<Rect> area)
{
    return paint(history, cel, area, [&](int x, int y) {
        Rgba now = cel.get(x, y);
        return now == from ? to : now;
    });
}

bool filter_hue_brightness(History &history, Cel &cel, int hueDegrees, int brightnessPercent,
                           std::optional<Rect> area)
{
    brightnessPercent = std::clamp(brightnessPercent, -100, 100);
    if (hueDegrees % 360 == 0 && brightnessPercent == 0) {
        return !history.strokeActive();
    }
    return paint(history, cel, area, [&](int x, int y) {
        return adjusted(cel.get(x, y), hueDegrees % 360, brightnessPercent);
    });
}

bool filter_apply(History &history, Cel &cel, const FilterParams &params, std::optional<Rect> area)
{
    switch (params.kind) {
    case FilterParams::Kind::Outline:
        return filter_outline(history, cel, params.color, params.diagonals, area);
    case FilterParams::Kind::ReplaceColor:
        return filter_replace_color(history, cel, params.from, params.to, area);
    case FilterParams::Kind::HueBrightness:
        return filter_hue_brightness(history, cel, params.hue, params.brightness, area);
    }
    return false;
}

namespace
{

struct CelChange {
    Cel *cel;
    size_t index;
    Rgba before, after;
};

/* The pixels `params` would change in `cel`, found by running it on a copy. */
void collectChanges(const Cel &cel, Cel &target, const FilterParams &params,
                    std::optional<Rect> area, std::vector<CelChange> &out)
{
    Cel copy = cel;
    History scratch;
    filter_apply(scratch, copy, params, area);
    size_t count = static_cast<size_t>(cel.width()) * static_cast<size_t>(cel.height());
    for (size_t i = 0; i < count; i++) {
        if (copy.pixels()[i] != cel.pixels()[i]) {
            out.push_back(CelChange{&target, i, cel.pixels()[i], copy.pixels()[i]});
        }
    }
}

} // namespace

bool filter_apply_cels(History &history, const std::vector<Cel *> &cels, const FilterParams &params,
                       std::optional<Rect> area)
{
    if (history.strokeActive()) {
        return false;
    }
    std::vector<CelChange> changes;
    for (Cel *cel : cels) {
        collectChanges(*cel, *cel, params, area, changes);
    }
    if (changes.empty()) {
        return true;
    }
    auto shared = std::make_shared<std::vector<CelChange>>(std::move(changes));
    HistoryAction action;
    action.bytes = shared->size() * sizeof(CelChange);
    action.undo = [shared] {
        for (auto it = shared->rbegin(); it != shared->rend(); ++it) {
            it->cel->pixels()[it->index] = it->before;
        }
    };
    action.redo = [shared] {
        for (const CelChange &c : *shared) {
            c.cel->pixels()[c.index] = c.after;
        }
    };
    return history.perform(std::move(action));
}
