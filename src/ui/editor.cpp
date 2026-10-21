#include "editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>

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

Editor::Editor(int width, int height) : sprite_(width, height)
{
}

std::unique_ptr<Editor> Editor::create(SDL_Renderer *renderer, int width, int height)
{
    std::unique_ptr<Editor> editor;
    try {
        editor.reset(new Editor(width, height));
    } catch (const std::invalid_argument &) {
        return nullptr;
    }
    editor->texture_.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height));
    if (!editor->texture_) {
        return nullptr;
    }
    editor->pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Rgba{});
    SDL_SetTextureBlendMode(editor->texture(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(editor->texture(), SDL_ScaleModeNearest);

    for (const Rgba &color : DEFAULT_PALETTE) {
        editor->sprite().palette().add(color);
    }
    editor->view_.viewport = Viewport{};
    editor->markAllDirty();
    return editor;
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

void Editor::swapColors()
{
    for (int i = 0; i < 4; i++) {
        std::swap(color_[i], bgColor_[i]);
    }
}

void Editor::setActiveLayer(int index)
{
    if (sprite().layer(index)) {
        activeLayer_ = index;
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

void Editor::press(int cx, int cy)
{
    if (!stroking()) {
        drag_ = beginDrag(*this, cx, cy);
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
    }
}

void Editor::cancelDrag()
{
    if (!drag_) {
        return;
    }
    drag_->cancel();
    drag_.reset();
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
    dirtyRect_ = dirty_ ? rect_union(dirtyRect_, area) : area;
    dirty_ = true;
}

void Editor::markAllDirty()
{
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
    return stroking() || !layer || !layer->visible() ? nullptr : activeCel();
}

void Editor::addLayer()
{
    if (stroking()) {
        return;
    }
    if (layer_op_add(history(), sprite(), "Layer " + std::to_string(sprite().layerCount() + 1))) {
        activeLayer_ = sprite().layerCount() - 1;
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

void Editor::duplicateLayer(int index)
{
    if (!stroking() && layer_op_duplicate(history(), sprite(), index)) {
        activeLayer_ = index + 1;
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
        activeLayer_ = index - 1;
    } else if (activeLayer_ > index) {
        activeLayer_--;
    }
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
