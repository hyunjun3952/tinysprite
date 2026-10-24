#include "tinysprite/sprite.hpp"

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

bool Sprite::fitsCels(int moreLayers, int moreFrames) const
{
    /* 64-bit: 65535 layers x 65535 frames x 64 MiB still fits */
    if (frameCount_ + moreFrames > SPRITE_MAX_FRAMES) {
        return false;
    }
    const unsigned long long cels = static_cast<unsigned long long>(layers_.size() + moreLayers) *
                                    static_cast<unsigned long long>(frameCount_ + moreFrames);
    const unsigned long long cel = static_cast<unsigned long long>(width_) * height_ * sizeof(Rgba);
    return cels * cel <= celBudget_;
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

int Sprite::frameDuration(int frame) const
{
    if (frame < 0 || frame >= frameCount_) {
        return FRAME_DURATION_DEFAULT;
    }
    return durations_[static_cast<size_t>(frame)];
}

bool Sprite::setFrameDuration(int frame, int ms)
{
    if (frame < 0 || frame >= frameCount_ || ms < 1 || ms > FRAME_DURATION_MAX) {
        return false;
    }
    durations_[static_cast<size_t>(frame)] = ms;
    return true;
}

Sprite::Frame Sprite::makeFrame() const
{
    Frame frame;
    for (const auto &layer : layers_) {
        frame.cels.push_back(
            std::make_shared<Cel>(layer->cel(0)->width(), layer->cel(0)->height()));
    }
    return frame;
}

Sprite::Frame Sprite::copyFrame(int index) const
{
    Frame frame = makeFrame();
    frame.duration = frameDuration(index);
    for (size_t i = 0; i < layers_.size(); i++) {
        const Cel *from = layers_[i]->cel(index);
        if (from) {
            std::copy_n(from->pixels(), static_cast<size_t>(width_) * height_,
                        frame.cels[i]->pixels());
        }
    }
    return frame;
}

bool Sprite::insertFrame(int index, Frame &&frame)
{
    if (index < 0 || index > frameCount_ || frameCount_ >= SPRITE_MAX_FRAMES ||
        frame.cels.size() != layers_.size() || frame.duration < 1 ||
        frame.duration > FRAME_DURATION_MAX) {
        return false;
    }
    for (size_t i = 0; i < layers_.size(); i++) {
        if (!frame.cels[i] || frame.cels[i]->width() != width_ ||
            frame.cels[i]->height() != height_) {
            return false;
        }
    }
    for (size_t i = 0; i < layers_.size(); i++) {
        layers_[i]->insertCel(index, std::move(frame.cels[i]));
    }
    durations_.insert(durations_.begin() + index, frame.duration);
    frameCount_++;
    for (Tag &tag : tags_) {
        if (index <= tag.from) {
            tag.from++;
            tag.to++;
        } else if (index <= tag.to) {
            tag.to++;
        }
    }
    return true;
}

std::optional<Sprite::Frame> Sprite::detachFrame(int index)
{
    if (index < 0 || index >= frameCount_ || frameCount_ == 1) {
        return std::nullopt;
    }
    Frame frame;
    for (const auto &layer : layers_) {
        frame.cels.push_back(layer->detachCel(index));
    }
    frame.duration = durations_[static_cast<size_t>(index)];
    durations_.erase(durations_.begin() + index);
    frameCount_--;
    for (Tag &tag : tags_) {
        if (index < tag.from) {
            tag.from--;
            tag.to--;
        } else if (index <= tag.to) {
            tag.to--;
        }
    }
    /* A tag whose only frame went away is now empty (to < from). */
    tags_.erase(std::remove_if(tags_.begin(), tags_.end(),
                               [](const Tag &tag) { return tag.to < tag.from; }),
                tags_.end());
    return frame;
}

bool Sprite::moveFrame(int from, int to)
{
    if (from < 0 || from >= frameCount_ || to < 0 || to >= frameCount_) {
        return false;
    }
    for (const auto &layer : layers_) {
        layer->moveCel(from, to);
    }
    int duration = durations_[static_cast<size_t>(from)];
    durations_.erase(durations_.begin() + from);
    durations_.insert(durations_.begin() + to, duration);
    return true;
}

bool Sprite::tagValid(const Tag &tag) const
{
    return tag.from >= 0 && tag.from <= tag.to && tag.to < frameCount_;
}

bool Sprite::addTag(Tag tag)
{
    if (!tagValid(tag)) {
        return false;
    }
    tag.name = layer_name_truncate(tag.name, TAG_NAME_MAX);
    tags_.push_back(std::move(tag));
    return true;
}

bool Sprite::setTag(int index, Tag tag)
{
    if (index < 0 || index >= static_cast<int>(tags_.size()) || !tagValid(tag)) {
        return false;
    }
    tag.name = layer_name_truncate(tag.name, TAG_NAME_MAX);
    tags_[static_cast<size_t>(index)] = std::move(tag);
    return true;
}

bool Sprite::removeTag(int index)
{
    if (index < 0 || index >= static_cast<int>(tags_.size())) {
        return false;
    }
    tags_.erase(tags_.begin() + index);
    return true;
}

bool Sprite::setTags(std::vector<Tag> tags)
{
    for (const Tag &tag : tags) {
        if (!tagValid(tag)) {
            return false;
        }
    }
    tags_ = std::move(tags);
    return true;
}

bool Sprite::sliceValid(const Slice &slice) const
{
    const Rect &r = slice.rect;
    bool inside =
        r.x >= 0 && r.y >= 0 && r.w >= 1 && r.h >= 1 && r.w <= width_ - r.x && r.h <= height_ - r.y;
    /* a pivot is a point on the slice's grid, so it may sit on the far edge too */
    bool pivotInside = !slice.hasPivot || (slice.pivotX >= 0 && slice.pivotX <= r.w &&
                                           slice.pivotY >= 0 && slice.pivotY <= r.h);
    return !slice.name.empty() && inside && pivotInside;
}

bool Sprite::addSlice(Slice slice)
{
    if (!sliceValid(slice)) {
        return false;
    }
    slice.name = layer_name_truncate(slice.name, SLICE_NAME_MAX);
    slices_.push_back(std::move(slice));
    return true;
}

bool Sprite::setSlice(int index, Slice slice)
{
    if (index < 0 || index >= static_cast<int>(slices_.size()) || !sliceValid(slice)) {
        return false;
    }
    slice.name = layer_name_truncate(slice.name, SLICE_NAME_MAX);
    slices_[static_cast<size_t>(index)] = std::move(slice);
    return true;
}

bool Sprite::removeSlice(int index)
{
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return false;
    }
    slices_.erase(slices_.begin() + index);
    return true;
}

bool Sprite::setSlices(std::vector<Slice> slices)
{
    for (const Slice &slice : slices) {
        if (!sliceValid(slice)) {
            return false;
        }
    }
    slices_ = std::move(slices);
    return true;
}

bool Sprite::setTileData(std::vector<Tileset> tilesets, std::vector<TileMap> maps)
{
    if (!tile_data_valid(tilesets, maps)) {
        return false;
    }
    tilesets_ = std::move(tilesets);
    tilemaps_ = std::move(maps);
    return true;
}

bool Sprite::setTile(int map, int x, int y, uint32_t cell)
{
    if (map < 0 || map >= static_cast<int>(tilemaps_.size())) {
        return false;
    }
    TileMap &target = tilemaps_[static_cast<size_t>(map)];
    const Tileset &tileset = tilesets_[static_cast<size_t>(target.tileset)];
    if (x < 0 || y < 0 || x >= target.columns || y >= target.rows ||
        cell > static_cast<uint32_t>(tileset.tileCount())) {
        return false;
    }
    target.cells[static_cast<size_t>(y) * static_cast<size_t>(target.columns) +
                 static_cast<size_t>(x)] = cell;
    return true;
}
