#include "editor/editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

#include "editor/drag.h"
#include "editor/tools.h"

#include "tinysprite/layer_ops.h"
#include "tinysprite/util.h"

namespace
{

uint8_t toByte(float value)
{
    return static_cast<uint8_t>(clamp_int(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
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

std::unique_ptr<Editor> Editor::create(SDL_Renderer *renderer, int width, int height)
{
    std::unique_ptr<Editor> editor(new Editor);
    editor->sprite_.reset(sprite_create(width, height));
    editor->history_.reset(history_create());
    editor->texture_.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height));
    if (!editor->sprite_ || !editor->history_ || !editor->texture_) {
        return nullptr;
    }
    editor->pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Rgba{});
    SDL_SetTextureBlendMode(editor->texture(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(editor->texture(), SDL_ScaleModeNearest);

    for (const Rgba &color : DEFAULT_PALETTE) {
        (void)palette_add(sprite_palette(editor->sprite()), color);
    }
    editor->view_.viewport = viewport_default();
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
    Palette *palette = sprite_palette(sprite());
    Rgba current = foreground();
    int index = palette_find(palette, current);
    if (index < 0) {
        index = palette_add(palette, current);
    }
    if (index >= 0) {
        paletteSelected_ = index;
    }
    return index;
}

void Editor::palettePick(int index)
{
    Palette *palette = sprite_palette(sprite());
    if (index < 0 || index >= palette_count(palette)) {
        return;
    }
    paletteSelected_ = index;
    setForeground(palette_get(palette, index));
}

void Editor::paletteRemove(int index)
{
    if (palette_remove(sprite_palette(sprite()), index) != 0) {
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
    if (sprite_layer(sprite(), index)) {
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
    if (drag_ && !drag_->update(cx, cy)) {
        cancelDrag();
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
    return Rect{0, 0, sprite_width(sprite()), sprite_height(sprite())};
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
    int width = sprite_width(sprite());
    Rect r = rect_clip(dirtyRect_, width, sprite_height(sprite()));
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
    float zoom = viewport_next_zoom(view_.viewport.zoom, direction);
    viewport_zoom_at(&view_.viewport, zoom, view_.canvas_w / 2.0f, view_.canvas_h / 2.0f);
    view_.auto_fit = false;
}

/* Undoing / redoing a layer operation can change the layer count under the active index. */
void Editor::afterHistoryStep()
{
    activeLayer_ = std::min(activeLayer_, sprite_layer_count(sprite()) - 1);
    markAllDirty();
}

void Editor::undo()
{
    if (history_undo(history()) == 0) {
        afterHistoryStep();
    }
}

void Editor::redo()
{
    if (history_redo(history()) == 0) {
        afterHistoryStep();
    }
}

bool Editor::hoverInSprite() const
{
    return hovering_ && hoverX_ >= 0 && hoverY_ >= 0 && hoverX_ < sprite_width(sprite()) &&
           hoverY_ < sprite_height(sprite());
}

Cel *Editor::activeCel()
{
    Layer *layer = sprite_layer(sprite(), activeLayer_);
    return layer ? layer_cel(layer, frame_) : nullptr;
}

Cel *Editor::editableCel()
{
    Layer *layer = sprite_layer(sprite(), activeLayer_);
    return stroking() || !layer || !layer_visible(layer) ? nullptr : activeCel();
}

void Editor::addLayer()
{
    if (stroking()) {
        return;
    }
    char name[LAYER_NAME_MAX + 1];
    std::snprintf(name, sizeof(name), "Layer %d", sprite_layer_count(sprite()) + 1);
    if (layer_op_add(history(), sprite(), name)) {
        activeLayer_ = sprite_layer_count(sprite()) - 1;
    }
}

void Editor::removeLayer(int index)
{
    if (stroking() || layer_op_remove(history(), sprite(), index) != 0) {
        return;
    }
    if (activeLayer_ > index) {
        activeLayer_--;
    }
    afterHistoryStep();
}

void Editor::moveLayer(int from, int to)
{
    if (stroking() || layer_op_move(history(), sprite(), from, to) != 0) {
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
    Layer *layer = sprite_layer(sprite(), index);
    if (!stroking() && layer && layer_op_set_visible(history(), layer, visible) == 0) {
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
    if (stroking() || layer_op_merge_down(history(), sprite(), index) != 0) {
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

void Editor::commitLayerName(int index, const char *beforeName)
{
    Layer *layer = sprite_layer(sprite(), index);
    if (!stroking() && layer) {
        (void)layer_op_record_name(history(), layer, beforeName);
    }
}

void Editor::commitLayerOpacity(int index, uint8_t beforeOpacity)
{
    Layer *layer = sprite_layer(sprite(), index);
    if (!stroking() && layer) {
        (void)layer_op_record_opacity(history(), layer, beforeOpacity);
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
    ShapeStyle erase = {SHAPE_RECT, true, Rgba{0, 0, 0, 0}, Symmetry{}};
    if (shape_stroke(history(), cel, erase, ToolPoint{r.x, r.y},
                     ToolPoint{r.x + r.w - 1, r.y + r.h - 1}) == 0) {
        markDirty(r);
    }
}

bool Editor::copySelection()
{
    Cel *cel = activeCel();
    if (stroking() || !hasSelection_ || !cel) {
        return false;
    }
    Clip *clip = clip_copy(cel, selection_);
    if (!clip) {
        return false;
    }
    clipboard_.reset(clip);
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
    int x = 0;
    int y = 0;
    if (hoverInSprite()) {
        x = hoverX_;
        y = hoverY_;
    } else if (hasSelection_) {
        x = selection_.x;
        y = selection_.y;
    }
    if (clip_paste(history(), cel, clipboard_.get(), x, y) != 0) {
        return;
    }
    Rect pasted = {x, y, clip_width(clipboard_.get()), clip_height(clipboard_.get())};
    markDirty(pasted);
    selectTool(ToolKind::Select);
    setSelectionClipped(pasted);
}

void Editor::setSelectionClipped(Rect r)
{
    Rect clipped = rect_clip(r, sprite_width(sprite()), sprite_height(sprite()));
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
    std::unique_ptr<Clip, Deleter<clip_destroy>> clip(clip_copy(cel, r));
    if (!clip) {
        return;
    }
    if (horizontal) {
        clip_flip_horizontal(clip.get());
    } else {
        clip_flip_vertical(clip.get());
    }
    if (clip_replace(history(), cel, r, clip.get(), r.x, r.y) == 0) {
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
    std::unique_ptr<Clip, Deleter<clip_destroy>> clip(clip_copy(cel, r));
    if (!clip || clip_rotate(clip.get(), clockwise) != 0) {
        return;
    }
    /* The turned rectangle shares the old one's center. */
    Rect turned = {r.x + (r.w - r.h) / 2, r.y + (r.h - r.w) / 2, r.h, r.w};
    if (clip_replace(history(), cel, r, clip.get(), turned.x, turned.y) == 0) {
        markDirty(r);
        markDirty(turned);
        setSelectionClipped(turned);
    }
}
