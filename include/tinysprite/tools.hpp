#ifndef TINYSPRITE_TOOLS_HPP
#define TINYSPRITE_TOOLS_HPP

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "tinysprite/history.hpp"
#include "tinysprite/raster.hpp"
#include "tinysprite/sprite.hpp"
#include "tinysprite/symmetry.hpp"

/* The drawing tools. Every one that edits a cel goes through History. */

/* Draws one stroke through the points: a single point plots a pixel, several points are joined by
 * straight lines. Pixels outside the cel are skipped. Returns true on success (including strokes
 * that changed nothing), false for no points or an active stroke. */
bool pencil_stroke(History &history, Cel &cel, const std::vector<ToolPoint> &points, Rgba color);

inline constexpr int PENCIL_BRUSH_MAX = 64;

/* Options for live strokes. Symmetry mirrors every pixel across the cel's center line(s); a
 * mirrored copy that falls outside the cel is clipped. Pixel-perfect drops the middle pixel of
 * every L-shaped corner so diagonal runs stay one pixel wide; it only applies to a one-pixel
 * brush. `size` is the brush width in pixels (values below 1 mean 1, above PENCIL_BRUSH_MAX are
 * capped): the brush covers size x size pixels around each plotted point, an even size reaching
 * one pixel further right and down, with the corners cut off when `round` is set. All fields
 * default to off (one-pixel square brush). */
struct PencilOptions {
    bool pixel_perfect = false;
    Symmetry symmetry = {};
    int size = 0;
    bool round = false;

    /* The brush these options describe. brushSize is the effective width (1..PENCIL_BRUSH_MAX);
     * the brush covers the pixels (i, j), 0 <= i, j < size, for which brushContains is true, laid
     * out so that plotting (x, y) paints (x - origin + i, y - origin + j). */
    int brushSize() const;
    int brushOrigin() const;
    bool brushContains(int i, int j) const;
};

/* A stroke built incrementally while the mouse is dragging. It owns the history's active stroke
 * from begin until end / cancel. */
class PencilStroke : public LiveStroke
{
  public:
    /* Starts a stroke at (x, y), plotting that pixel in `color` (pass transparent to erase).
     * Returns null if the history already has an active stroke. The history and cel must outlive
     * the stroke. */
    static std::unique_ptr<PencilStroke> begin(History &history, Cel &cel, Rgba color,
                                               PencilOptions options, int x, int y);

    /* Joins the previous point to (x, y) with a line. Pixels outside the cel are skipped. */
    void extend(int x, int y);

  private:
    /* One plotted pixel and the (up to four) cel positions it touched with the colors they had
     * before, so a pixel-perfect correction can put them back. */
    struct Placed {
        int x = 0, y = 0;
        struct Copy {
            ToolPoint at;
            Rgba before;
        };
        std::vector<Copy> copies;
    };

    PencilStroke(History &history, Cel &cel, Rgba color, PencilOptions options, int x, int y);
    void buildBrush();
    void buildLeadingEdges();

    /* Pixel-perfect only applies to a one-pixel brush. */
    bool usesPixelPerfect() const
    {
        return options_.pixel_perfect && options_.brushSize() == 1;
    }

    void plotPixel(int x, int y);
    Placed placePixel(int x, int y);
    void unplacePixel(const Placed &placed);
    void paint(int x, int y, const std::vector<ToolPoint> &cels);
    void plotPixelPerfect(int x, int y);

    Rgba color_;
    PencilOptions options_;
    int lastX_, lastY_;
    std::vector<ToolPoint> brush_; /* the brush's cels relative to the plotted point */
    /* For a one-cel step (dx, dy) of the brush: its cels the previous stamp did not cover, indexed
     * by (dy + 1) * 3 + (dx + 1). */
    std::array<std::vector<ToolPoint>, 9> leadingEdge_;
    bool hasPlotted_ = false;
    int plotX_ = 0, plotY_ = 0;          /* the last plotted point */
    std::vector<unsigned char> painted_; /* cels this stroke painted (non-pixel-perfect) */
    std::vector<Placed> window_;         /* pixel-perfect: the last two kept pixels, oldest first */
};

enum class ShapeKind {
    Line,
    Rect,
    Ellipse,
    Triangle, /* apex at the top middle of the box */
    Diamond,
    Hexagon, /* flat top and bottom, points left and right */
    Star,    /* five points, the top one at the middle */
    Cross,   /* a plus sign */
    Count,
};

inline constexpr int SHAPE_COUNT = static_cast<int>(ShapeKind::Count);

/* Whether `filled` means anything for the kind: true for every closed shape, false for a line. */
bool shape_has_fill(ShapeKind kind);

/* What to draw and how. Every shape fits the box spanned by its two points; `filled` is ignored for
 * lines. With symmetry the shape is also drawn reflected across each enabled axis; reflected parts
 * outside the cel are clipped. The brush (the PencilOptions size / round rules: values below 1 mean
 * 1, above PENCIL_BRUSH_MAX are capped) widens the outline of a line or an unfilled shape: it is
 * stamped around every outline pixel, an even size reaching one pixel further right and down
 * (mirrored with the shape). A filled shape is drawn without it. */
struct ShapeStyle {
    ShapeKind kind = ShapeKind::Line;
    bool filled = false;
    Rgba color = {};
    Symmetry symmetry = {};
    int brush_size = 1;
    bool brush_round = false;
};

/*
 * Where a shape dragged from `anchor` to `to` ends when it is held to even proportions (Shift).
 * A line becomes horizontal when the drag is more than twice as wide as it is tall, vertical when
 * it is more than twice as tall as wide, and 45 degrees otherwise. Every other shape gets a
 * square box (so the rectangle is a square, the ellipse a circle) whose side is the longer of
 * the two drag distances.
 */
ToolPoint shape_constrain(ShapeKind kind, ToolPoint anchor, ToolPoint to);

/* Draws the shape spanning `from`-`to` as one undo step. Pixels outside the cel are skipped.
 * Returns false if a stroke is active. */
bool shape_stroke(History &history, Cel &cel, ShapeStyle style, ToolPoint from, ToolPoint to);

/* A shape dragged out with the mouse: it is anchored at the press point and redrawn from scratch
 * on every update, so shrinking it restores the pixels underneath. It owns the history's active
 * stroke from begin until end / cancel. */
class ShapeStroke : public LiveStroke
{
  public:
    /* Starts a shape anchored at `at`, plotting that pixel. Returns null if the history already
     * has an active stroke. The history and cel must outlive the stroke. */
    static std::unique_ptr<ShapeStroke> begin(History &history, Cel &cel, ShapeStyle style,
                                              ToolPoint at);

    /* Replaces the preview with the shape from the anchor to (x, y). */
    void update(int x, int y);

  private:
    ShapeStroke(History &history, Cel &cel, ShapeStyle style, ToolPoint anchor);

    ShapeStyle style_;
    ToolPoint anchor_;
};

/* Replaces the pixels that match the one at (x, y) with `color` as one undo step: by default the
 * 4-connected region of exactly-equal pixels, or with `options` every such pixel of the cel and/or
 * the ones within a tolerance (see FillOptions). With symmetry the region under each reflection of
 * (x, y) is filled too. Filling with the color the region already has records nothing. Returns
 * false if (x, y) is out of bounds or a stroke is active (the cel is then left unchanged). */
bool bucket_fill(History &history, Cel &cel, int x, int y, Rgba color, Symmetry symmetry,
                 const FillOptions &options = FillOptions());

/* Reads the color at (x, y) of a single cel. Returns nothing if (x, y) is out of bounds. */
std::optional<Rgba> eyedropper_pick_cel(const Cel &cel, int x, int y);

/* Reads the color at (x, y) of the flattened frame (visible layers, as displayed). Returns nothing
 * if the frame is out of range or (x, y) is out of bounds. */
std::optional<Rgba> eyedropper_pick_sprite(const Sprite &sprite, int frame, int x, int y);

#endif
