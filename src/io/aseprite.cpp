#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>

#include "byte_reader.hpp"
#include "tinysprite/inflate.hpp"
#include "tinysprite/io.hpp"

namespace
{

constexpr unsigned FILE_MAGIC = 0xA5E0;
constexpr unsigned FRAME_MAGIC = 0xF1FA;
constexpr size_t HEADER_BYTES = 128;
constexpr size_t FRAME_HEADER_BYTES = 16;
constexpr size_t CHUNK_HEADER_BYTES = 6;
constexpr size_t MAX_CEL_PIXELS = static_cast<size_t>(CEL_MAX_DIMENSION) * CEL_MAX_DIMENSION;
constexpr size_t MAX_LEVELS = 256; /* nesting of layer groups */

enum ChunkType : unsigned {
    CHUNK_OLD_PALETTE = 0x0004,
    CHUNK_LAYER = 0x2004,
    CHUNK_CEL = 0x2005,
    CHUNK_TAGS = 0x2018,
    CHUNK_PALETTE = 0x2019,
    CHUNK_SLICE = 0x2022,
};

enum LayerFlag : unsigned { LAYER_VISIBLE = 1, LAYER_EDITABLE = 2, LAYER_BACKGROUND = 8 };
enum LayerKind : unsigned { LAYER_IMAGE = 0, LAYER_GROUP = 1, LAYER_TILEMAP = 2 };
enum CelKind : unsigned { CEL_RAW = 0, CEL_LINKED = 1, CEL_COMPRESSED = 2 };
enum SliceFlag : unsigned { SLICE_NINE_PATCH = 1, SLICE_PIVOT = 2 };

int16_t signed16(uint32_t value)
{
    return static_cast<int16_t>(static_cast<uint16_t>(value));
}

int32_t signed32(uint32_t value)
{
    return static_cast<int32_t>(value);
}

/* A reader over the next `count` bytes of `in`, which are skipped in it. */
Reader takeReader(Reader &in, size_t count)
{
    const uint8_t *bytes = in.take(count);
    return {bytes, bytes + count};
}

std::string readString(Reader &in)
{
    uint32_t length = in.u16();
    const uint8_t *bytes = in.take(length);
    return {bytes, bytes + length};
}

BlendMode blendFor(unsigned aseprite)
{
    switch (aseprite) {
    case 1:
        return BlendMode::Multiply;
    case 2:
        return BlendMode::Screen;
    case 3:
        return BlendMode::Overlay;
    case 4:
        return BlendMode::Darken;
    case 5:
        return BlendMode::Lighten;
    case 10:
        return BlendMode::Difference;
    case 16:
        return BlendMode::Add;
    default:
        return BlendMode::Normal; /* the modes this editor lacks show as Normal */
    }
}

PlayMode directionFor(unsigned aseprite)
{
    switch (aseprite) {
    case 0:
        return PlayMode::Forward;
    case 1:
        return PlayMode::Reverse;
    default:
        return PlayMode::PingPong; /* 3 is ping-pong reverse */
    }
}

/* What an Aseprite layer chunk says. Groups and tilemap layers stay in the list too, since a cel
 * names its layer by its position among all of them; only image layers become layers. */
struct LayerInfo {
    std::string name;
    bool image = false;
    bool visible = true;
    bool locked = false;
    bool background = false;
    uint8_t opacity = 255;
    BlendMode blend = BlendMode::Normal;
};

/* The pixels of one layer and frame at full sprite size, or (a linked cel) the earlier frame whose
 * cel it shows. */
struct CelSlot {
    std::shared_ptr<Cel> cel;
    int linkTo = -1;
};

/* Where a cel chunk puts its pixels. */
struct CelPlace {
    int layer;
    int frame;
    int x;
    int y;
    unsigned opacity;
};

class AseDecoder
{
  public:
    explicit AseDecoder(const std::vector<uint8_t> &data)
        : file_(data.data(), data.data() + data.size())
    {
    }

    std::optional<Sprite> decode();

  private:
    void readHeader();
    void readFrame(int index);
    void readChunk(int frame, unsigned type, Reader &payload);
    void readLayer(Reader &chunk);
    void readCel(Reader &chunk, int frame);
    void readImageCel(Reader &chunk, const CelPlace &place, bool compressed);
    std::vector<uint8_t> celPixels(Reader &chunk, size_t expected, bool compressed);
    void readPalette(Reader &chunk);
    void readOldPalette(Reader &chunk);
    void readTags(Reader &chunk);
    void readSlice(Reader &chunk);
    Rgba pixelFrom(const uint8_t *p, bool background) const;
    void paint(Cel &cel, const std::vector<uint8_t> &pixels, int w, int h, const CelPlace &place);
    CelSlot &slot(int layer, int frame);
    bool levelShows(size_t level) const;
    std::optional<Sprite> build();
    void applyLayer(Layer &target, const LayerInfo &info, const std::vector<CelSlot> &slots) const;
    void applyMetadata(Sprite &sprite) const;

    Reader file_;
    int width_ = 0;
    int height_ = 0;
    int bytesPerPixel_ = 4;
    int frames_ = 0; /* the frames that are read: at most SPRITE_MAX_FRAMES */
    unsigned speed_ = FRAME_DURATION_DEFAULT;
    unsigned transparentIndex_ = 0;
    std::vector<LayerInfo> layers_;
    std::vector<bool> groupShows_; /* per nesting level: whether the layers inside show */
    std::vector<Rgba> palette_;
    std::vector<unsigned> durations_;
    std::vector<std::vector<CelSlot>> cels_; /* [Aseprite layer][frame], made as cels come */
    std::vector<Sprite::Tag> tags_;
    std::vector<Sprite::Slice> slices_;
    unsigned long long celBytes_ = 0;
};

void AseDecoder::readHeader()
{
    Reader h = takeReader(file_, HEADER_BYTES);
    h.u32(); /* the size of the file */
    unsigned magic = h.u16();
    unsigned frames = h.u16();
    unsigned width = h.u16();
    unsigned height = h.u16();
    unsigned depth = h.u16();
    h.u32(); /* flags */
    speed_ = h.u16();
    h.take(8);
    transparentIndex_ = h.u8();
    bool depthOk = depth == 8 || depth == 16 || depth == 32;
    bool sizeOk =
        width >= 1 && height >= 1 && width <= CEL_MAX_DIMENSION && height <= CEL_MAX_DIMENSION;
    if (magic != FILE_MAGIC || frames < 1 || !depthOk || !sizeOk) {
        throw std::out_of_range("bad aseprite header");
    }
    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);
    bytesPerPixel_ = static_cast<int>(depth / 8);
    frames_ = static_cast<int>(std::min<unsigned>(frames, SPRITE_MAX_FRAMES));
    if (speed_ == 0) {
        speed_ = FRAME_DURATION_DEFAULT;
    }
}

void AseDecoder::readFrame(int index)
{
    uint32_t bytes = file_.u32();
    if (bytes < FRAME_HEADER_BYTES) {
        throw std::out_of_range("bad frame");
    }
    uint32_t magic = file_.u16();
    uint32_t oldChunks = file_.u16();
    uint32_t duration = file_.u16();
    file_.take(2);
    uint32_t newChunks = file_.u32();
    if (magic != FRAME_MAGIC) {
        throw std::out_of_range("bad frame magic");
    }
    durations_.push_back(duration != 0 ? duration : speed_);
    Reader frame = takeReader(file_, bytes - FRAME_HEADER_BYTES);
    uint32_t count = newChunks;
    if (count == 0 && oldChunks != 0xFFFF) {
        count = oldChunks; /* older files count their chunks in the old field */
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t size = frame.u32();
        uint32_t type = frame.u16();
        if (size < CHUNK_HEADER_BYTES) {
            throw std::out_of_range("bad chunk");
        }
        Reader payload = takeReader(frame, size - CHUNK_HEADER_BYTES);
        readChunk(index, type, payload);
    }
}

void AseDecoder::readChunk(int frame, unsigned type, Reader &payload)
{
    switch (type) {
    case CHUNK_LAYER:
        readLayer(payload);
        break;
    case CHUNK_CEL:
        readCel(payload, frame);
        break;
    case CHUNK_PALETTE:
        readPalette(payload);
        break;
    case CHUNK_OLD_PALETTE:
        readOldPalette(payload);
        break;
    case CHUNK_TAGS:
        readTags(payload);
        break;
    case CHUNK_SLICE:
        readSlice(payload);
        break;
    default:
        break; /* user data, color profile, external files ... */
    }
}

/* Whether the layers at nesting `level` show: every group around them is visible. */
bool AseDecoder::levelShows(size_t level) const
{
    return level == 0 || level > groupShows_.size() || groupShows_[level - 1];
}

void AseDecoder::readLayer(Reader &chunk)
{
    unsigned flags = chunk.u16();
    unsigned kind = chunk.u16();
    size_t level = chunk.u16();
    chunk.take(4); /* default size, unused */
    unsigned blend = chunk.u16();
    LayerInfo info;
    info.opacity = static_cast<uint8_t>(chunk.u8());
    chunk.take(3);
    info.name = readString(chunk);
    if (level >= MAX_LEVELS) {
        throw std::out_of_range("layers nested too deep");
    }
    info.image = kind == LAYER_IMAGE;
    info.visible = (flags & LAYER_VISIBLE) != 0 && levelShows(level);
    info.locked = (flags & LAYER_EDITABLE) == 0;
    info.background = (flags & LAYER_BACKGROUND) != 0;
    info.blend = blendFor(blend);
    groupShows_.resize(std::max(groupShows_.size(), level + 1), true);
    groupShows_[level] = info.visible;
    layers_.push_back(std::move(info));
}

CelSlot &AseDecoder::slot(int layer, int frame)
{
    cels_.resize(layers_.size());
    std::vector<CelSlot> &row = cels_[static_cast<size_t>(layer)];
    row.resize(static_cast<size_t>(frames_));
    return row[static_cast<size_t>(frame)];
}

void AseDecoder::readCel(Reader &chunk, int frame)
{
    unsigned layer = chunk.u16();
    int x = signed16(chunk.u16());
    int y = signed16(chunk.u16());
    unsigned opacity = chunk.u8();
    unsigned kind = chunk.u16();
    chunk.take(7); /* z index and padding */
    if (layer >= layers_.size()) {
        throw std::out_of_range("cel of a missing layer");
    }
    if (!layers_[layer].image || frame >= frames_) {
        return; /* a tilemap layer, or a frame past the ones that are kept */
    }
    CelPlace place{static_cast<int>(layer), frame, x, y, opacity};
    if (kind == CEL_LINKED) {
        unsigned target = chunk.u16();
        if (target < static_cast<unsigned>(frame)) {
            slot(place.layer, frame).linkTo = static_cast<int>(target);
        }
    } else if (kind == CEL_RAW || kind == CEL_COMPRESSED) {
        readImageCel(chunk, place, kind == CEL_COMPRESSED);
    }
}

/* The w x h pixels of a cel in the file's color depth, plain or deflated. */
std::vector<uint8_t> AseDecoder::celPixels(Reader &chunk, size_t expected, bool compressed)
{
    if (!compressed) {
        const uint8_t *bytes = chunk.take(expected);
        return {bytes, bytes + expected};
    }
    size_t left = chunk.remaining();
    std::optional<std::vector<uint8_t>> pixels = zlib_inflate(chunk.take(left), left, expected);
    if (!pixels || pixels->size() != expected) {
        throw std::out_of_range("bad cel data");
    }
    return std::move(*pixels);
}

void AseDecoder::readImageCel(Reader &chunk, const CelPlace &place, bool compressed)
{
    size_t w = chunk.u16();
    size_t h = chunk.u16();
    if (w * h > MAX_CEL_PIXELS) {
        throw std::out_of_range("cel too big");
    }
    std::vector<uint8_t> pixels =
        celPixels(chunk, w * h * static_cast<size_t>(bytesPerPixel_), compressed);
    CelSlot &target = slot(place.layer, place.frame);
    if (!target.cel) {
        celBytes_ += static_cast<unsigned long long>(width_) * height_ * sizeof(Rgba);
        if (celBytes_ > SPRITE_MAX_CEL_BYTES) {
            throw std::out_of_range("too many pixels");
        }
        target.cel = std::make_shared<Cel>(width_, height_);
    }
    paint(*target.cel, pixels, static_cast<int>(w), static_cast<int>(h), place);
}

Rgba AseDecoder::pixelFrom(const uint8_t *p, bool background) const
{
    if (bytesPerPixel_ == 4) {
        return Rgba{p[0], p[1], p[2], p[3]};
    }
    if (bytesPerPixel_ == 2) {
        return Rgba{p[0], p[0], p[0], p[1]};
    }
    if (!background && p[0] == transparentIndex_) {
        return Rgba{};
    }
    return p[0] < palette_.size() ? palette_[p[0]] : Rgba{};
}

/* Copies a w x h cel with its top-left corner at the place, clipped to the sprite; the cel's
 * opacity goes into the alpha of its pixels, since layers only have one opacity of their own. */
void AseDecoder::paint(Cel &cel, const std::vector<uint8_t> &pixels, int w, int h,
                       const CelPlace &place)
{
    bool background = layers_[static_cast<size_t>(place.layer)].background;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t at = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) *
                        static_cast<size_t>(bytesPerPixel_);
            Rgba color = pixelFrom(&pixels[at], background);
            color.a = static_cast<uint8_t>((color.a * place.opacity + 127) / 255);
            cel.set(place.x + x, place.y + y, color);
        }
    }
}

void AseDecoder::readPalette(Reader &chunk)
{
    chunk.u32(); /* the size of the whole palette */
    uint32_t first = chunk.u32();
    uint32_t last = chunk.u32();
    chunk.take(8);
    if (last < first || last >= 0x10000) {
        throw std::out_of_range("bad palette");
    }
    for (uint32_t i = first; i <= last; i++) {
        unsigned flags = chunk.u16();
        Rgba color{static_cast<uint8_t>(chunk.u8()), static_cast<uint8_t>(chunk.u8()),
                   static_cast<uint8_t>(chunk.u8()), static_cast<uint8_t>(chunk.u8())};
        if ((flags & 1) != 0) {
            readString(chunk);
        }
        if (i < PALETTE_MAX_COLORS) {
            palette_.resize(std::max<size_t>(palette_.size(), i + 1));
            palette_[i] = color;
        }
    }
}

/* The old palette chunk: packets of colors with 8-bit channels, each after a number to skip. */
void AseDecoder::readOldPalette(Reader &chunk)
{
    uint32_t packets = chunk.u16();
    size_t index = 0;
    for (uint32_t i = 0; i < packets; i++) {
        index += chunk.u8();
        uint32_t count = chunk.u8();
        for (uint32_t c = count == 0 ? 256 : count; c > 0; c--, index++) {
            Rgba color{static_cast<uint8_t>(chunk.u8()), static_cast<uint8_t>(chunk.u8()),
                       static_cast<uint8_t>(chunk.u8()), 255};
            if (index < PALETTE_MAX_COLORS) {
                palette_.resize(std::max(palette_.size(), index + 1));
                palette_[index] = color;
            }
        }
    }
}

void AseDecoder::readTags(Reader &chunk)
{
    uint32_t count = chunk.u16();
    chunk.take(8);
    for (uint32_t i = 0; i < count; i++) {
        Sprite::Tag tag;
        tag.from = static_cast<int>(chunk.u16());
        tag.to = static_cast<int>(chunk.u16());
        tag.direction = directionFor(chunk.u8());
        chunk.take(2 + 6 + 3 + 1); /* repeat, reserved, color, extra byte */
        tag.name = readString(chunk);
        tags_.push_back(std::move(tag));
    }
}

/* A slice chunk; only its first key is used, since slices here are the same on every frame. */
void AseDecoder::readSlice(Reader &chunk)
{
    uint32_t keys = chunk.u32();
    uint32_t flags = chunk.u32();
    chunk.u32();
    Sprite::Slice slice;
    slice.name = readString(chunk);
    if (keys == 0) {
        return;
    }
    chunk.u32(); /* the frame the key starts at */
    int x = signed32(chunk.u32());
    int y = signed32(chunk.u32());
    uint32_t w = chunk.u32();
    uint32_t h = chunk.u32();
    if (w > INT32_MAX || h > INT32_MAX) {
        return;
    }
    slice.rect = Rect{x, y, static_cast<int>(w), static_cast<int>(h)};
    if ((flags & SLICE_NINE_PATCH) != 0) {
        chunk.take(16);
    }
    if ((flags & SLICE_PIVOT) != 0) {
        slice.hasPivot = true;
        slice.pivotX = signed32(chunk.u32());
        slice.pivotY = signed32(chunk.u32());
    }
    slices_.push_back(std::move(slice));
}

void AseDecoder::applyLayer(Layer &target, const LayerInfo &info,
                            const std::vector<CelSlot> &slots) const
{
    target.setName(info.name);
    target.setVisible(info.visible);
    target.setLocked(info.locked);
    target.setOpacity(info.opacity);
    target.setBlendMode(info.blend);
    std::vector<std::shared_ptr<Cel>> cels;
    for (int f = 0; f < frames_; f++) {
        const CelSlot empty;
        const CelSlot &s =
            static_cast<size_t>(f) < slots.size() ? slots[static_cast<size_t>(f)] : empty;
        if (s.cel) {
            cels.push_back(s.cel);
        } else if (s.linkTo >= 0) {
            cels.push_back(cels[static_cast<size_t>(s.linkTo)]); /* linkTo is an earlier frame */
        } else {
            cels.push_back(std::make_shared<Cel>(width_, height_));
        }
    }
    target.setCelList(std::move(cels));
}

void AseDecoder::applyMetadata(Sprite &sprite) const
{
    for (int f = 0; f < frames_; f++) {
        unsigned ms =
            std::clamp<unsigned>(durations_[static_cast<size_t>(f)], 1, FRAME_DURATION_MAX);
        sprite.setFrameDuration(f, static_cast<int>(ms));
    }
    for (const Rgba &color : palette_) {
        sprite.palette().add(color);
    }
    for (Sprite::Tag tag : tags_) {
        tag.to = std::min(tag.to, frames_ - 1);
        sprite.addTag(std::move(tag)); /* refuses a range outside the frames */
    }
    for (const Sprite::Slice &slice : slices_) {
        sprite.addSlice(slice); /* refuses one that does not fit the sprite */
    }
}

std::optional<Sprite> AseDecoder::build()
{
    unsigned long long imageLayers = 0;
    for (const LayerInfo &info : layers_) {
        imageLayers += info.image ? 1 : 0;
    }
    unsigned long long perLayer = static_cast<unsigned long long>(width_) * height_ * sizeof(Rgba) *
                                  static_cast<unsigned long long>(frames_);
    if (imageLayers * perLayer > SPRITE_MAX_CEL_BYTES) {
        return std::nullopt;
    }
    Sprite sprite(width_, height_);
    for (int f = 1; f < frames_; f++) {
        sprite.insertFrame(f, sprite.makeFrame());
    }
    cels_.resize(layers_.size());
    bool first = true;
    for (size_t i = 0; i < layers_.size(); i++) {
        if (!layers_[i].image) {
            continue;
        }
        Layer &target = first ? *sprite.layer(0) : sprite.addLayer(layers_[i].name);
        first = false;
        applyLayer(target, layers_[i], cels_[i]);
    }
    applyMetadata(sprite);
    return sprite;
}

std::optional<Sprite> AseDecoder::decode()
{
    readHeader();
    for (int f = 0; f < frames_; f++) {
        readFrame(f);
    }
    return build();
}

} // namespace

std::optional<Sprite> aseprite_decode(const std::vector<uint8_t> &data)
{
    try {
        return AseDecoder(data).decode();
    } catch (const std::out_of_range &) {
        return std::nullopt;
    } catch (const std::invalid_argument &) {
        return std::nullopt;
    } catch (const std::bad_alloc &) {
        return std::nullopt;
    }
}

std::optional<Sprite> aseprite_load(const std::string &path)
{
    std::ifstream file(path_from_utf8(path), std::ios::binary);
    uint8_t head[6] = {};
    if (!file.read(reinterpret_cast<char *>(head), sizeof head) ||
        (head[4] | (head[5] << 8)) != static_cast<int>(FILE_MAGIC)) {
        return std::nullopt; /* not an Aseprite file: do not read the rest of it */
    }
    std::vector<uint8_t> data(head, head + sizeof head);
    data.insert(data.end(), std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return aseprite_decode(data);
}
