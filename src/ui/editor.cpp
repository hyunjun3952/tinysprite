#include "editor.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "drag.h"
#include "editor_internal.h"

Editor::Editor(SDL_Renderer *renderer, Sprite sprite)
    : renderer_(renderer), sprite_(std::move(sprite))
{
}

std::unique_ptr<SDL_Texture, Editor::TextureDeleter> Editor::newTexture(int width, int height) const
{
    std::unique_ptr<SDL_Texture, TextureDeleter> texture(SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height));
    if (texture) {
        SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture.get(), SDL_ScaleModeNearest);
    }
    return texture;
}

std::unique_ptr<Editor> Editor::create(SDL_Renderer *renderer, int width, int height)
{
    std::optional<Sprite> sprite;
    try {
        sprite.emplace(width, height);
    } catch (const std::invalid_argument &) {
        return nullptr;
    }
    sprite->palette() = editor_detail::defaultPalette();
    std::unique_ptr<Editor> editor(new Editor(renderer, std::move(*sprite)));
    auto texture = editor->newTexture(width, height);
    if (!texture) {
        return nullptr;
    }
    editor->resetDocument(std::move(texture));
    return editor;
}

bool Editor::setSprite(Sprite sprite)
{
    if (stroking()) {
        return false;
    }
    auto texture = newTexture(sprite.width(), sprite.height());
    if (!texture) {
        return false;
    }
    sprite_ = std::move(sprite);
    resetDocument(std::move(texture));
    return true;
}

void Editor::resetDocument(std::unique_ptr<SDL_Texture, TextureDeleter> texture)
{
    texture_ = std::move(texture);
    pixels_.assign(static_cast<size_t>(sprite_.width()) * static_cast<size_t>(sprite_.height()),
                   Rgba{});
    history_ = History{};
    lastDrawPoint_.reset();
    activeLayer_ = 0;
    picked_.clear();
    anchor_ = nullptr;
    hasSelection_ = false;
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
    std::optional<Sprite> sprite;
    try {
        sprite.emplace(width, height);
    } catch (const std::invalid_argument &) {
        return false;
    }
    sprite->palette() = editor_detail::defaultPalette();
    return setSprite(std::move(*sprite));
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
    hasSelection_ = true;
    selection_ = r;
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

void Editor::press(int cx, int cy, bool shift)
{
    if (!stroking()) {
        /* Drawing tools stay inside the selection; the select tool moves it and must not be
         * clipped by it. */
        bool draws = tool_ != ToolKind::Select && tool_ != ToolKind::Eyedropper;
        history_.setClip(draws && hasSelection_ ? std::optional<Rect>(selection_) : std::nullopt);
        if (shift && (tool_ == ToolKind::Pencil || tool_ == ToolKind::Eraser)) {
            lineFrom_ = lastDrawPoint_;
        }
        drag_ = beginDrag(*this, cx, cy);
        lineFrom_.reset();
        if (!drag_) {
            history_.setClip(std::nullopt);
        }
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
    if (drag_) {
        drag_->finish();
        drag_.reset();
        history_.setClip(std::nullopt);
    }
}

void Editor::cancelDrag()
{
    if (!drag_) {
        return;
    }
    drag_->cancel();
    drag_.reset();
    history_.setClip(std::nullopt);
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
    dirtyRect_ = dirty_ ? rect_union(dirtyRect_, area) : area;
    dirty_ = true;
}

void Editor::markAllDirty()
{
    revision_++;
    dirtyRect_ = spriteBounds();
    dirty_ = true;
}

void Editor::updateTexture()
{
    if (!dirty_) {
        return;
    }
    dirty_ = false;
    int width = sprite().width();
    Rect r = rect_clip(dirtyRect_, width, sprite().height());
    if (rect_is_empty(r)) {
        return;
    }
    composite_sprite(sprite(), frame_, r, pixels_.data());
    SDL_Rect rect = {r.x, r.y, r.w, r.h};
    const Rgba *origin =
        &pixels_[static_cast<size_t>(r.y) * static_cast<size_t>(width) + static_cast<size_t>(r.x)];
    SDL_UpdateTexture(texture(), &rect, origin, width * static_cast<int>(sizeof(Rgba)));
}

void Editor::zoomStep(int direction)
{
    float zoom = Viewport::nextZoom(view_.viewport.zoom, direction);
    view_.viewport.zoomAt(zoom, view_.canvas_w / 2.0f, view_.canvas_h / 2.0f);
    view_.auto_fit = false;
}

/* Undoing / redoing a layer operation can change the layer count under the active index. */
void Editor::afterHistoryStep(const Layer *active)
{
    /* Keep pointing at the same layer when it is still there. */
    int found = active ? editor_detail::indexOfLayer(sprite(), active) : -1;
    activeLayer_ = found >= 0 ? found : std::min(activeLayer_, sprite().layerCount() - 1);
    /* Layers are tracked by address: forget the ones that left the sprite, so a new layer that
     * reuses an address is not taken for a selected one. */
    picked_.erase(std::remove_if(picked_.begin(), picked_.end(),
                                 [this](const Layer *l) {
                                     return editor_detail::indexOfLayer(sprite(), l) < 0;
                                 }),
                  picked_.end());
    if (anchor_ && editor_detail::indexOfLayer(sprite(), anchor_) < 0) {
        anchor_ = nullptr;
    }
    markAllDirty();
}

void Editor::undo()
{
    const Layer *active = sprite().layer(activeLayer_);
    if (history().undo()) {
        afterHistoryStep(active);
    }
}

void Editor::redo()
{
    const Layer *active = sprite().layer(activeLayer_);
    if (history().redo()) {
        afterHistoryStep(active);
    }
}

bool Editor::hoverInSprite() const
{
    return hovering_ && hoverX_ >= 0 && hoverY_ >= 0 && hoverX_ < sprite().width() &&
           hoverY_ < sprite().height();
}

Cel *Editor::activeCel()
{
    Layer *layer = sprite().layer(activeLayer_);
    return layer ? layer->cel(frame_) : nullptr;
}

Cel *Editor::editableCel()
{
    Layer *layer = sprite().layer(activeLayer_);
    return stroking() || !layer || !layer->visible() || layer->locked() ? nullptr : activeCel();
}
