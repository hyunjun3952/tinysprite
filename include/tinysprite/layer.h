#ifndef TINYSPRITE_LAYER_H
#define TINYSPRITE_LAYER_H

#include <memory>
#include <string>
#include <vector>

#include "tinysprite/cel.h"

inline constexpr int LAYER_NAME_MAX = 63;

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

    int frameCount() const
    {
        return static_cast<int>(cels_.size());
    }

    /* Returns null if frame is out of range. The address is stable for the layer's lifetime. */
    Cel *cel(int frame);
    const Cel *cel(int frame) const;

  private:
    std::string name_;
    bool visible_ = true;
    bool locked_ = false;
    uint8_t opacity_ = 255;
    std::vector<std::unique_ptr<Cel>> cels_;
};

#endif
