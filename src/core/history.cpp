#include "tinysprite/history.h"

#include <memory>
#include <utility>

/* Restores old colors newest-first so a pixel written several times ends at its original value. */
void History::revert(const Step &step)
{
    if (!step.cel) {
        if (step.action.undo) {
            step.action.undo();
        }
        return;
    }
    Rgba *pixels = step.cel->pixels();
    for (auto it = step.deltas.rbegin(); it != step.deltas.rend(); ++it) {
        pixels[it->index] = it->oldColor;
    }
}

void History::apply(const Step &step)
{
    if (!step.cel) {
        if (step.action.redo) {
            step.action.redo();
        }
        return;
    }
    Rgba *pixels = step.cel->pixels();
    for (const Delta &delta : step.deltas) {
        pixels[delta.index] = delta.newColor;
    }
}

bool History::beginStroke(Cel &cel)
{
    if (strokeActive() || grouping_) {
        return false;
    }
    active_ = Step{};
    active_.cel = &cel;
    return true;
}

bool History::setPixel(int x, int y, Rgba color)
{
    if (!strokeActive() || !active_.cel->inBounds(x, y) ||
        (clip_ && !rect_contains(*clip_, x, y))) {
        return false;
    }
    size_t index = active_.cel->indexOf(x, y);
    Rgba &pixel = active_.cel->pixels()[index];
    if (pixel == color) {
        return true;
    }
    active_.deltas.push_back(Delta{index, pixel, color});
    pixel = color;
    return true;
}

void History::markSaved()
{
    saved_ = cursor_;
}

bool History::beginGroup()
{
    if (strokeActive() || grouping_) {
        return false;
    }
    grouping_ = true;
    return true;
}

bool History::endGroup()
{
    if (!grouping_) {
        return false;
    }
    grouping_ = false;
    std::vector<Step> steps = std::move(group_);
    group_.clear();
    if (steps.empty()) {
        return true;
    }
    /* The group becomes one action step that owns the steps it covers. */
    auto shared = std::make_shared<std::vector<Step>>(std::move(steps));
    Step group;
    group.action.undo = [shared] {
        for (auto it = shared->rbegin(); it != shared->rend(); ++it) {
            revert(*it);
        }
    };
    group.action.redo = [shared] {
        for (const Step &step : *shared) {
            apply(step);
        }
    };
    commit(std::move(group));
    return true;
}

void History::commit(Step &&step)
{
    if (grouping_) {
        group_.push_back(std::move(step));
        return;
    }
    if (saved_ && *saved_ > cursor_) {
        saved_.reset();
    }
    steps_.erase(steps_.begin() + static_cast<std::ptrdiff_t>(cursor_), steps_.end());
    steps_.push_back(std::move(step));
    cursor_ = steps_.size();
}

bool History::endStroke()
{
    if (!strokeActive()) {
        return false;
    }
    Step stroke = std::exchange(active_, Step{});
    if (!stroke.deltas.empty()) {
        commit(std::move(stroke));
    }
    return true;
}

bool History::cancelStroke()
{
    if (!strokeActive()) {
        return false;
    }
    revert(std::exchange(active_, Step{}));
    return true;
}

bool History::pushAction(HistoryAction action)
{
    if (strokeActive()) {
        return false;
    }
    Step step;
    step.action = std::move(action);
    commit(std::move(step));
    return true;
}

bool History::undo()
{
    if (!canUndo() || strokeActive() || grouping_) {
        return false;
    }
    revert(steps_[--cursor_]);
    return true;
}

bool History::redo()
{
    if (!canRedo() || strokeActive() || grouping_) {
        return false;
    }
    apply(steps_[cursor_++]);
    return true;
}

void History::forgetCel(Cel *cel)
{
    if (!cel) {
        return;
    }
    if (active_.cel == cel) {
        cancelStroke();
    }
    size_t kept = 0;
    size_t keptApplied = 0;
    for (size_t i = 0; i < steps_.size(); i++) {
        if (steps_[i].cel == cel) {
            continue;
        }
        if (i < cursor_) {
            keptApplied++;
        }
        if (kept != i) {
            steps_[kept] = std::move(steps_[i]);
        }
        kept++;
    }
    if (kept != steps_.size()) {
        saved_.reset();
    }
    steps_.resize(kept);
    cursor_ = keptApplied;
}

LiveStroke::LiveStroke(History &history, Cel &cel) : history_(history), cel_(cel)
{
    history_.beginStroke(cel_);
}

LiveStroke::~LiveStroke()
{
    cancel();
}

void LiveStroke::restart()
{
    history_.cancelStroke();
    history_.beginStroke(cel_);
}

bool LiveStroke::end()
{
    if (!open_) {
        return false;
    }
    open_ = false;
    return history_.endStroke();
}

void LiveStroke::cancel()
{
    if (open_) {
        open_ = false;
        history_.cancelStroke();
    }
}
