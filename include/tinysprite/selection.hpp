#ifndef TINYSPRITE_SELECTION_HPP
#define TINYSPRITE_SELECTION_HPP

#include <memory>
#include <optional>
#include <vector>

#include "tinysprite/history.hpp"
#include "tinysprite/mask.hpp"
#include "tinysprite/rect.hpp"

/* Selections: copying a rectangle of pixels out of a cel and moving one. */

/* A rectangle of pixels copied out of a cel: the clipboard behind copy / cut / paste, and the
 * scratch buffer for flipping and rotating a selection. Independent of the cel it came from. */
class Clip
{
  public:
    /* Copies the pixels of the rectangle, clipped to the cel. Returns nothing if the clipped
     * rectangle is empty. */
    static std::optional<Clip> copy(const Cel &cel, Rect area);
    /* The same for a selection of any shape: the clip covers the mask's bounds, clipped to the cel,
     * and the pixels outside the mask are transparent (so a paste or a move leaves what is under
     * them alone). Nothing for a mask with no pixel inside the cel. */
    static std::optional<Clip> copy(const Cel &cel, const Mask &mask);

    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }

    /* Returns transparent black for out-of-bounds coordinates. */
    Rgba get(int x, int y) const;

    void flipHorizontal();
    void flipVertical();

    /* Turns the clip by 90 degrees, swapping its width and height. */
    void rotate(bool clockwise);

  private:
    Clip(int width, int height) : width_(width), height_(height)
    {
    }

    friend bool clip_paste(History &history, Cel &cel, const Clip &clip, int x, int y,
                           HistoryAction also);
    friend bool clip_replace(History &history, Cel &cel, Rect cleared, const Clip &clip, int x,
                             int y, HistoryAction also);
    friend bool clip_replace(History &history, Cel &cel, const Mask &cleared, const Clip &clip,
                             int x, int y, HistoryAction also);

    /* Draws the pixels that are not fully transparent, as they are (no blending), with their
     * top-left corner at (x, y); those landing outside the cel are skipped. */
    void writeTo(History &history, const Cel &cel, int x, int y) const;

    int width_, height_;
    std::vector<Rgba> pixels_; /* row-major, width * height */
};

/* Writes the clip with its top-left corner at (x, y) as one undo step: pixels that are not fully
 * transparent replace whatever is there as they are (no blending), fully transparent ones leave
 * the destination alone, and pixels outside the cel are skipped. Returns true on success (including
 * a paste that changed nothing), false if a stroke is active. `also` runs along with the undo /
 * redo of the step (History::endStroke), for state that moves with the pixels, such as the
 * selection. */
bool clip_paste(History &history, Cel &cel, const Clip &clip, int x, int y,
                HistoryAction also = {});

/* Clears `cleared` and then pastes the clip with its top-left corner at (x, y), all in one undo
 * step. This is how a selection is flipped or rotated in place. Same return values as
 * clip_paste. */
bool clip_replace(History &history, Cel &cel, Rect cleared, const Clip &clip, int x, int y,
                  HistoryAction also = {});

/* The same as clip_replace, for a selection of any shape: only the pixels of `cleared` are cleared
 * before the clip is pasted (what lies in the rectangle around it is kept). */
bool clip_replace(History &history, Cel &cel, const Mask &cleared, const Clip &clip, int x, int y,
                  HistoryAction also = {});

/* Clears the pixels of `mask` (one undo step; `also` runs along with it). Returns false if a
 * stroke is active. */
bool clear_mask(History &history, Cel &cel, const Mask &mask, HistoryAction also = {});

/* Moves the pixels of a rectangular selection: the selected area is cleared and its non-transparent
 * pixels are written at the offset, over whatever is there (transparent pixels leave the
 * destination alone). Pixels moved outside the cel are lost. The selection is clipped to the cel
 * first. Returns false for an empty selection or an active stroke. */
bool move_stroke(History &history, Cel &cel, Rect selection, int dx, int dy);
/* The same for a selection of any shape: only the pixels of the mask are cleared and moved. */
bool move_stroke(History &history, Cel &cel, const Mask &selection, int dx, int dy);

/* A selection dragged with the mouse. The pixels are captured once; every update redraws the cel
 * from the original, so earlier previews never accumulate. It owns the history's active stroke
 * from begin until end / cancel (a move that ended where it began records no undo step). */
class MoveStroke : public LiveStroke
{
  public:
    /* Returns null for a selection with no pixels inside the cel or an active stroke. Nothing is
     * drawn until the first update. The history and cel must outlive the stroke. */
    static std::unique_ptr<MoveStroke> begin(History &history, Cel &cel, Rect selection);
    /* A selection of any shape: only its pixels are lifted and cleared. Null for a mask with no
     * pixel inside the cel. */
    static std::unique_ptr<MoveStroke> begin(History &history, Cel &cel, const Mask &selection);

    /* Shows the selection displaced by (dx, dy) from where it started (not from the last
     * update). */
    void update(int dx, int dy);

  private:
    MoveStroke(History &history, Cel &cel, Rect area, Clip pixels, std::optional<Mask> mask);

    /* Calls `visit(i, j)` for every selected cell of the area, as its offset from the corner. */
    void eachSelected(const std::function<void(int, int)> &visit) const;

    Rect area_;                /* selection clipped to the cel */
    Clip pixels_;              /* the selection as it was when the move began */
    std::optional<Mask> mask_; /* which pixels of area_ are selected; none: all of them */
};

#endif
