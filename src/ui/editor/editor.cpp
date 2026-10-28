#include "editor.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "drag.hpp"
#include "editor/detail.hpp"

Editor::Editor(SDL_Renderer *renderer, Sprite sprite)
    : renderer_(renderer), sprite_(std::move(sprite))
{
}

std::unique_ptr<Editor> Editor::create(SDL_Renderer *renderer, int width, int height)
{
    std::optional<Sprite> sprite = editor_detail::blankSprite(width, height);
    if (!sprite) {
        return nullptr;
    }
    std::unique_ptr<Editor> editor(new Editor(renderer, std::move(*sprite)));
    auto surface = CompositeSurface::create(renderer, width, height);
    if (!surface) {
        return nullptr;
    }
    editor->resetDocument(std::move(surface));
    return editor;
}

bool Editor::setSprite(Sprite sprite)
{
    if (stroking()) {
        return false;
    }
    auto surface = CompositeSurface::create(renderer_, sprite.width(), sprite.height());
    if (!surface) {
        return false;
    }
    sprite_ = std::move(sprite);
    resetDocument(std::move(surface));
    return true;
}

void Editor::resetDocument(std::unique_ptr<CompositeSurface> surface)
{
    retiredSurface_ = std::move(surface_);
    surface_ = std::move(surface);
    tileStroke_.reset(); /* before the history it records into is replaced */
    history_ = History{};
    selectedTilemap_ = -1;
    selectedTileset_ = -1;
    tilesRevision_++;
    lastDrawPoint_.reset();
    layers_.reset();
    frame_ = 0;
    selectedTag_ = -1;
    playback_.stop();
    selection_.reset();
    mask_.reset();
    lassoPath_.clear();
    selectedSliceName_.clear();
    slicePreview_.reset();
    slicePreviewMove_.reset();
    slicePreviewIndex_ = -1;
    paletteSelected_ = -1;
    modifiedOutsideHistory_ = false;
    view_ = ViewState{};
    markAllDirty();
}

Editor::~Editor()
{
    cancelDrag();
}

void Editor::markSaved()
{
    history_.markSaved();
    modifiedOutsideHistory_ = false;
}

bool Editor::newDocument(int width, int height)
{
    if (stroking()) {
        return false;
    }
    std::optional<Sprite> sprite = editor_detail::blankSprite(width, height);
    return sprite && setSprite(std::move(*sprite));
}

void Editor::changeBrushSize(int delta)
{
    options_.brush_size = std::clamp(options_.brush_size + delta, 1, PENCIL_BRUSH_MAX);
}

void Editor::setHover(int x, int y, bool hovering)
{
    hoverX_ = x;
    hoverY_ = y;
    hovering_ = hovering;
}

void Editor::setSelection(Rect r)
{
    selection_ = r;
    mask_.reset();
}

void Editor::setMoveOffset(int dx, int dy)
{
    moveDx_ = dx;
    moveDy_ = dy;
}

void Editor::selectTool(ToolKind kind)
{
    if (stroking() || static_cast<unsigned>(kind) >= static_cast<unsigned>(TOOL_COUNT)) {
        return;
    }
    tempPick_ = false;
    tool_ = kind;
}

void Editor::tempPick(bool on)
{
    if (stroking()) {
        return;
    }
    if (on && !tempPick_ && tool_ != ToolKind::Eyedropper) {
        pickReturn_ = tool_;
        tool_ = ToolKind::Eyedropper;
        tempPick_ = true;
    } else if (!on && tempPick_) {
        tool_ = pickReturn_;
        tempPick_ = false;
    }
}

void Editor::press(int cx, int cy, bool shift, bool ctrl)
{
    if (stroking()) {
        return;
    }
    if (playing()) {
        stopPlayback();
        return;
    }
    /* Drawing tools stay inside the selection; the select tool moves it and must not be clipped by
     * it. */
    history_.setClip(tool_draws(tool_) ? selection_ : std::nullopt);
    history_.setClipMask(tool_draws(tool_) ? mask_ : nullptr);
    shiftAtPress_ = shift;
    ctrlAtPress_ = ctrl;
    if (shift && tool_traits(tool_).group == ToolGroup::Brush) {
        lineFrom_ = lastDrawPoint_;
    }
    drag_ = beginDrag(*this, cx, cy);
    lineFrom_.reset();
    if (!drag_) {
        history_.setClip(std::nullopt);
        history_.setClipMask(nullptr);
    }
}

void Editor::drag(int cx, int cy, bool shift)
{
    if (drag_) {
        drag_->setShift(shift);
        drag_->update(cx, cy);
    }
}

void Editor::release()
{
    if (!drag_) {
        return;
    }
    finishingDrag_ = true;
    drag_->finish();
    finishingDrag_ = false;
    drag_.reset();
    history_.setClip(std::nullopt);
    history_.setClipMask(nullptr);
}

void Editor::cancelDrag()
{
    if (tileStroke_) {
        tileStroke_.reset(); /* cancels itself and puts the cells back */
        tilesChanged();
    }
    if (!drag_) {
        return;
    }
    drag_->cancel();
    drag_.reset();
    history_.setClip(std::nullopt);
    history_.setClipMask(nullptr);
    lassoPath_.clear();
    moveDx_ = 0;
    moveDy_ = 0;
    markAllDirty();
}

Rect Editor::spriteBounds() const
{
    return Rect{0, 0, sprite().width(), sprite().height()};
}

void Editor::markDirty(Rect area)
{
    revision_++;
    surface_->markDirty(area);
}

void Editor::markAllDirty()
{
    revision_++;
    surface_->markAllDirty();
}

void Editor::updateTexture()
{
    retiredSurface_.reset(); /* what the last frame drew has been rendered by now */
    OnionSkin ghosts = onion_;
    surface_->update(sprite(), frame_, onionOn_ && !playing() ? &ghosts : nullptr);
}

void Editor::zoomStep(int direction)
{
    float zoom = Viewport::nextZoom(view_.viewport.zoom, direction);
    view_.viewport.zoomAt(zoom, view_.canvas_w / 2.0f, view_.canvas_h / 2.0f);
    view_.auto_fit = false;
}

/* Undoing / redoing a layer operation can change the layer count under the active index. */
void Editor::afterHistoryStep(const Layer *active, const std::vector<Sprite::Tag> *tagsBefore)
{
    layers_.refresh(sprite(), active);
    frame_ = std::clamp(frame_, 0, sprite().frameCount() - 1);
    if (tagsBefore) {
        selectedTag_ = editor_detail::followTag(*tagsBefore, sprite().tags(), selectedTag_);
    }
    if (selectedTag_ >= static_cast<int>(sprite().tags().size())) {
        selectedTag_ = -1;
    }
    if (paletteSelected_ >= sprite().palette().count()) {
        paletteSelected_ = -1; /* the palette it pointed into changed with the step */
    }
    tilesRevision_++;
    markAllDirty();
}

/* Undoes or redoes one history step and brings the editor in line with it. Refused while a tile
 * stroke is open: it is not in the history, which would not refuse. */
void Editor::stepHistory(bool undo)
{
    if (stroking()) {
        return;
    }
    stopPlayback();
    const Layer *active = sprite().layer(layers_.active());
    const std::vector<Sprite::Tag> tags = sprite().tags();
    if (undo ? history().undo() : history().redo()) {
        afterHistoryStep(active, &tags);
    }
}

void Editor::undo()
{
    stepHistory(true);
}

void Editor::redo()
{
    stepHistory(false);
}

bool Editor::hoverInSprite() const
{
    return hovering_ && hoverX_ >= 0 && hoverY_ >= 0 && hoverX_ < sprite().width() &&
           hoverY_ < sprite().height();
}

Cel *Editor::activeCel()
{
    Layer *layer = sprite().layer(layers_.active());
    return layer ? layer->cel(frame_) : nullptr;
}

Cel *Editor::editableCel()
{
    Layer *layer = sprite().layer(layers_.active());
    return stroking() || playing() || !layer || !layer->visible() || layer->locked() ? nullptr
                                                                                     : activeCel();
}
