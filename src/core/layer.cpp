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

std::string layer_name_truncate(const std::string &name, size_t maxBytes)
{
    if (name.size() <= maxBytes) {
        return name;
    }
    size_t cut = maxBytes;
    /* A continuation byte (10xxxxxx) at the cut means the character started earlier. */
    while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) {
        cut--;
    }
    return name.substr(0, cut);
}

void Layer::setName(const std::string &name)
{
    name_ = layer_name_truncate(name);
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
