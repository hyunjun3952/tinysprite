#include "tinysprite/sprite.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

Sprite::Sprite(int width, int height) : width_(width), height_(height)
{
    if (width < 1 || width > CEL_MAX_DIMENSION || height < 1 || height > CEL_MAX_DIMENSION) {
        throw std::invalid_argument("sprite dimension out of range");
    }
    addLayer("Layer 1");
}

Layer *Sprite::layer(int index)
{
    return const_cast<Layer *>(std::as_const(*this).layer(index));
}

const Layer *Sprite::layer(int index) const
{
    if (index < 0 || index >= layerCount()) {
        return nullptr;
    }
    return layers_[static_cast<size_t>(index)].get();
}

Layer &Sprite::addLayer(const std::string &name)
{
    layers_.push_back(std::make_unique<Layer>(name, width_, height_, frameCount_));
    return *layers_.back();
}

std::unique_ptr<Layer> Sprite::detachLayer(int index)
{
    if (index < 0 || index >= layerCount() || layerCount() == 1) {
        return nullptr;
    }
    auto it = layers_.begin() + index;
    std::unique_ptr<Layer> layer = std::move(*it);
    layers_.erase(it);
    return layer;
}

bool Sprite::insertLayer(int index, std::unique_ptr<Layer> &&layer)
{
    if (!layer || index < 0 || index > layerCount()) {
        return false;
    }
    layers_.insert(layers_.begin() + index, std::move(layer));
    return true;
}

bool Sprite::removeLayer(int index)
{
    return detachLayer(index) != nullptr;
}

bool Sprite::moveLayer(int from, int to)
{
    if (from < 0 || from >= layerCount() || to < 0 || to >= layerCount()) {
        return false;
    }
    auto first = layers_.begin();
    if (from < to) {
        std::rotate(first + from, first + from + 1, first + to + 1);
    } else {
        std::rotate(first + to, first + from, first + from + 1);
    }
    return true;
}
