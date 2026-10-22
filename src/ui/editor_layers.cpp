/* Editor: the layer stack and which layers are selected. */

#include "editor.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "editor_internal.h"

#include "tinysprite/layer_ops.h"

void Editor::setActiveLayer(int index)
{
    pickLayer(index, LayerPick::Only);
}

bool Editor::layerSelected(int index) const
{
    return layers_.contains(sprite(), index);
}

std::vector<int> Editor::selectedLayers() const
{
    return layers_.indexes(sprite());
}

void Editor::pickLayer(int index, LayerPick mode)
{
    layers_.pick(sprite(), index, mode);
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
    if (layers_.active() > index) {
        layers_.setActive(layers_.active() - 1);
    }
    afterHistoryStep();
}

void Editor::moveLayer(int from, int to)
{
    if (stroking() || !layer_op_move(history(), sprite(), from, to)) {
        return;
    }
    layers_.moved(from, to);
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
    if (layers_.active() == index) {
        pickLayer(index - 1, LayerPick::Only);
    } else if (layers_.active() > index) {
        layers_.setActive(layers_.active() - 1);
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
            activeCopy = *it == layers_.active() ? copy : activeCopy;
        }
    }
    history().endGroup();
    if (copies.empty()) {
        return;
    }
    activeCopy = activeCopy ? activeCopy : copies.front();
    pickLayer(editor_detail::indexOfLayer(sprite(), activeCopy), LayerPick::Only);
    layers_.setPicked(copies);
    markAllDirty();
}

bool Editor::canMergeLayers() const
{
    std::vector<int> selected = selectedLayers();
    if (selected.size() == 1) {
        return layer_can_merge_down(sprite(), selected.front());
    }
    /* Each selected layer is folded into the one selected below it. Once gathered, only the
     * lowest one can have nothing visible under it. */
    bool visibleBelow = false;
    for (int i = 0; i < selected.front(); i++) {
        visibleBelow = visibleBelow || sprite().layer(i)->visible();
    }
    for (size_t k = 1; k < selected.size(); k++) {
        const Layer &upper = *sprite().layer(selected[k]);
        const Layer &lower = *sprite().layer(selected[k - 1]);
        if (!layer_can_merge(upper, lower) || !layer_merge_keeps_look(upper, lower, visibleBelow)) {
            return false;
        }
        visibleBelow = true;
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

bool Editor::editSelectedLayers(const std::function<void(Layer &)> &edit)
{
    if (stroking()) {
        return false;
    }
    history().beginGroup();
    for (int i : selectedLayers()) {
        edit(*sprite().layer(i));
    }
    history().endGroup();
    return true;
}

void Editor::setSelectedLayersVisible(bool visible)
{
    if (editSelectedLayers(
            [&](Layer &layer) { layer_op_set_visible(history(), layer, visible); })) {
        markAllDirty();
    }
}

void Editor::setSelectedLayersLocked(bool locked)
{
    editSelectedLayers([&](Layer &layer) { layer_op_set_locked(history(), layer, locked); });
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
    reorderLayers(remaining);
}

/* Puts the layers in `order` (bottom first) with the fewest moves, as one undo step; the active
 * layer stays the same layer. */
void Editor::reorderLayers(const std::vector<Layer *> &order)
{
    Layer *active = sprite().layer(layers_.active());
    history().beginGroup();
    for (int pos = 0; pos < static_cast<int>(order.size()); pos++) {
        int at = editor_detail::indexOfLayer(sprite(), order[static_cast<size_t>(pos)]);
        if (at != pos) {
            layer_op_move(history(), sprite(), at, pos);
        }
    }
    history().endGroup();
    layers_.setActive(editor_detail::indexOfLayer(sprite(), active));
    markAllDirty();
}

/* The layer order after every selected layer swaps with the unselected one next to it in the given
 * direction (+1 up the stack, -1 down), and whether anything moved. Selected layers that have
 * another selected layer or the end of the stack in the way stay put, so a block keeps its shape.
 */
std::vector<Layer *> Editor::shiftedLayerOrder(int direction, bool &moved) const
{
    int count = sprite().layerCount();
    std::vector<Layer *> order;
    std::vector<unsigned char> selected; /* not vector<bool>: its elements cannot be swapped */
    for (int i = 0; i < count; i++) {
        order.push_back(const_cast<Sprite &>(sprite()).layer(i));
        selected.push_back(layerSelected(i) ? 1 : 0);
    }
    moved = false;
    for (int step = 0; step < count - 1; step++) {
        /* walk from the end the layers move towards, so a layer never moves twice */
        int i = direction > 0 ? count - 2 - step : step + 1;
        int next = i + direction;
        if (selected[static_cast<size_t>(i)] != 0 && selected[static_cast<size_t>(next)] == 0) {
            std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(next)]);
            std::swap(selected[static_cast<size_t>(i)], selected[static_cast<size_t>(next)]);
            moved = true;
        }
    }
    return order;
}

bool Editor::canMoveLayersUp() const
{
    bool moved = false;
    shiftedLayerOrder(1, moved);
    return !stroking() && moved;
}

bool Editor::canMoveLayersDown() const
{
    bool moved = false;
    shiftedLayerOrder(-1, moved);
    return !stroking() && moved;
}

void Editor::moveSelectedLayersBy(int direction)
{
    bool moved = false;
    std::vector<Layer *> order = shiftedLayerOrder(direction > 0 ? 1 : -1, moved);
    if (!stroking() && moved) {
        reorderLayers(order);
    }
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

std::vector<std::pair<int, uint8_t>> Editor::selectedLayerOpacities() const
{
    std::vector<std::pair<int, uint8_t>> out;
    for (int i : selectedLayers()) {
        out.emplace_back(i, sprite().layer(i)->opacity());
    }
    return out;
}

void Editor::setSelectedLayersOpacityLive(uint8_t opacity)
{
    if (stroking()) {
        return;
    }
    for (int i : selectedLayers()) {
        sprite().layer(i)->setOpacity(opacity);
    }
    markAllDirty();
}

void Editor::commitSelectedLayersOpacity(const std::vector<std::pair<int, uint8_t>> &before)
{
    if (stroking()) {
        return;
    }
    history().beginGroup();
    for (const auto &entry : before) {
        if (Layer *layer = sprite().layer(entry.first)) {
            layer_op_record_opacity(history(), *layer, entry.second);
        }
    }
    history().endGroup();
    markAllDirty();
}

void Editor::setSelectedLayersBlendMode(BlendMode mode)
{
    if (stroking()) {
        return;
    }
    history().beginGroup();
    for (int i : selectedLayers()) {
        layer_op_set_blend_mode(history(), *sprite().layer(i), mode);
    }
    history().endGroup();
    markAllDirty();
}
