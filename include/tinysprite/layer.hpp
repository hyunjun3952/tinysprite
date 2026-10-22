#ifndef TINYSPRITE_LAYER_HPP
#define TINYSPRITE_LAYER_HPP

#include <memory>
#include <string>
#include <vector>

#include "tinysprite/cel.hpp"

inline constexpr int LAYER_NAME_MAX = 63;

/* How a layer's color combines with what is under it (the alpha rules stay "over"). The values are
 * stored in .pxa files, so never reorder them. */
enum class BlendMode : uint8_t {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    Add,
    Difference,
};
inline constexpr int BLEND_MODE_COUNT = 8;

/* The display name of `mode` ("Normal" for a value outside the enum). */
const char *blend_mode_name(BlendMode mode);

/* `name` cut to at most `maxBytes` bytes without splitting a UTF-8 character. */
std::string layer_name_truncate(const std::string &name, size_t maxBytes = LAYER_NAME_MAX);

class Layer
{
  public:
    /* A visible, fully opaque layer holding one blank cel per frame. Names longer than
     * LAYER_NAME_MAX are truncated. Throws std::invalid_argument for a frame count below 1 or an
     * invalid size. */
    Layer(const std::string &name, int width, int height, int frameCount);

    const std::string &name() const
    {
        return name_;
    }

    /* Names longer than LAYER_NAME_MAX are truncated. */
    void setName(const std::string &name);

    bool visible() const
    {
        return visible_;
    }
    void setVisible(bool visible)
    {
        visible_ = visible;
    }

    /* A locked layer is protected from edits made by the tools and the clipboard; the UI checks
     * it, the document model itself does not enforce it. */
    bool locked() const
    {
        return locked_;
    }
    void setLocked(bool locked)
    {
        locked_ = locked;
    }

    uint8_t opacity() const
    {
        return opacity_;
    }
    void setOpacity(uint8_t opacity)
    {
        opacity_ = opacity;
    }

    BlendMode blendMode() const
    {
        return blendMode_;
    }
    void setBlendMode(BlendMode mode)
    {
        blendMode_ = mode;
    }

    int frameCount() const
    {
        return static_cast<int>(cels_.size());
    }

    /* Returns null if frame is out of range. The address is stable for as long as the cel stays in
     * the layer (or in whoever it was handed to). */
    Cel *cel(int frame);
    const Cel *cel(int frame) const;

    /* Whether another frame of this layer shows the very same cel (a linked cel): drawing on one
     * changes them all. Frames are linked and unlinked with cel_ops.hpp. */
    bool celLinked(int frame) const;

    /* The cels of all frames, in order, and a way to replace them all at once (one per frame,
     * none null, all of the layer's size; returns false and changes nothing otherwise). Frames that
     * hold the same cel are linked. For the undoable operations, which swap cels in and out. */
    const std::vector<std::shared_ptr<Cel>> &celList() const
    {
        return cels_;
    }
    bool setCelList(std::vector<std::shared_ptr<Cel>> cels);

    /* Inserts `cel` so it ends up at `frame`, shifting the later frames up. Returns false (leaving
     * `cel` untouched) if frame is outside 0..frameCount, or the cel is null or has another size
     * than the layer's cels. Used by Sprite, which keeps every layer at the same frame count. */
    bool insertCel(int frame, std::shared_ptr<Cel> &&cel);

    /* Removes the cel at `frame` and hands it over (null if frame is out of range). */
    std::shared_ptr<Cel> detachCel(int frame);

    /* Moves the cel at `from` so it ends up at `to`. Returns false if either is out of range. */
    bool moveCel(int from, int to);

  private:
    std::string name_;
    bool visible_ = true;
    bool locked_ = false;
    uint8_t opacity_ = 255;
    BlendMode blendMode_ = BlendMode::Normal;
    std::vector<std::shared_ptr<Cel>> cels_;
};

#endif
