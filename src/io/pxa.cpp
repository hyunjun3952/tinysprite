#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
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

/* The layer at a file's layer index, or null if the sprite has no such layer. */
Layer *layerAt(Sprite &sprite, uint32_t index)
{
    return index < static_cast<uint32_t>(sprite.layerCount())
               ? sprite.layer(static_cast<int>(index))
               : nullptr;
}

/* One frame of a layer that shows the same cel as an earlier frame. */
struct CelLink {
    uint32_t layer;
    uint32_t frame;
    uint32_t source;
};

/* A LINK payload: a layer, then how many (frame, source) pairs follow. */
void readLinks(Reader &chunk, std::vector<CelLink> &links)
{
    uint32_t layer = chunk.u32();
    uint32_t count = chunk.u32();
    for (uint32_t i = 0; i < count; i++) {
        uint32_t frame = chunk.u32();
        uint32_t source = chunk.u32();
        links.push_back(CelLink{layer, frame, source});
    }
}

/* Makes the linked frames share their cels; false if a link points outside the sprite or forward.
 */
bool applyLinks(Sprite &sprite, const std::vector<CelLink> &links)
{
    for (const CelLink &link : links) {
        Layer *layer = layerAt(sprite, link.layer);
        if (!layer || link.frame >= static_cast<uint32_t>(layer->frameCount()) ||
            link.source >= link.frame) {
            return false;
        }
    }
    for (size_t i = 0; i < links.size();) {
        /* all the consecutive pairs of one layer take a single swap of its cel list */
        Layer &layer = *sprite.layer(static_cast<int>(links[i].layer));
        std::vector<std::shared_ptr<Cel>> cels = layer.celList();
        size_t j = i;
        for (; j < links.size() && links[j].layer == links[i].layer; j++) {
            cels[links[j].frame] = cels[links[j].source];
        }
        layer.setCelList(std::move(cels));
        i = j;
    }
    return true;
}

/* A layer's blend mode from a BLND chunk. */
struct LayerBlend {
    uint32_t layer;
    BlendMode mode;
};

/* A BLND payload: the count, then per layer that is not Normal its index and mode byte. A mode
 * this version does not know reads as Normal. */
void readBlends(Reader &chunk, std::vector<LayerBlend> &blends)
{
    uint32_t count = chunk.u32();
    for (uint32_t i = 0; i < count; i++) {
        uint32_t layer = chunk.u32();
        uint32_t mode = chunk.u8();
        blends.push_back(LayerBlend{layer, mode < BLEND_MODE_COUNT ? static_cast<BlendMode>(mode)
                                                                   : BlendMode::Normal});
    }
}

/* False if an entry names a layer the sprite does not have. */
bool applyBlends(Sprite &sprite, const std::vector<LayerBlend> &blends)
{
    for (const LayerBlend &blend : blends) {
        Layer *layer = layerAt(sprite, blend.layer);
        if (!layer) {
            return false;
        }
        layer->setBlendMode(blend.mode);
    }
    return true;
}

/* A TAGS payload: the count, then per tag its name, first and last frame and direction. */
void readTags(Reader &chunk, std::vector<Sprite::Tag> &tags)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        Sprite::Tag tag;
        uint32_t nameLength = chunk.u8();
        const uint8_t *name = chunk.take(nameLength);
        tag.name.assign(name, name + nameLength);
        uint32_t from = chunk.u32();
        uint32_t to = chunk.u32();
        uint32_t direction = chunk.u8();
        if (from > INT32_MAX || to > INT32_MAX ||
            direction > static_cast<uint32_t>(PlayMode::PingPong) || nameLength > TAG_NAME_MAX) {
            throw std::out_of_range("pxa: bad tag");
        }
        tag.from = static_cast<int>(from);
        tag.to = static_cast<int>(to);
        tag.direction = static_cast<PlayMode>(direction);
        tags.push_back(std::move(tag));
    }
}

/* What the chunks read so far add up to. The chunks that fill the sprite (FRAM, LINK, BLND, TAGS)
 * are kept aside and applied in finish(), once the frames and layers they refer to exist. */
class Decoder
{
  public:
    /* Reads one chunk; false if the file is invalid. Chunks this version does not know are skipped.
     */
    bool chunk(const uint8_t *tag, Reader &payload, uint32_t length);

    std::optional<Sprite> finish()
    {
        if (layers_ == 0 || !sprite_) {
            return std::nullopt;
        }
        for (size_t i = 0; i < durations_.size(); i++) {
            sprite_->setFrameDuration(static_cast<int>(i), static_cast<int>(durations_[i]));
        }
        if (!applyLinks(*sprite_, links_) || !applyBlends(*sprite_, blends_) ||
            !sprite_->setTags(std::move(tags_))) {
            return std::nullopt;
        }
        return std::move(sprite_);
    }

  private:
    using Read = bool (Decoder::*)(Reader &chunk, uint32_t length);
    struct ChunkReader {
        const char *tag;
        Read read;
    };
    static const ChunkReader CHUNKS[];

    bool readHead(Reader &chunk, uint32_t)
    {
        uint32_t width = chunk.u32();
        uint32_t height = chunk.u32();
        frames_ = chunk.u32();
        /* The Sprite constructor checks the size; the frames are created with the first layer,
         * once the file is known to hold their pixels. */
        if (sprite_ || frames_ < 1 || width > CEL_MAX_DIMENSION || height > CEL_MAX_DIMENSION) {
            return false;
        }
        sprite_.emplace(static_cast<int>(width), static_cast<int>(height));
        return true;
    }

    bool readDurations(Reader &chunk, uint32_t length)
    {
        if (!sprite_ || layers_ > 0 || length % 2 != 0 || length / 2 != frames_) {
            return false;
        }
        for (uint32_t i = 0; i < frames_; i++) {
            uint32_t ms = chunk.u16();
            if (ms < 1) {
                return false;
            }
            durations_.push_back(ms);
        }
        return true;
    }

    bool readPaletteChunk(Reader &chunk, uint32_t)
    {
        if (!sprite_) {
            return false;
        }
        readPalette(chunk, sprite_->palette());
        return true;
    }

    bool readLayerChunk(Reader &chunk, uint32_t length)
    {
        if (!sprite_) {
            return false;
        }
        if (layers_ == 0) {
            if (length < minLayerBytes(*sprite_, frames_)) {
                return false;
            }
            growFrames(*sprite_, frames_);
        }
        readLayer(chunk, *sprite_, layers_++);
        return true;
    }

    bool readLinkChunk(Reader &chunk, uint32_t)
    {
        readLinks(chunk, links_);
        return true;
    }

    bool readBlendChunk(Reader &chunk, uint32_t)
    {
        readBlends(chunk, blends_);
        return true;
    }

    bool readTagChunk(Reader &chunk, uint32_t)
    {
        readTags(chunk, tags_);
        return true;
    }

    std::optional<Sprite> sprite_;
    int layers_ = 0;
    uint32_t frames_ = 1;
    std::vector<uint32_t> durations_; /* from FRAM, applied once the frames exist */
    std::vector<Sprite::Tag> tags_;   /* from TAGS, checked against the frames at the end */
    std::vector<CelLink> links_;      /* from LINK, applied once the layers exist */
    std::vector<LayerBlend> blends_;  /* from BLND, applied once the layers exist */
};

const Decoder::ChunkReader Decoder::CHUNKS[] = {
    {"HEAD", &Decoder::readHead},         {"FRAM", &Decoder::readDurations},
    {"PALT", &Decoder::readPaletteChunk}, {"LAYR", &Decoder::readLayerChunk},
    {"LINK", &Decoder::readLinkChunk},    {"BLND", &Decoder::readBlendChunk},
    {"TAGS", &Decoder::readTagChunk},
};

bool Decoder::chunk(const uint8_t *tag, Reader &payload, uint32_t length)
{
    for (const ChunkReader &known : CHUNKS) {
        if (isTag(tag, known.tag)) {
            return (this->*known.read)(payload, length);
        }
    }
    return true;
}

std::optional<Sprite> decode(const std::vector<uint8_t> &data)
{
    Reader file(data.data(), data.data() + data.size());
    if (std::memcmp(file.take(MAGIC.size()), MAGIC.data(), MAGIC.size()) != 0 ||
        file.u32() != PXA_VERSION) {
        return std::nullopt;
    }
    Decoder decoder;
    while (!file.atEnd()) {
        const uint8_t *tag = file.take(4);
        uint32_t length = file.u32();
        const uint8_t *payload = file.take(length);
        Reader chunk(payload, payload + length);
        if (!decoder.chunk(tag, chunk, length)) {
            return std::nullopt;
        }
    }
    return decoder.finish();
}

void writeHead(std::vector<uint8_t> &out, const Sprite &sprite)
{
    putChunk(out, "HEAD", [&](std::vector<uint8_t> &o) {
        putU32(o, static_cast<uint32_t>(sprite.width()));
        putU32(o, static_cast<uint32_t>(sprite.height()));
        putU32(o, static_cast<uint32_t>(sprite.frameCount()));
    });
}

/* Only a sprite with non-default durations needs the chunk. */
void writeDurations(std::vector<uint8_t> &out, const Sprite &sprite)
{
    bool timed = false;
    for (int frame = 0; frame < sprite.frameCount(); frame++) {
        timed = timed || sprite.frameDuration(frame) != FRAME_DURATION_DEFAULT;
    }
    if (!timed) {
        return;
    }
    putChunk(out, "FRAM", [&](std::vector<uint8_t> &o) {
        for (int frame = 0; frame < sprite.frameCount(); frame++) {
            putU16(o, static_cast<uint32_t>(sprite.frameDuration(frame)));
        }
    });
}

void writePalette(std::vector<uint8_t> &out, const Palette &palette)
{
    putChunk(out, "PALT", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(palette.count()));
        for (int i = 0; i < palette.count(); i++) {
            Rgba c = palette.get(i);
            o.insert(o.end(), {c.r, c.g, c.b, c.a});
        }
    });
}

void writeLayer(std::vector<uint8_t> &out, const Layer &layer)
{
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

/* Each frame of the layer that shows a cel of an earlier frame, with the first frame that shows
 * it. */
std::vector<std::pair<uint32_t, uint32_t>> celLinks(const Layer &layer)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    for (int frame = 1; frame < layer.frameCount(); frame++) {
        int first = 0;
        while (layer.cel(first) != layer.cel(frame)) {
            first++;
        }
        if (first < frame) {
            pairs.emplace_back(static_cast<uint32_t>(frame), static_cast<uint32_t>(first));
        }
    }
    return pairs;
}

void writeLinks(std::vector<uint8_t> &out, const Sprite &sprite)
{
    for (int i = 0; i < sprite.layerCount(); i++) {
        auto pairs = celLinks(*sprite.layer(i));
        if (pairs.empty()) {
            continue;
        }
        putChunk(out, "LINK", [&](std::vector<uint8_t> &o) {
            putU32(o, static_cast<uint32_t>(i));
            putU32(o, static_cast<uint32_t>(pairs.size()));
            for (const auto &pair : pairs) {
                putU32(o, pair.first);
                putU32(o, pair.second);
            }
        });
    }
}

/* Only the layers that are not Normal are listed. */
void writeBlends(std::vector<uint8_t> &out, const Sprite &sprite)
{
    std::vector<std::pair<uint32_t, uint8_t>> blends;
    for (int i = 0; i < sprite.layerCount(); i++) {
        BlendMode mode = sprite.layer(i)->blendMode();
        if (mode != BlendMode::Normal) {
            blends.emplace_back(static_cast<uint32_t>(i), static_cast<uint8_t>(mode));
        }
    }
    if (blends.empty()) {
        return;
    }
    putChunk(out, "BLND", [&](std::vector<uint8_t> &o) {
        putU32(o, static_cast<uint32_t>(blends.size()));
        for (const auto &entry : blends) {
            putU32(o, entry.first);
            o.push_back(entry.second);
        }
    });
}

void writeTags(std::vector<uint8_t> &out, const std::vector<Sprite::Tag> &tags)
{
    if (tags.empty()) {
        return;
    }
    putChunk(out, "TAGS", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(tags.size()));
        for (const Sprite::Tag &tag : tags) {
            o.push_back(static_cast<uint8_t>(tag.name.size()));
            o.insert(o.end(), tag.name.begin(), tag.name.end());
            putU32(o, static_cast<uint32_t>(tag.from));
            putU32(o, static_cast<uint32_t>(tag.to));
            o.push_back(static_cast<uint8_t>(tag.direction));
        }
    });
}

} // namespace

std::vector<uint8_t> pxa_encode(const Sprite &sprite)
{
    std::vector<uint8_t> out(MAGIC.begin(), MAGIC.end());
    putU32(out, PXA_VERSION);
    writeHead(out, sprite);
    writeDurations(out, sprite);
    writePalette(out, sprite.palette());
    for (int i = 0; i < sprite.layerCount(); i++) {
        writeLayer(out, *sprite.layer(i));
    }
    writeLinks(out, sprite);
    writeBlends(out, sprite);
    writeTags(out, sprite.tags());
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
