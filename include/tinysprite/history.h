#ifndef TINYSPRITE_HISTORY_H
#define TINYSPRITE_HISTORY_H

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "tinysprite/cel.h"
#include "tinysprite/rect.h"

/* A non-pixel edit with its own undo / redo (layer operations). The callables are destroyed
 * exactly once when the step is dropped (a new edit discards it from the redo side, or the history
 * goes away), so state they own is released with them. That must not touch objects other steps
 * refer to, since steps are dropped oldest-first. Either callable may be empty. */
struct HistoryAction {
    std::function<void()> undo;
    std::function<void()> redo;
};

/* Undo/redo log. One undo step is one stroke; internally a stroke is a list of pixel deltas
 * (index, old, new). Every pixel edit must go through History::setPixel. Edits that are not pixel
 * deltas are recorded as actions, see pushAction.
 * A cel must outlive every history entry that refers to it. */
class History
{
  public:
    /* Starts a stroke on `cel`. Returns false if a stroke is already active. */
    bool beginStroke(Cel &cel);

    /* Writes a pixel into the active stroke's cel and records the delta. Writing the color a pixel
     * already has records nothing. Returns false if no stroke is active or (x, y) is out of
     * bounds. */
    bool setPixel(int x, int y, Rgba color);

    /* Commits the active stroke as one undo step and discards the redo steps. A stroke that
     * changed nothing is dropped. Returns false if no stroke is active. */
    bool endStroke();

    /* Reverts every pixel written by the active stroke and discards it without touching the
     * undo/redo steps. Returns false if no stroke is active. */
    bool cancelStroke();

    /* True while a stroke is open (between beginStroke and endStroke / cancelStroke). */
    bool strokeActive() const
    {
        return active_.cel != nullptr;
    }

    /* Limits setPixel to the cels inside `area` (nullopt lifts the limit); a pixel outside it is
     * rejected like one out of bounds. Undo and redo are not affected. */
    void setClip(std::optional<Rect> area)
    {
        clip_ = area;
    }

    /* Records an action whose effect the caller has already applied, as one undo step, and
     * discards the redo steps. Returns false (dropping the action) if a stroke is active. */
    bool pushAction(HistoryAction action);

    bool canUndo() const
    {
        return cursor_ > 0;
    }
    bool canRedo() const
    {
        return cursor_ < steps_.size();
    }

    /* Return false if there is nothing to undo/redo or a stroke is active. */
    bool undo();
    bool redo();

    /* Remembers the current state as the saved one. atSavePoint() is true again whenever undo /
     * redo bring the history back to it, and false once an edit replaces the steps it stood on
     * (or forgetCel drops steps). A new history counts as saved. */
    void markSaved();
    bool atSavePoint() const
    {
        return saved_ && *saved_ == cursor_;
    }

    /* Drops every undo/redo step that touches `cel` (cancelling the active stroke if it is on
     * `cel`), so the cel can be destroyed. The remaining steps keep their order and the undo/redo
     * boundary. Steps on other cels are assumed independent of the dropped ones. */
    void forgetCel(Cel *cel);

  private:
    struct Delta {
        size_t index;
        Rgba oldColor;
        Rgba newColor;
    };

    /* One undo step: pixel deltas on `cel`, or (cel == nullptr) an action. */
    struct Step {
        Cel *cel = nullptr;
        std::vector<Delta> deltas;
        HistoryAction action;
    };

    static void revert(const Step &step);
    static void apply(const Step &step);

    /* Appends `step` after the cursor, discarding the redo steps. */
    void commit(Step &&step);

    std::vector<Step> steps_; /* steps_[0..cursor_) are applied, the rest can be redone */
    size_t cursor_ = 0;
    std::optional<Rect> clip_;
    std::optional<size_t> saved_ = 0; /* cursor_ value of the saved state, if still reachable */
    Step active_; /* the open stroke; it is open exactly while active_.cel is set */
};

/* Base of the strokes that are built while the mouse drags (pencil, shape, move): it owns the
 * history's open stroke from construction until end() / cancel(), and cancels it if destroyed
 * unfinished. The history and cel must outlive it. */
class LiveStroke
{
  public:
    LiveStroke(const LiveStroke &) = delete;
    LiveStroke &operator=(const LiveStroke &) = delete;

    /* Commits the stroke as one undo step. */
    bool end();

    /* Reverts the stroke's pixels and records no undo step. */
    void cancel();

  protected:
    /* The history must not have an open stroke (the subclasses' begin() checks that). */
    LiveStroke(History &history, Cel &cel);
    ~LiveStroke();

    /* Reverts the pixels drawn so far and starts over, so a preview can be redrawn from the
     * original pixels. */
    void restart();

    History &history_;
    Cel &cel_;

  private:
    bool open_ = true;
};

#endif
