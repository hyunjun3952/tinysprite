/* Editor: the layer stack and which layers are selected. */

#include "editor.h"

#include <algorithm>
#include <string>

#include "editor_internal.h"

#include "tinysprite/layer_ops.h"

namespace editor_detail
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

} // namespace editor_detail

void Editor::setActiveLayer(int index)
{
    pickLayer(index, LayerPick::Only);
}

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

/* Ctrl+click: adds the layer to the selection, or removes it (never the last selected one). */
void Editor::toggleLayerPick(int index)
{
    const Layer *layer = sprite().layer(index);
    if (!layerSelected(index)) {
        /* The active layer stays selected, now as one of the picked ones. */
        picked_.push_back(sprite().layer(activeLayer_));
        activeLayer_ = index;
        anchor_ = layer;
        return;
    }
    if (selectedLayers().size() <= 1) {
        return;
    }
    picked_.erase(std::remove(picked_.begin(), picked_.end(), layer), picked_.end());
    if (index != activeLayer_) {
        return;
    }
    /* Another selected layer takes over as the active one: the topmost. */
    for (int i = sprite().layerCount() - 1; i >= 0; i--) {
        if (i != index && layerSelected(i)) {
            activeLayer_ = i;
            break;
        }
    }
    picked_.erase(std::remove(picked_.begin(), picked_.end(), sprite().layer(activeLayer_)),
                  picked_.end());
}

/* Shift+click: selects everything between the anchor and the layer. */
void Editor::rangeLayerPick(int index)
{
    int from = editor_detail::indexOfLayer(sprite(), anchor_);
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
        toggleLayerPick(index);
        break;
    case LayerPick::Range:
        rangeLayerPick(index);
        break;
    }
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
    pickLayer(editor_detail::indexOfLayer(sprite(), activeCopy), LayerPick::Only);
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
    /* Every layer but the topmost receives the one above it, so none of those may be locked. */
    for (size_t k = 0; k < selected.size(); k++) {
        const Layer *layer = sprite().layer(selected[k]);
        if (!layer->visible() || (k + 1 < selected.size() && layer->locked())) {
            return false;
        }
    }
    return true;
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
        int at = editor_detail::indexOfLayer(sprite(), layers[k]);
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
        int at = editor_detail::indexOfLayer(sprite(), remaining[static_cast<size_t>(pos)]);
        if (at != pos) {
            layer_op_move(history(), sprite(), at, pos);
        }
    }
    history().endGroup();
    activeLayer_ = editor_detail::indexOfLayer(sprite(), active);
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
