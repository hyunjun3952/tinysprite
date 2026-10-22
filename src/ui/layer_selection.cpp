#include "layer_selection.h"

#include <algorithm>

#include "editor_internal.h"

bool LayerSelection::contains(const Sprite &sprite, int index) const
{
    const Layer *layer = sprite.layer(index);
    return layer != nullptr &&
           (index == active_ || std::find(picked_.begin(), picked_.end(), layer) != picked_.end());
}

std::vector<int> LayerSelection::indexes(const Sprite &sprite) const
{
    std::vector<int> out;
    for (int i = 0; i < sprite.layerCount(); i++) {
        if (contains(sprite, i)) {
            out.push_back(i);
        }
    }
    return out;
}

/* Ctrl+click: adds the layer to the selection, or removes it (never the last selected one). */
void LayerSelection::toggle(const Sprite &sprite, int index)
{
    const Layer *layer = sprite.layer(index);
    if (!contains(sprite, index)) {
        /* The active layer stays selected, now as one of the picked ones. */
        picked_.push_back(sprite.layer(active_));
        active_ = index;
        anchor_ = layer;
        return;
    }
    if (indexes(sprite).size() <= 1) {
        return;
    }
    picked_.erase(std::remove(picked_.begin(), picked_.end(), layer), picked_.end());
    if (index != active_) {
        return;
    }
    /* Another selected layer takes over as the active one: the topmost. */
    for (int i = sprite.layerCount() - 1; i >= 0; i--) {
        if (i != index && contains(sprite, i)) {
            active_ = i;
            break;
        }
    }
    picked_.erase(std::remove(picked_.begin(), picked_.end(), sprite.layer(active_)),
                  picked_.end());
}

/* Shift+click: selects everything between the anchor and the layer. */
void LayerSelection::range(const Sprite &sprite, int index)
{
    int from = editor_detail::indexOfLayer(sprite, anchor_);
    if (from < 0) {
        from = active_;
    }
    picked_.clear();
    for (int i = std::min(from, index); i <= std::max(from, index); i++) {
        if (i != index) {
            picked_.push_back(sprite.layer(i));
        }
    }
    active_ = index;
}

void LayerSelection::pick(const Sprite &sprite, int index, LayerPick mode)
{
    const Layer *layer = sprite.layer(index);
    if (!layer) {
        return;
    }
    switch (mode) {
    case LayerPick::Only:
        picked_.clear();
        active_ = index;
        anchor_ = layer;
        break;
    case LayerPick::Toggle:
        toggle(sprite, index);
        break;
    case LayerPick::Range:
        range(sprite, index);
        break;
    }
}

void LayerSelection::moved(int from, int to)
{
    if (active_ == from) {
        active_ = to;
    } else if (from < active_ && active_ <= to) {
        active_--;
    } else if (to <= active_ && active_ < from) {
        active_++;
    }
}

void LayerSelection::refresh(const Sprite &sprite, const Layer *active)
{
    /* Keep pointing at the same layer when it is still there. */
    int found = active ? editor_detail::indexOfLayer(sprite, active) : -1;
    active_ = found >= 0 ? found : std::min(active_, sprite.layerCount() - 1);
    /* Layers are tracked by address: forget the ones that left the sprite, so a new layer that
     * reuses an address is not taken for a selected one. */
    picked_.erase(std::remove_if(picked_.begin(), picked_.end(),
                                 [&sprite](const Layer *l) {
                                     return editor_detail::indexOfLayer(sprite, l) < 0;
                                 }),
                  picked_.end());
    if (anchor_ && editor_detail::indexOfLayer(sprite, anchor_) < 0) {
        anchor_ = nullptr;
    }
}

void LayerSelection::reset()
{
    active_ = 0;
    picked_.clear();
    anchor_ = nullptr;
}
