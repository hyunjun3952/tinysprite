#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "tinysprite/io.h"

namespace
{

constexpr std::array<uint8_t, 4> MAGIC = {'P', 'X', 'A', 0};

void putU16(std::vector<uint8_t> &out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void putU32(std::vector<uint8_t> &out, uint32_t value)
{
    putU16(out, value & 0xffff);
    putU16(out, value >> 16);
}

/* Appends a chunk: the tag, the length of the payload `fill` writes, then the payload. */
template <class Fill> void putChunk(std::vector<uint8_t> &out, const char tag[4], Fill fill)
{
    out.insert(out.end(), tag, tag + 4);
    size_t lengthAt = out.size();
    putU32(out, 0);
    fill(out);
    uint32_t length = static_cast<uint32_t>(out.size() - lengthAt - 4);
    for (size_t i = 0; i < 4; i++) {
        out[lengthAt + i] = static_cast<uint8_t>(length >> (8 * i));
    }
}

/* Reads little-endian values from a byte range; running past its end throws std::out_of_range,
 * which pxa_decode reports as an invalid file. */
class Reader
{
  public:
    Reader(const uint8_t *begin, const uint8_t *end) : p_(begin), end_(end)
    {
    }

    bool atEnd() const
    {
        return p_ == end_;
    }
    const uint8_t *take(size_t count)
    {
        if (count > static_cast<size_t>(end_ - p_)) {
            throw std::out_of_range("pxa: truncated");
        }
        const uint8_t *start = p_;
        p_ += count;
        return start;
    }
    uint32_t u8()
    {
        return *take(1);
    }
    uint32_t u16()
    {
        const uint8_t *b = take(2);
        return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8;
    }
    uint32_t u32()
    {
        uint32_t low = u16();
        return low | u16() << 16;
    }

  private:
    const uint8_t *p_;
    const uint8_t *end_;
};

bool isTag(const uint8_t *tag, const char *name)
{
    return std::memcmp(tag, name, 4) == 0;
}

size_t pixelBytes(const Cel &cel)
{
    return static_cast<size_t>(cel.width()) * static_cast<size_t>(cel.height()) * sizeof(Rgba);
}

/* Fills layer `index` of the sprite (adding it unless it is the first, which every sprite
 * already has) from a LAYR payload. */
void readLayer(Reader &chunk, Sprite &sprite, int index)
{
    uint32_t nameLength = chunk.u8();
    const uint8_t *nameBytes = chunk.take(nameLength);
    std::string name(nameBytes, nameBytes + nameLength);
    uint32_t flags = chunk.u8(); /* bit 0: visible, bit 1: locked */
    auto opacity = static_cast<uint8_t>(chunk.u8());

    Layer &layer = index == 0 ? *sprite.layer(0) : sprite.addLayer(name);
    layer.setName(name);
    layer.setVisible((flags & 1) != 0);
    layer.setLocked((flags & 2) != 0);
    layer.setOpacity(opacity);
    for (int frame = 0; frame < sprite.frameCount(); frame++) {
        Cel &cel = *layer.cel(frame);
        std::memcpy(cel.pixels(), chunk.take(pixelBytes(cel)), pixelBytes(cel));
    }
}

void readPalette(Reader &chunk, Palette &palette)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *c = chunk.take(4);
        palette.add(Rgba{c[0], c[1], c[2], c[3]});
    }
}

/* Grows a one-frame sprite to `frames` blank frames. The caller has checked that the file holds
 * the pixels of that many frames, so the allocation is bounded by the file's size. */
void growFrames(Sprite &sprite, uint32_t frames)
{
    while (static_cast<uint32_t>(sprite.frameCount()) < frames) {
        sprite.insertFrame(sprite.frameCount(), sprite.makeFrame());
    }
}

/* The smallest LAYR payload for a layer of `frames` frames: an empty name, the flags and opacity
 * bytes, and the pixels. */
uint64_t minLayerBytes(const Sprite &sprite, uint32_t frames)
{
    return 1 + 2 + static_cast<uint64_t>(frames) * sprite.width() * sprite.height() * sizeof(Rgba);
}

std::optional<Sprite> decode(const std::vector<uint8_t> &data)
{
    Reader file(data.data(), data.data() + data.size());
    if (std::memcmp(file.take(MAGIC.size()), MAGIC.data(), MAGIC.size()) != 0 ||
        file.u32() != PXA_VERSION) {
        return std::nullopt;
    }
    std::optional<Sprite> sprite;
    int layers = 0;
    uint32_t frames = 1;
    std::vector<uint32_t> durations; /* from FRAM, applied once the frames exist */
    while (!file.atEnd()) {
        const uint8_t *tag = file.take(4);
        uint32_t length = file.u32();
        const uint8_t *payload = file.take(length);
        Reader chunk(payload, payload + length);

        if (isTag(tag, "HEAD")) {
            uint32_t width = chunk.u32();
            uint32_t height = chunk.u32();
            frames = chunk.u32();
            /* The Sprite constructor checks the size; the frames are created with the first layer,
             * once the file is known to hold their pixels. */
            if (sprite || frames < 1 || width > CEL_MAX_DIMENSION || height > CEL_MAX_DIMENSION) {
                return std::nullopt;
            }
            sprite.emplace(static_cast<int>(width), static_cast<int>(height));
        } else if (isTag(tag, "FRAM")) {
            if (!sprite || layers > 0 || length != 2 * static_cast<uint64_t>(length / 2) ||
                length / 2 != frames) {
                return std::nullopt;
            }
            for (uint32_t i = 0; i < frames; i++) {
                uint32_t ms = chunk.u16();
                if (ms < 1) {
                    return std::nullopt;
                }
                durations.push_back(ms);
            }
        } else if (isTag(tag, "PALT") || isTag(tag, "LAYR")) {
            if (!sprite) {
                return std::nullopt;
            }
            if (isTag(tag, "PALT")) {
                readPalette(chunk, sprite->palette());
            } else {
                if (layers == 0) {
                    if (length < minLayerBytes(*sprite, frames)) {
                        return std::nullopt;
                    }
                    growFrames(*sprite, frames);
                }
                readLayer(chunk, *sprite, layers++);
            }
        }
    }
    if (layers == 0 || !sprite) {
        return std::nullopt;
    }
    for (size_t i = 0; i < durations.size(); i++) {
        sprite->setFrameDuration(static_cast<int>(i), static_cast<int>(durations[i]));
    }
    return sprite;
}

} // namespace

std::vector<uint8_t> pxa_encode(const Sprite &sprite)
{
    std::vector<uint8_t> out(MAGIC.begin(), MAGIC.end());
    putU32(out, PXA_VERSION);
    putChunk(out, "HEAD", [&](std::vector<uint8_t> &o) {
        putU32(o, static_cast<uint32_t>(sprite.width()));
        putU32(o, static_cast<uint32_t>(sprite.height()));
        putU32(o, static_cast<uint32_t>(sprite.frameCount()));
    });
    bool timed = false; /* only a sprite with non-default durations needs the chunk */
    for (int frame = 0; frame < sprite.frameCount(); frame++) {
        timed = timed || sprite.frameDuration(frame) != FRAME_DURATION_DEFAULT;
    }
    if (timed) {
        putChunk(out, "FRAM", [&](std::vector<uint8_t> &o) {
            for (int frame = 0; frame < sprite.frameCount(); frame++) {
                putU16(o, static_cast<uint32_t>(sprite.frameDuration(frame)));
            }
        });
    }
    putChunk(out, "PALT", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(sprite.palette().count()));
        for (int i = 0; i < sprite.palette().count(); i++) {
            Rgba c = sprite.palette().get(i);
            o.insert(o.end(), {c.r, c.g, c.b, c.a});
        }
    });
    for (int i = 0; i < sprite.layerCount(); i++) {
        const Layer &layer = *sprite.layer(i);
        putChunk(out, "LAYR", [&](std::vector<uint8_t> &o) {
            o.push_back(static_cast<uint8_t>(layer.name().size()));
            o.insert(o.end(), layer.name().begin(), layer.name().end());
            o.push_back(static_cast<uint8_t>((layer.visible() ? 1 : 0) | (layer.locked() ? 2 : 0)));
            o.push_back(layer.opacity());
            for (int frame = 0; frame < layer.frameCount(); frame++) {
                const Cel &cel = *layer.cel(frame);
                auto bytes = reinterpret_cast<const uint8_t *>(cel.pixels());
                o.insert(o.end(), bytes, bytes + pixelBytes(cel));
            }
        });
    }
    return out;
}

std::optional<Sprite> pxa_decode(const std::vector<uint8_t> &data)
{
    try {
        return decode(data);
    } catch (const std::out_of_range &) {
        return std::nullopt;
    } catch (const std::invalid_argument &) {
        return std::nullopt;
    }
}

bool pxa_save(const Sprite &sprite, const std::string &path)
{
    std::vector<uint8_t> data = pxa_encode(sprite);
    return file_write_atomic(path, std::string(data.begin(), data.end()));
}

std::optional<Sprite> pxa_load(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return pxa_decode(std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                           std::istreambuf_iterator<char>()));
}
