#include "editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#include "drag.h"

#include "tinysprite/layer_ops.h"

namespace
{

uint8_t toByte(float value)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
}

void setFloats(float out[4], Rgba color)
{
    out[0] = static_cast<float>(color.r) / 255.0f;
    out[1] = static_cast<float>(color.g) / 255.0f;
    out[2] = static_cast<float>(color.b) / 255.0f;
    out[3] = static_cast<float>(color.a) / 255.0f;
}

/* A starting palette so the panel is useful before the first color is added. */
const Rgba DEFAULT_PALETTE[] = {
    {0, 0, 0, 255},      {255, 255, 255, 255}, {127, 127, 127, 255}, {195, 195, 195, 255},
    {136, 0, 21, 255},   {237, 28, 36, 255},   {255, 127, 39, 255},  {255, 242, 0, 255},
    {34, 177, 76, 255},  {0, 162, 232, 255},   {63, 72, 204, 255},   {163, 73, 164, 255},
    {185, 122, 87, 255}, {255, 174, 201, 255}, {255, 201, 14, 255},  {181, 230, 29, 255},
};

} // namespace

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
    for (const Rgba &color : DEFAULT_PALETTE) {
        sprite->palette().add(color);
    }
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
    view_ = ViewState{};
    markAllDirty();
}

Editor::~Editor()
{
    cancelDrag();
}

Rgba Editor::foreground() const
{
    return Rgba{toByte(color_[0]), toByte(color_[1]), toByte(color_[2]), toByte(color_[3])};
}

void Editor::setForeground(Rgba rgba)
{
    setFloats(color_, rgba);
}

int Editor::paletteAdd()
{
    Palette &palette = sprite().palette();
    Rgba current = foreground();
    int index = palette.find(current);
    if (index < 0) {
        index = palette.add(current);
    }
    if (index >= 0) {
        paletteSelected_ = index;
    }
    return index;
}

void Editor::palettePick(int index)
{
    const Palette &palette = sprite().palette();
    if (index < 0 || index >= palette.count()) {
        return;
    }
    paletteSelected_ = index;
    setForeground(palette.get(index));
}

void Editor::paletteRemove(int index)
{
    if (!sprite().palette().remove(index)) {
        return;
    }
    if (paletteSelected_ == index) {
        paletteSelected_ = -1;
    } else if (paletteSelected_ > index) {
        paletteSelected_--;
    }
}

void Editor::changeBrushSize(int delta)
{
    options_.brush_size = std::clamp(options_.brush_size + delta, 1, PENCIL_BRUSH_MAX);
}

void Editor::swapColors()
{
    for (int i = 0; i < 4; i++) {
        std::swap(color_[i], bgColor_[i]);
    }
}

void Editor::setActiveLayer(int index)
{
    pickLayer(index, LayerPick::Only);
}

namespace
{

int indexOfLayer(const Sprite &sprite, const Layer *layer)
{
    for (int i = 0; i < sprite.layerCount(); i++) {
        if (sprite.layer(i) == layer) {
            return i;
        }
    }
    return -1;
}

} // namespace

bool Editor::layerSelected(int index) const
{
    const Layer *layer = sprite().layer(index);
    return layer && (index == activeLayer_ ||
                     std::find(picked_.begin(), picked_.end(), layer) != picked_.end());
}

std::vector<int> Editor::selectedLayers() const
{
    std::vector<int> out;
    for (int i = 0; i < sprite().layerCount(); i++) {
        if (layerSelected(i)) {
            out.push_back(i);
        }
    }
    return out;
}

void Editor::pickLayer(int index, LayerPick mode)
{
    const Layer *layer = sprite().layer(index);
    if (!layer) {
        return;
    }
    switch (mode) {
    case LayerPick::Only:
        picked_.clear();
        activeLayer_ = index;
        anchor_ = layer;
        break;
    case LayerPick::Toggle:
        if (!layerSelected(index)) {
            /* The active layer stays selected, now as one of the picked ones. */
            picked_.push_back(sprite().layer(activeLayer_));
            activeLayer_ = index;
            anchor_ = layer;
        } else if (selectedLayers().size() > 1) {
            picked_.erase(std::remove(picked_.begin(), picked_.end(), layer), picked_.end());
            if (index == activeLayer_) {
                /* Another selected layer takes over as the active one: the topmost. */
                for (int i = sprite().layerCount() - 1; i >= 0; i--) {
                    if (i != index && layerSelected(i)) {
                        activeLayer_ = i;
                        break;
                    }
                }
                picked_.erase(
                    std::remove(picked_.begin(), picked_.end(), sprite().layer(activeLayer_)),
                    picked_.end());
            }
        }
        break;
    case LayerPick::Range: {
        int from = indexOfLayer(sprite(), anchor_);
        if (from < 0) {
            from = activeLayer_;
        }
        picked_.clear();
        for (int i = std::min(from, index); i <= std::max(from, index); i++) {
            if (i != index) {
                picked_.push_back(sprite().layer(i));
            }
        }
        activeLayer_ = index;
        break;
    }
    }
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

void Editor::drag(int cx, int cy)
{
    if (drag_) {
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
void Editor::afterHistoryStep()
{
    activeLayer_ = std::min(activeLayer_, sprite().layerCount() - 1);
    markAllDirty();
}

void Editor::undo()
{
    if (history().undo()) {
        afterHistoryStep();
    }
}

void Editor::redo()
{
    if (history().redo()) {
        afterHistoryStep();
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

void Editor::addLayer()
{
    if (stroking()) {
        return;
    }
    if (layer_op_add(history(), sprite(), "Layer " + std::to_string(sprite().layerCount() + 1))) {
        pickLayer(sprite().layerCount() - 1, LayerPick::Only);
    }
}

void Editor::removeLayer(int index)
{
    if (stroking() || !layer_op_remove(history(), sprite(), index)) {
        return;
    }
    if (activeLayer_ > index) {
        activeLayer_--;
    }
    afterHistoryStep();
}

void Editor::moveLayer(int from, int to)
{
    if (stroking() || !layer_op_move(history(), sprite(), from, to)) {
        return;
    }
    /* The active layer keeps pointing at the same layer. */
    int active = activeLayer_;
    if (active == from) {
        activeLayer_ = to;
    } else if (from < active && active <= to) {
        activeLayer_ = active - 1;
    } else if (to <= active && active < from) {
        activeLayer_ = active + 1;
    }
    markAllDirty();
}

void Editor::setLayerVisible(int index, bool visible)
{
    Layer *layer = sprite().layer(index);
    if (!stroking() && layer && layer_op_set_visible(history(), *layer, visible)) {
        markAllDirty();
    }
}

void Editor::setLayerLocked(int index, bool locked)
{
    Layer *layer = sprite().layer(index);
    if (!stroking() && layer) {
        layer_op_set_locked(history(), *layer, locked);
    }
}

void Editor::duplicateLayer(int index)
{
    if (!stroking() && layer_op_duplicate(history(), sprite(), index)) {
        pickLayer(index + 1, LayerPick::Only);
        markAllDirty();
    }
}

void Editor::mergeLayerDown(int index)
{
    if (stroking() || !layer_op_merge_down(history(), sprite(), index)) {
        return;
    }
    /* The merged layer is gone: its selection moves down, layers above it shift down by one. */
    if (activeLayer_ == index) {
        pickLayer(index - 1, LayerPick::Only);
    } else if (activeLayer_ > index) {
        activeLayer_--;
    }
    markAllDirty();
}

void Editor::removeSelectedLayers()
{
    std::vector<int> selected = selectedLayers();
    if (stroking() || static_cast<int>(selected.size()) >= sprite().layerCount()) {
        return;
    }
    history().beginGroup();
    for (auto it = selected.rbegin(); it != selected.rend(); ++it) {
        layer_op_remove(history(), sprite(), *it);
    }
    history().endGroup();
    pickLayer(std::max(selected.front() - 1, 0), LayerPick::Only);
    afterHistoryStep();
}

void Editor::duplicateSelectedLayers()
{
    if (stroking()) {
        return;
    }
    const Layer *activeCopy = nullptr;
    std::vector<const Layer *> copies;
    std::vector<int> selected = selectedLayers();
    history().beginGroup();
    /* From the top down, so the indexes still to be copied do not move. */
    for (auto it = selected.rbegin(); it != selected.rend(); ++it) {
        const Layer *copy = layer_op_duplicate(history(), sprite(), *it);
        if (copy) {
            copies.push_back(copy);
            activeCopy = *it == activeLayer_ ? copy : activeCopy;
        }
    }
    history().endGroup();
    if (copies.empty()) {
        return;
    }
    activeCopy = activeCopy ? activeCopy : copies.front();
    pickLayer(indexOfLayer(sprite(), activeCopy), LayerPick::Only);
    picked_ = copies;
    markAllDirty();
}

bool Editor::canMergeLayers() const
{
    std::vector<int> selected = selectedLayers();
    if (selected.size() == 1) {
        const Layer *upper = sprite().layer(selected.front());
        const Layer *lower = sprite().layer(selected.front() - 1);
        return upper && lower && upper->visible() && lower->visible() && !lower->locked();
    }
    for (int i : selected) {
        if (!sprite().layer(i)->visible()) {
            return false;
        }
    }
    return !sprite().layer(selected.front())->locked();
}

void Editor::mergeSelectedLayers()
{
    std::vector<int> selected = selectedLayers();
    if (stroking() || !canMergeLayers()) {
        return;
    }
    if (selected.size() == 1) {
        mergeLayerDown(selected.front());
        return;
    }
    int lowest = selected.front();
    std::vector<Layer *> layers;
    layers.reserve(selected.size());
    for (int i : selected) {
        layers.push_back(sprite().layer(i));
    }
    history().beginGroup();
    /* Bring the selected layers next to each other, just above the lowest one ... */
    for (size_t k = 1; k < layers.size(); k++) {
        int at = indexOfLayer(sprite(), layers[k]);
        int to = lowest + static_cast<int>(k);
        if (at != to) {
            layer_op_move(history(), sprite(), at, to);
        }
    }
    /* ... then fold them down one by one, topmost first. */
    for (size_t k = layers.size() - 1; k >= 1; k--) {
        layer_op_merge_down(history(), sprite(), lowest + static_cast<int>(k));
    }
    history().endGroup();
    pickLayer(lowest, LayerPick::Only);
    markAllDirty();
}

void Editor::setSelectedLayersVisible(bool visible)
{
    if (stroking()) {
        return;
    }
    history().beginGroup();
    for (int i : selectedLayers()) {
        layer_op_set_visible(history(), *sprite().layer(i), visible);
    }
    history().endGroup();
    markAllDirty();
}

void Editor::setSelectedLayersLocked(bool locked)
{
    if (stroking()) {
        return;
    }
    history().beginGroup();
    for (int i : selectedLayers()) {
        layer_op_set_locked(history(), *sprite().layer(i), locked);
    }
    history().endGroup();
}

void Editor::moveSelectedLayers(int from, int to)
{
    if (stroking() || !sprite().layer(from) || !sprite().layer(to)) {
        return;
    }
    if (!layerSelected(from) || selectedLayers().size() < 2) {
        moveLayer(from, to);
        return;
    }
    if (layerSelected(to)) {
        return;
    }
    /* The new order: the unselected layers keep theirs, the selected ones form a block next to
     * the target. */
    std::vector<Layer *> remaining;
    std::vector<Layer *> block;
    for (int i = 0; i < sprite().layerCount(); i++) {
        (layerSelected(i) ? block : remaining).push_back(sprite().layer(i));
    }
    auto target = std::find(remaining.begin(), remaining.end(), sprite().layer(to));
    remaining.insert(from < to ? target + 1 : target, block.begin(), block.end());
    Layer *active = sprite().layer(activeLayer_);
    history().beginGroup();
    for (int pos = 0; pos < static_cast<int>(remaining.size()); pos++) {
        int at = indexOfLayer(sprite(), remaining[static_cast<size_t>(pos)]);
        if (at != pos) {
            layer_op_move(history(), sprite(), at, pos);
        }
    }
    history().endGroup();
    activeLayer_ = indexOfLayer(sprite(), active);
    markAllDirty();
}

void Editor::commitLayerName(int index, const std::string &beforeName)
{
    Layer *layer = sprite().layer(index);
    if (!stroking() && layer) {
        layer_op_record_name(history(), *layer, beforeName);
    }
}

void Editor::commitLayerOpacity(int index, uint8_t beforeOpacity)
{
    Layer *layer = sprite().layer(index);
    if (!stroking() && layer) {
        layer_op_record_opacity(history(), *layer, beforeOpacity);
        markAllDirty();
    }
}

void Editor::selectAll()
{
    if (stroking()) {
        return;
    }
    hasSelection_ = true;
    selection_ = spriteBounds();
}

void Editor::clearSelection()
{
    if (!stroking()) {
        hasSelection_ = false;
    }
}

void Editor::deleteSelection()
{
    Cel *cel = editableCel();
    if (!cel || !hasSelection_) {
        return;
    }
    Rect r = selection_;
    ShapeStyle erase{ShapeKind::Rect, true, Rgba{}, Symmetry{}};
    if (shape_stroke(history(), *cel, erase, ToolPoint{r.x, r.y},
                     ToolPoint{r.x + r.w - 1, r.y + r.h - 1})) {
        markDirty(r);
    }
}

bool Editor::copySelection()
{
    Cel *cel = activeCel();
    if (stroking() || !hasSelection_ || !cel) {
        return false;
    }
    std::optional<Clip> clip = Clip::copy(*cel, selection_);
    if (!clip) {
        return false;
    }
    clipboard_ = std::move(clip);
    return true;
}

bool Editor::cutSelection()
{
    if (!editableCel() || !copySelection()) {
        return false;
    }
    deleteSelection();
    return true;
}

void Editor::paste()
{
    Cel *cel = editableCel();
    if (!cel || !clipboard_) {
        return;
    }
    const Clip &clip = *clipboard_;
    int x = 0;
    int y = 0;
    if (hoverInSprite()) {
        x = hoverX_;
        y = hoverY_;
    } else if (hasSelection_) {
        x = selection_.x;
        y = selection_.y;
    }
    if (!clip_paste(history(), *cel, clip, x, y)) {
        return;
    }
    Rect pasted = {x, y, clip.width(), clip.height()};
    markDirty(pasted);
    selectTool(ToolKind::Select);
    setSelectionClipped(pasted);
}

void Editor::setSelectionClipped(Rect r)
{
    Rect clipped = rect_clip(r, sprite().width(), sprite().height());
    hasSelection_ = !rect_is_empty(clipped);
    if (hasSelection_) {
        selection_ = clipped;
    }
}

void Editor::flipSelection(bool horizontal)
{
    Cel *cel = editableCel();
    if (!cel || !hasSelection_) {
        return;
    }
    Rect r = selection_;
    std::optional<Clip> clip = Clip::copy(*cel, r);
    if (!clip) {
        return;
    }
    if (horizontal) {
        clip->flipHorizontal();
    } else {
        clip->flipVertical();
    }
    if (clip_replace(history(), *cel, r, *clip, r.x, r.y)) {
        markDirty(r);
    }
}

void Editor::rotateSelection(bool clockwise)
{
    Cel *cel = editableCel();
    if (!cel || !hasSelection_) {
        return;
    }
    Rect r = selection_;
    std::optional<Clip> clip = Clip::copy(*cel, r);
    if (!clip) {
        return;
    }
    clip->rotate(clockwise);
    /* The turned rectangle shares the old one's center. */
    Rect turned = {r.x + (r.w - r.h) / 2, r.y + (r.h - r.w) / 2, r.h, r.w};
    if (clip_replace(history(), *cel, r, *clip, turned.x, turned.y)) {
        markDirty(r);
        markDirty(turned);
        setSelectionClipped(turned);
    }
}
