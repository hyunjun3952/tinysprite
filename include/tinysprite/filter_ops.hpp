#ifndef TINYSPRITE_FILTER_OPS_HPP
#define TINYSPRITE_FILTER_OPS_HPP

#include <optional>
#include <vector>

#include "tinysprite/history.hpp"
#include "tinysprite/rect.hpp"

/* Filters: whole-cel pixel effects, each recorded as one undo step (a pixel stroke). They return
 * false only while a stroke is active; a filter that changes nothing returns true and records
 * nothing. `area`, when given, limits which pixels are written (neighbours are still read from
 * outside it); it is clipped to the cel. */

/* Draws `color` on every transparent pixel that touches a pixel with any alpha: on its four sides,
 * or also on its diagonals. The outline is read from the cel as it was, so it is one pixel wide. */
bool filter_outline(History &history, Cel &cel, Rgba color, bool diagonals,
                    std::optional<Rect> area = std::nullopt);

/* Replaces every pixel equal to `from` (alpha included) by `to`. */
bool filter_replace_color(History &history, Cel &cel, Rgba from, Rgba to,
                          std::optional<Rect> area = std::nullopt);

/* Turns the hue of every non-transparent pixel by `hueDegrees` (any value, it wraps) and moves its
 * lightness by `brightnessPercent`, clamped to -100 (black) .. 100 (white). Alpha is kept. */
bool filter_hue_brightness(History &history, Cel &cel, int hueDegrees, int brightnessPercent,
                           std::optional<Rect> area = std::nullopt);

/* One filter and its settings, for a caller that picks the filter at run time (a dialog). */
struct FilterParams {
    enum class Kind { Outline, ReplaceColor, HueBrightness };
    Kind kind = Kind::Outline;
    Rgba color{0, 0, 0, 255}; /* Outline: the line color */
    bool diagonals = false;   /* Outline: also around the corners */
    Rgba from, to;            /* ReplaceColor */
    int hue = 0;              /* HueBrightness: degrees */
    int brightness = 0;       /* HueBrightness: percent */
};

/* Runs the filter `params` describes. */
bool filter_apply(History &history, Cel &cel, const FilterParams &params,
                  std::optional<Rect> area = std::nullopt);

/* Runs the filter on several cels as one undo step (a History group cannot hold strokes, so the
 * changes of all cels are kept together in one action). Returns false while a stroke is active;
 * nothing is recorded when no pixel changes. The cels must outlive the history step. */
bool filter_apply_cels(History &history, const std::vector<Cel *> &cels, const FilterParams &params,
                       std::optional<Rect> area = std::nullopt);

#endif
