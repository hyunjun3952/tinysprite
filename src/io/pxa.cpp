#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

#include "byte_reader.hpp"
#include "tinysprite/io.hpp"

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
    auto length = static_cast<uint32_t>(out.size() - lengthAt - 4);
    for (size_t i = 0; i < 4; i++) {
        out[lengthAt + i] = static_cast<uint8_t>(length >> (8 * i));
    }
}

/* Appends a name as a length byte and the bytes. */
void putName(std::vector<uint8_t> &out, const std::string &name)
{
    out.push_back(static_cast<uint8_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());
}

/* Reads a name written by putName. */
std::string readName(Reader &chunk)
{
    const uint32_t length = chunk.u8();
    const uint8_t *bytes = chunk.take(length);
    return {bytes, bytes + length};
}

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
    std::string name = readName(chunk);
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
        tag.name = readName(chunk);
        uint32_t from = chunk.u32();
        uint32_t to = chunk.u32();
        uint32_t direction = chunk.u8();
        if (from > INT32_MAX || to > INT32_MAX ||
            direction > static_cast<uint32_t>(PlayMode::PingPong) ||
            tag.name.size() > static_cast<size_t>(TAG_NAME_MAX)) {
            throw std::out_of_range("pxa: bad tag");
        }
        tag.from = static_cast<int>(from);
        tag.to = static_cast<int>(to);
        tag.direction = static_cast<PlayMode>(direction);
        tags.push_back(std::move(tag));
    }
}

/* One slice of an SLCE payload: its name, rectangle (four u32) and a flags byte (bit 0: a pivot
 * follows as two u32, relative to the rectangle). */
Sprite::Slice readSlice(Reader &chunk)
{
    Sprite::Slice slice;
    slice.name = readName(chunk);
    uint32_t values[4];
    for (uint32_t &value : values) {
        value = chunk.u32();
    }
    uint32_t flags = chunk.u8();
    if (flags > 1 || slice.name.size() > static_cast<size_t>(SLICE_NAME_MAX) ||
        *std::max_element(values, values + 4) > INT32_MAX) {
        throw std::out_of_range("pxa: bad slice");
    }
    slice.rect = Rect{static_cast<int>(values[0]), static_cast<int>(values[1]),
                      static_cast<int>(values[2]), static_cast<int>(values[3])};
    slice.hasPivot = flags == 1;
    if (slice.hasPivot) {
        uint32_t px = chunk.u32();
        uint32_t py = chunk.u32();
        if (px > INT32_MAX || py > INT32_MAX) {
            throw std::out_of_range("pxa: bad slice pivot");
        }
        slice.pivotX = static_cast<int>(px);
        slice.pivotY = static_cast<int>(py);
    }
    return slice;
}

/* An SLCE payload: the count, then that many slices. */
void readSlices(Reader &chunk, std::vector<Sprite::Slice> &slices)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        slices.push_back(readSlice(chunk));
    }
}

/* A GUID payload: the vertical guides then the horizontal ones, each a u16 count and that many u32
 * positions. */
void readGuides(Reader &chunk, Guides &guides)
{
    for (std::vector<int> *axis : {&guides.vertical, &guides.horizontal}) {
        uint32_t count = chunk.u16();
        for (uint32_t i = 0; i < count; i++) {
            uint32_t position = chunk.u32();
            if (position > INT32_MAX) {
                throw std::out_of_range("pxa: bad guide");
            }
            axis->push_back(static_cast<int>(position));
        }
    }
}

/* A TSET payload: the count, then per tileset its name, tile size, atlas size (u32 each) and the
 * atlas pixels. The pixels are taken before the cel is made, so a header that promises more than
 * the chunk holds never allocates. */
void readTilesets(Reader &chunk, std::vector<Tileset> &tilesets)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        Tileset tileset;
        tileset.name = readName(chunk);
        uint32_t values[4];
        for (uint32_t &value : values) {
            value = chunk.u32();
        }
        if (tileset.name.size() > static_cast<size_t>(TILE_NAME_MAX) || values[0] > TILE_SIZE_MAX ||
            values[1] > TILE_SIZE_MAX || values[2] < 1 || values[3] < 1 ||
            values[2] > CEL_MAX_DIMENSION || values[3] > CEL_MAX_DIMENSION) {
            throw std::out_of_range("pxa: bad tileset");
        }
        const size_t bytes = static_cast<size_t>(values[2]) * values[3] * sizeof(Rgba);
        const uint8_t *pixels = chunk.take(bytes);
        auto sized =
            std::make_shared<Cel>(static_cast<int>(values[2]), static_cast<int>(values[3]));
        std::memcpy(sized->pixels(), pixels, bytes);
        tileset.tileWidth = static_cast<int>(values[0]);
        tileset.tileHeight = static_cast<int>(values[1]);
        tileset.atlas = std::move(sized);
        tilesets.push_back(std::move(tileset));
    }
}

/* A TMAP payload: the count, then per map its name, tileset index, columns, rows (u32 each), a
 * visible byte and one u32 per cell. */
void readTilemaps(Reader &chunk, std::vector<TileMap> &maps)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        TileMap map;
        map.name = readName(chunk);
        uint32_t tileset = chunk.u32();
        uint32_t columns = chunk.u32();
        uint32_t rows = chunk.u32();
        uint32_t visible = chunk.u8();
        if (map.name.size() > static_cast<size_t>(TILE_NAME_MAX) || tileset > INT32_MAX ||
            columns < 1 || rows < 1 || columns > TILEMAP_MAX_DIMENSION ||
            rows > TILEMAP_MAX_DIMENSION || visible > 1) {
            throw std::out_of_range("pxa: bad tilemap");
        }
        map.tileset = static_cast<int>(tileset);
        map.columns = static_cast<int>(columns);
        map.rows = static_cast<int>(rows);
        map.visible = visible == 1;
        const size_t cells = static_cast<size_t>(columns) * rows;
        const uint8_t *raw = chunk.take(cells * 4);
        map.cells.resize(cells);
        for (size_t c = 0; c < cells; c++) {
            Reader one(raw + c * 4, raw + c * 4 + 4);
            map.cells[c] = one.u32();
        }
        maps.push_back(std::move(map));
    }
}

/* A TPOS payload: a u16 count, then the origin of that many maps in the order of the TMAP chunk,
 * x and y as u32 (two's complement). */
void readMapOrigins(Reader &chunk, std::vector<std::pair<int, int>> &origins)
{
    uint32_t count = chunk.u16();
    for (uint32_t i = 0; i < count; i++) {
        uint32_t x = chunk.u32();
        uint32_t y = chunk.u32();
        origins.emplace_back(static_cast<int32_t>(x), static_cast<int32_t>(y));
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
        keepValidSlices(*sprite_);
        keepValidGuides(*sprite_);
        /* Tile data is all or nothing: maps point into the tilesets by index, so a part that is
         * invalid costs the tiles but never the pixels. */
        placeMaps();
        sprite_->setTileData(std::move(tilesets_), std::move(maps_));
        return std::move(sprite_);
    }

  private:
    /* Where a map lies is metadata: an origin out of range, or for a map the file does not have, is
     * ignored and the map stays at (0, 0). */
    void placeMaps()
    {
        for (size_t i = 0; i < origins_.size() && i < maps_.size(); i++) {
            const auto [x, y] = origins_[i];
            if (std::abs(x) <= TILEMAP_OFFSET_MAX && std::abs(y) <= TILEMAP_OFFSET_MAX) {
                maps_[i].x = x;
                maps_[i].y = y;
            }
        }
    }

    /* Slices are metadata: one that does not fit the sprite (an empty name, a rectangle or pivot
     * out of bounds) is dropped, so a bad one never costs the pixels. */
    void keepValidSlices(Sprite &sprite)
    {
        std::vector<Sprite::Slice> valid;
        for (Sprite::Slice &slice : slices_) {
            if (sprite.sliceValid(slice)) {
                valid.push_back(std::move(slice));
            }
        }
        sprite.setSlices(std::move(valid));
    }

    /* Guides are metadata too: one off the sprite's edges is dropped. */
    void keepValidGuides(Sprite &sprite)
    {
        auto keep = [](std::vector<int> &lines, int size) {
            lines.erase(std::remove_if(lines.begin(), lines.end(),
                                       [size](int line) { return line < 0 || line > size; }),
                        lines.end());
        };
        keep(guides_.vertical, sprite.width());
        keep(guides_.horizontal, sprite.height());
        sprite.guides() = std::move(guides_);
    }

    using Read = bool (Decoder::*)(Reader &chunk, uint32_t length);
    struct ChunkReader {
        const char *tag;
        Read read;
    };
    static const ChunkReader CHUNKS[];

    bool readHead(Reader &chunk, uint32_t /*unused*/)
    {
        uint32_t width = chunk.u32();
        uint32_t height = chunk.u32();
        frames_ = chunk.u32();
        /* The Sprite constructor checks the size; the frames are created with the first layer,
         * once the file is known to hold their pixels. */
        if (sprite_ || frames_ < 1 || frames_ > SPRITE_MAX_FRAMES || width > CEL_MAX_DIMENSION ||
            height > CEL_MAX_DIMENSION) {
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

    bool readPaletteChunk(Reader &chunk, uint32_t /*unused*/)
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

    bool readLinkChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readLinks(chunk, links_);
        return true;
    }

    bool readBlendChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readBlends(chunk, blends_);
        return true;
    }

    bool readTagChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readTags(chunk, tags_);
        return true;
    }

    bool readSliceChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readSlices(chunk, slices_);
        return true;
    }

    bool readGuideChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readGuides(chunk, guides_);
        return true;
    }

    bool readTilesetChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readTilesets(chunk, tilesets_);
        return true;
    }

    bool readTilemapChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readTilemaps(chunk, maps_);
        return true;
    }

    bool readOriginChunk(Reader &chunk, uint32_t /*unused*/)
    {
        readMapOrigins(chunk, origins_);
        return true;
    }

    std::optional<Sprite> sprite_;
    int layers_ = 0;
    uint32_t frames_ = 1;
    std::vector<uint32_t> durations_;   /* from FRAM, applied once the frames exist */
    std::vector<Sprite::Tag> tags_;     /* from TAGS, checked against the frames at the end */
    std::vector<Sprite::Slice> slices_; /* from SLCE, checked against the size at the end */
    Guides guides_;                     /* from GUID, checked against the size at the end */
    std::vector<Tileset> tilesets_;     /* from TSET and TMAP, checked together at the end */
    std::vector<TileMap> maps_;
    std::vector<std::pair<int, int>> origins_; /* from TPOS, applied to maps_ at the end */
    std::vector<CelLink> links_;               /* from LINK, applied once the layers exist */
    std::vector<LayerBlend> blends_;           /* from BLND, applied once the layers exist */
};

const Decoder::ChunkReader Decoder::CHUNKS[] = {
    {"HEAD", &Decoder::readHead},         {"FRAM", &Decoder::readDurations},
    {"PALT", &Decoder::readPaletteChunk}, {"LAYR", &Decoder::readLayerChunk},
    {"LINK", &Decoder::readLinkChunk},    {"BLND", &Decoder::readBlendChunk},
    {"TAGS", &Decoder::readTagChunk},     {"SLCE", &Decoder::readSliceChunk},
    {"TSET", &Decoder::readTilesetChunk}, {"TMAP", &Decoder::readTilemapChunk},
    {"GUID", &Decoder::readGuideChunk},   {"TPOS", &Decoder::readOriginChunk},
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
            const auto *bytes = reinterpret_cast<const uint8_t *>(cel.pixels());
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
            putName(o, tag.name);
            putU32(o, static_cast<uint32_t>(tag.from));
            putU32(o, static_cast<uint32_t>(tag.to));
            o.push_back(static_cast<uint8_t>(tag.direction));
        }
    });
}

/* Appends one slice of an SLCE payload, as readSlice reads it. */
void putSlice(std::vector<uint8_t> &out, const Sprite::Slice &slice)
{
    putName(out, slice.name);
    for (int value : {slice.rect.x, slice.rect.y, slice.rect.w, slice.rect.h}) {
        putU32(out, static_cast<uint32_t>(value));
    }
    out.push_back(slice.hasPivot ? 1 : 0);
    if (slice.hasPivot) {
        putU32(out, static_cast<uint32_t>(slice.pivotX));
        putU32(out, static_cast<uint32_t>(slice.pivotY));
    }
}

void writeSlices(std::vector<uint8_t> &out, const std::vector<Sprite::Slice> &slices)
{
    if (slices.empty()) {
        return;
    }
    putChunk(out, "SLCE", [&](std::vector<uint8_t> &o) {
        const size_t count = std::min<size_t>(slices.size(), UINT16_MAX); /* the count is a u16 */
        putU16(o, static_cast<uint32_t>(count));
        for (size_t i = 0; i < count; i++) {
            putSlice(o, slices[i]);
        }
    });
}

/* The guides that lie on the sprite (0..size), at most as many as a u16 count can hold. */
std::vector<int> guidesOnSprite(const std::vector<int> &lines, int size)
{
    std::vector<int> kept;
    for (int line : lines) {
        if (line >= 0 && line <= size && kept.size() < UINT16_MAX) {
            kept.push_back(line);
        }
    }
    return kept;
}

/* Writes the guides that lie on the sprite (0..size); nothing at all when none are left. */
void writeGuides(std::vector<uint8_t> &out, const Sprite &sprite)
{
    const std::vector<int> vertical = guidesOnSprite(sprite.guides().vertical, sprite.width());
    const std::vector<int> horizontal = guidesOnSprite(sprite.guides().horizontal, sprite.height());
    if (vertical.empty() && horizontal.empty()) {
        return;
    }
    putChunk(out, "GUID", [&](std::vector<uint8_t> &o) {
        for (const std::vector<int> *axis : {&vertical, &horizontal}) {
            putU16(o, static_cast<uint32_t>(axis->size()));
            for (int line : *axis) {
                putU32(o, static_cast<uint32_t>(line));
            }
        }
    });
}

void writeTilesets(std::vector<uint8_t> &out, const std::vector<Tileset> &tilesets)
{
    putChunk(out, "TSET", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(tilesets.size()));
        for (const Tileset &tileset : tilesets) {
            putName(o, tileset.name);
            for (int value : {tileset.tileWidth, tileset.tileHeight, tileset.atlas->width(),
                              tileset.atlas->height()}) {
                putU32(o, static_cast<uint32_t>(value));
            }
            const auto *raw = reinterpret_cast<const uint8_t *>(tileset.atlas->pixels());
            o.insert(o.end(), raw, raw + pixelBytes(*tileset.atlas));
        }
    });
}

void writeTilemaps(std::vector<uint8_t> &out, const std::vector<TileMap> &maps)
{
    putChunk(out, "TMAP", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(maps.size()));
        for (const TileMap &map : maps) {
            putName(o, map.name);
            for (int value : {map.tileset, map.columns, map.rows}) {
                putU32(o, static_cast<uint32_t>(value));
            }
            o.push_back(map.visible ? 1 : 0);
            for (uint32_t cell : map.cells) {
                putU32(o, cell);
            }
        }
    });
}

/* The origin of every map, in the order of the TMAP chunk; nothing when all lie at (0, 0). */
void writeMapOrigins(std::vector<uint8_t> &out, const std::vector<TileMap> &maps)
{
    const bool moved = std::any_of(maps.begin(), maps.end(),
                                   [](const TileMap &map) { return map.x != 0 || map.y != 0; });
    if (!moved) {
        return;
    }
    putChunk(out, "TPOS", [&](std::vector<uint8_t> &o) {
        putU16(o, static_cast<uint32_t>(maps.size()));
        for (const TileMap &map : maps) {
            putU32(o, static_cast<uint32_t>(map.x));
            putU32(o, static_cast<uint32_t>(map.y));
        }
    });
}

void writeTiles(std::vector<uint8_t> &out, const Sprite &sprite)
{
    if (!sprite.tilesets().empty()) {
        writeTilesets(out, sprite.tilesets());
    }
    if (!sprite.tilemaps().empty()) {
        writeTilemaps(out, sprite.tilemaps());
        writeMapOrigins(out, sprite.tilemaps());
    }
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
    writeSlices(out, sprite.slices());
    writeTiles(out, sprite);
    writeGuides(out, sprite);
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
    return file_write_atomic(path, data.data(), data.size());
}

std::optional<Sprite> pxa_load(const std::string &path)
{
    std::ifstream file(path_from_utf8(path), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return pxa_decode(std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                           std::istreambuf_iterator<char>()));
}
