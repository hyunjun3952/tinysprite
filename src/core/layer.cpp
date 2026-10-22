#include "tinysprite/layer.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

Layer::Layer(const std::string &name, int width, int height, int frameCount)
{
    if (frameCount < 1) {
        throw std::invalid_argument("layer needs at least one frame");
    }
    setName(name);
    for (int i = 0; i < frameCount; i++) {
        cels_.push_back(std::make_shared<Cel>(width, height));
    }
}

const char *blend_mode_name(BlendMode mode)
{
    static const char *const names[BLEND_MODE_COUNT] = {
        "Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Add", "Difference"};
    auto i = static_cast<size_t>(mode);
    return i < BLEND_MODE_COUNT ? names[i] : names[0];
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

bool Layer::insertCel(int frame, std::shared_ptr<Cel> &&cel)
{
    if (!cel || cels_.empty() || frame < 0 || frame > frameCount() ||
        cel->width() != cels_[0]->width() || cel->height() != cels_[0]->height()) {
        return false;
    }
    cels_.insert(cels_.begin() + frame, std::move(cel));
    return true;
}

std::shared_ptr<Cel> Layer::detachCel(int frame)
{
    if (frame < 0 || frame >= frameCount()) {
        return nullptr;
    }
    auto it = cels_.begin() + frame;
    std::shared_ptr<Cel> cel = std::move(*it);
    cels_.erase(it);
    return cel;
}

bool Layer::moveCel(int from, int to)
{
    if (from < 0 || from >= frameCount() || to < 0 || to >= frameCount()) {
        return false;
    }
    auto first = cels_.begin();
    if (from < to) {
        std::rotate(first + from, first + from + 1, first + to + 1);
    } else {
        std::rotate(first + to, first + from, first + from + 1);
    }
    return true;
}

bool Layer::celLinked(int frame) const
{
    const Cel *cel = this->cel(frame);
    return cel != nullptr &&
           std::count_if(cels_.begin(), cels_.end(),
                         [cel](const std::shared_ptr<Cel> &c) { return c.get() == cel; }) > 1;
}

bool Layer::setCelList(std::vector<std::shared_ptr<Cel>> cels)
{
    if (cels.size() != cels_.size()) {
        return false;
    }
    for (const std::shared_ptr<Cel> &cel : cels) {
        if (!cel || cel->width() != cels_[0]->width() || cel->height() != cels_[0]->height()) {
            return false;
        }
    }
    cels_ = std::move(cels);
    return true;
}
