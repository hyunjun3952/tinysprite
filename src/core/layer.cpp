#include "tinysprite/layer.h"

#include <stdexcept>
#include <utility>

Layer::Layer(const std::string &name, int width, int height, int frameCount)
{
    if (frameCount < 1) {
        throw std::invalid_argument("layer needs at least one frame");
    }
    setName(name);
    for (int i = 0; i < frameCount; i++) {
        cels_.push_back(std::make_unique<Cel>(width, height));
    }
}

void Layer::setName(const std::string &name)
{
    name_ = name.substr(0, LAYER_NAME_MAX);
}

Cel *Layer::cel(int frame)
{
    return const_cast<Cel *>(std::as_const(*this).cel(frame));
}

const Cel *Layer::cel(int frame) const
{
    if (frame < 0 || frame >= frameCount()) {
        return nullptr;
    }
    return cels_[static_cast<size_t>(frame)].get();
}
