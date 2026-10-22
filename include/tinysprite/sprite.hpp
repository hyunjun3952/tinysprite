#ifndef TINYSPRITE_SPRITE_HPP
#define TINYSPRITE_SPRITE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tinysprite/layer.hpp"
#include "tinysprite/palette.hpp"
#include "tinysprite/rect.hpp"

/* The order a range of frames plays in; a ping-pong turns at the ends without repeating them. */
enum class PlayMode { Forward, Reverse, PingPong };

inline constexpr int TAG_NAME_MAX = 63;   /* bytes */
inline constexpr int SLICE_NAME_MAX = 63; /* bytes */

inline constexpr int FRAME_DURATION_DEFAULT = 100; /* milliseconds */
inline constexpr int FRAME_DURATION_MAX = 65535;

class Sprite
{
  public:
    /* A named range of frames (both ends included) that plays in its own direction, such as "walk"
     * or "attack". Tags may overlap. */
    struct Tag {
        std::string name;
        int from = 0;
        int to = 0;
        PlayMode direction = PlayMode::Forward;
    };

    /* A named rectangle of the sprite (the same on every frame), such as a "hitbox" or a "head",
     * with an optional pivot given relative to the rectangle's top-left cel. Games read slices from
     * the JSON export. */
    struct Slice {
        std::string name;
        Rect rect = {0, 0, 1, 1};
        bool hasPivot = false;
        int pivotX = 0;
        int pivotY = 0;
    };

    /* One frame taken out of (or ready to go into) a sprite: a cel for every layer, bottom to top,
     * and the frame's duration. */
    struct Frame {
        std::vector<std::shared_ptr<Cel>> cels;
        int duration = FRAME_DURATION_DEFAULT;
    };

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

    /* How long `frame` is shown, in milliseconds (FRAME_DURATION_DEFAULT for a frame out of range).
     */
    int frameDuration(int frame) const;

    /* Returns false if frame is out of range or ms is outside 1..FRAME_DURATION_MAX. */
    bool setFrameDuration(int frame, int ms);

    /* The tags, in the order they were added. Frames that are inserted or removed keep them in
     * step: a tag shifts with the frames before it, grows when a frame goes inside it, shrinks when
     * one is removed from it, and disappears with its last frame. */
    const std::vector<Tag> &tags() const
    {
        return tags_;
    }

    /* Whether `tag` is a range inside the sprite's frames (from <= to). */
    bool tagValid(const Tag &tag) const;

    /* Return false (changing nothing) for a range outside 0..frameCount-1, from > to, or an index
     * out of range. Names longer than TAG_NAME_MAX bytes are cut. */
    bool addTag(Tag tag);
    bool setTag(int index, Tag tag);
    bool removeTag(int index);

    /* Replaces all the tags (an undo step restoring them); returns false and changes nothing if one
     * of them is invalid. */
    bool setTags(std::vector<Tag> tags);

    /* The slices, in the order they were added. Slices do not move with frames. */
    const std::vector<Slice> &slices() const
    {
        return slices_;
    }

    /* Whether `slice` has a name, lies fully inside the sprite and has its pivot (if any) inside
     * its own rectangle. */
    bool sliceValid(const Slice &slice) const;

    /* Return false (changing nothing) for an invalid slice or an index out of range. Names longer
     * than SLICE_NAME_MAX bytes are cut. */
    bool addSlice(Slice slice);
    bool setSlice(int index, Slice slice);
    bool removeSlice(int index);

    /* Replaces all the slices (an undo step restoring them); returns false and changes nothing if
     * one of them is invalid. */
    bool setSlices(std::vector<Slice> slices);

    /* A blank frame with the default duration, and a deep copy of `frame` (null cels if it is out
     * of range), both shaped for this sprite's layers. */
    Frame makeFrame() const;
    Frame copyFrame(int index) const;

    /* Inserts `frame` so it ends up at `index`, shifting the later frames up. Returns false (and
     * leaves `frame` untouched) if index is outside 0..frameCount, or frame has no cel per layer
     * or an invalid duration. */
    bool insertFrame(int index, Frame frame);

    /* Takes the frame at `index` out and hands it to the caller (its cels stay valid). Nothing if
     * index is out of range or it is the only frame. */
    std::optional<Frame> detachFrame(int index);

    /* Moves the frame at `from` so it ends up at `to`. Returns false if either is out of range. */
    bool moveFrame(int from, int to);

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
    std::vector<int> durations_ = {FRAME_DURATION_DEFAULT}; /* one per frame */
    Palette palette_;
    std::vector<Tag> tags_;
    std::vector<Slice> slices_;
    std::vector<std::unique_ptr<Layer>> layers_;
};

#endif
