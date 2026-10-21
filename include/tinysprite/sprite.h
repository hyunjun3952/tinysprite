#ifndef TINYSPRITE_SPRITE_H
#define TINYSPRITE_SPRITE_H

#include <memory>
#include <vector>

#include "tinysprite/layer.h"
#include "tinysprite/palette.h"

class Sprite
{
  public:
    /* One frame, an empty palette and one layer named "Layer 1". Throws std::invalid_argument if a
     * dimension is outside 1..CEL_MAX_DIMENSION. */
    Sprite(int width, int height);

    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }
    int frameCount() const
    {
        return frameCount_;
    }

    Palette &palette()
    {
        return palette_;
    }
    const Palette &palette() const
    {
        return palette_;
    }

    int layerCount() const
    {
        return static_cast<int>(layers_.size());
    }

    /* Layers are ordered bottom (index 0) to top. Returns null if index is out of range. */
    Layer *layer(int index);
    const Layer *layer(int index) const;

    /* Appends a new layer on top and returns it. */
    Layer &addLayer(const std::string &name);

    /* Destroys the layer at `index`; the layers above it shift down. A sprite always keeps at
     * least one layer. Returns false if index is out of range or only one layer is left. The
     * layer's cels are freed, so callers must drop them from any history first
     * (History::forgetCel). */
    bool removeLayer(int index);

    /* Like removeLayer, but hands the layer to the caller instead of destroying it (its cels stay
     * valid). Returns null under the same conditions removeLayer fails. */
    std::unique_ptr<Layer> detachLayer(int index);

    /* Inserts a layer (made for this sprite's size and frame count) so it ends up at `index`,
     * shifting the layers from there up. The sprite takes ownership. Returns false (and leaves
     * `layer` untouched) if index is outside 0..layerCount or layer is null. */
    bool insertLayer(int index, std::unique_ptr<Layer> &&layer);

    /* Moves the layer at `from` so it ends up at index `to`, shifting the layers in between.
     * Returns false if either index is out of range. */
    bool moveLayer(int from, int to);

  private:
    int width_;
    int height_;
    int frameCount_ = 1;
    Palette palette_;
    std::vector<std::unique_ptr<Layer>> layers_;
};

#endif
