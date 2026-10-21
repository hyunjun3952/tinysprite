#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

#include "path_util.h"
#include "tinysprite/composite.h"
#include "tinysprite/io.h"
#include "tinysprite/playback.h"

namespace
{

constexpr int MAX_COLORS = 256;
constexpr int MIN_DELAY_CS = 2; /* viewers slow down anything shorter */
constexpr int MAX_CODES = 4096;

uint32_t packRgb(Rgba c)
{
    return static_cast<uint32_t>(c.r) << 16 | static_cast<uint32_t>(c.g) << 8 | c.b;
}

/* A pixel of the GIF: a palette index, kTransparent for none (resolved once the palette is known).
 */
constexpr int NOT_SHOWN = -1;

/* The colors of the animation and how they map to palette entries. */
struct GifPalette {
    std::vector<Rgba> entries; /* index 0 is transparent when `transparent` */
    bool transparent = false;
    std::unordered_map<uint32_t, int> indexOf; /* packed rgb -> entry */
};

struct Counted {
    uint32_t rgb;
    uint32_t count;
};

/* Median cut over the distinct colors: boxes are split along their widest channel at the median
 * (by pixel count) until there are `limit` of them; a box becomes its weighted average. */
std::vector<Rgba> medianCut(std::vector<Counted> colors, size_t limit)
{
    using Box = std::vector<Counted>;
    std::vector<Box> boxes;
    boxes.push_back(std::move(colors));
    auto channel = [](const Counted &c, int ch) { return (c.rgb >> (16 - 8 * ch)) & 0xff; };
    auto range = [&](const Box &box, int &widest) {
        uint32_t best = 0;
        for (int ch = 0; ch < 3; ch++) {
            uint32_t lo = 255;
            uint32_t hi = 0;
            for (const Counted &c : box) {
                lo = std::min(lo, channel(c, ch));
                hi = std::max(hi, channel(c, ch));
            }
            if (hi - lo >= best) {
                best = hi - lo;
                widest = ch;
            }
        }
        return best;
    };
    while (boxes.size() < limit) {
        size_t pick = boxes.size();
        uint32_t pickRange = 0;
        int pickChannel = 0;
        for (size_t i = 0; i < boxes.size(); i++) {
            int widest = 0;
            uint32_t r = boxes[i].size() > 1 ? range(boxes[i], widest) : 0;
            if (r > pickRange) {
                pickRange = r;
                pickChannel = widest;
                pick = i;
            }
        }
        if (pick == boxes.size()) {
            break; /* every box is a single color */
        }
        Box &box = boxes[pick];
        std::sort(box.begin(), box.end(), [&](const Counted &a, const Counted &b) {
            return channel(a, pickChannel) < channel(b, pickChannel);
        });
        uint64_t total = 0;
        for (const Counted &c : box) {
            total += c.count;
        }
        uint64_t seen = 0;
        size_t cut = 1;
        for (size_t i = 0; i + 1 < box.size(); i++) {
            seen += box[i].count;
            cut = i + 1;
            if (seen * 2 >= total) {
                break;
            }
        }
        Box upper(box.begin() + static_cast<std::ptrdiff_t>(cut), box.end());
        box.resize(cut);
        boxes.push_back(std::move(upper));
    }
    std::vector<Rgba> out;
    for (const Box &box : boxes) {
        uint64_t total = 0;
        uint64_t sum[3] = {0, 0, 0};
        for (const Counted &c : box) {
            total += c.count;
            for (int ch = 0; ch < 3; ch++) {
                sum[ch] += static_cast<uint64_t>(channel(c, ch)) * c.count;
            }
        }
        auto avg = [&](int ch) { return static_cast<uint8_t>((sum[ch] + total / 2) / total); };
        out.push_back(Rgba{avg(0), avg(1), avg(2), 255});
    }
    return out;
}

/* The palette for the flattened frames: the exact colors when they fit, else a median cut. */
GifPalette buildPalette(const std::vector<const std::vector<Rgba> *> &frames)
{
    GifPalette palette;
    std::map<uint32_t, uint32_t> histogram;
    for (const std::vector<Rgba> *frame : frames) {
        for (const Rgba &p : *frame) {
            if (p.a >= 128) {
                histogram[packRgb(p)]++;
            } else {
                palette.transparent = true;
            }
        }
    }
    size_t room = MAX_COLORS - (palette.transparent ? 1 : 0);
    std::vector<Counted> colors;
    colors.reserve(histogram.size());
    for (const auto &entry : histogram) {
        colors.push_back(Counted{entry.first, entry.second});
    }
    std::vector<Rgba> chosen;
    if (colors.size() <= room) {
        for (const Counted &c : colors) {
            chosen.push_back(Rgba{static_cast<uint8_t>(c.rgb >> 16),
                                  static_cast<uint8_t>(c.rgb >> 8), static_cast<uint8_t>(c.rgb),
                                  255});
        }
    } else {
        chosen = medianCut(colors, room);
    }
    if (palette.transparent) {
        palette.entries.push_back(Rgba{0, 0, 0, 0});
    }
    palette.entries.insert(palette.entries.end(), chosen.begin(), chosen.end());
    if (palette.entries.empty()) {
        palette.entries.push_back(Rgba{0, 0, 0, 255}); /* a blank sprite still needs a table */
    }
    /* every distinct color gets its nearest entry (itself, when it fits) */
    int first = palette.transparent ? 1 : 0;
    for (const Counted &c : colors) {
        int best = first;
        long bestDistance = -1;
        for (int i = first; i < static_cast<int>(palette.entries.size()); i++) {
            const Rgba &e = palette.entries[static_cast<size_t>(i)];
            long dr = static_cast<long>(c.rgb >> 16) - e.r;
            long dg = static_cast<long>((c.rgb >> 8) & 0xff) - e.g;
            long db = static_cast<long>(c.rgb & 0xff) - e.b;
            long d = dr * dr + dg * dg + db * db;
            if (bestDistance < 0 || d < bestDistance) {
                bestDistance = d;
                best = i;
                if (d == 0) {
                    break;
                }
            }
        }
        palette.indexOf[c.rgb] = best;
    }
    return palette;
}

void putU16(std::vector<uint8_t> &out, int value)
{
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

/* Writes variable-width codes, least significant bit first, as GIF data sub-blocks. */
class CodeWriter
{
  public:
    explicit CodeWriter(std::vector<uint8_t> &out) : out_(out)
    {
    }

    void put(uint32_t code, int width)
    {
        bits_ |= static_cast<uint64_t>(code) << count_;
        count_ += width;
        while (count_ >= 8) {
            byte(static_cast<uint8_t>(bits_ & 0xff));
            bits_ >>= 8;
            count_ -= 8;
        }
    }

    /* Flushes the last bits and ends the data with the empty block. */
    void finish()
    {
        if (count_ > 0) {
            byte(static_cast<uint8_t>(bits_ & 0xff));
        }
        flushBlock();
        out_.push_back(0);
    }

  private:
    void byte(uint8_t value)
    {
        block_.push_back(value);
        if (block_.size() == 255) {
            flushBlock();
        }
    }
    void flushBlock()
    {
        if (!block_.empty()) {
            out_.push_back(static_cast<uint8_t>(block_.size()));
            out_.insert(out_.end(), block_.begin(), block_.end());
            block_.clear();
        }
    }

    std::vector<uint8_t> &out_;
    std::vector<uint8_t> block_;
    uint64_t bits_ = 0;
    int count_ = 0;
};

/* GIF's variable-width LZW of the palette indexes. */
void lzwEncode(std::vector<uint8_t> &out, const std::vector<uint8_t> &indexes, int minCodeSize)
{
    out.push_back(static_cast<uint8_t>(minCodeSize));
    CodeWriter writer(out);
    const uint32_t clear = 1u << minCodeSize;
    const uint32_t end = clear + 1;
    uint32_t next = end + 1;
    int width = minCodeSize + 1;
    std::unordered_map<uint32_t, uint32_t> dictionary;

    writer.put(clear, width);
    uint32_t prefix = indexes.empty() ? 0 : indexes[0];
    for (size_t i = 1; i < indexes.size(); i++) {
        uint32_t key = prefix << 8 | indexes[i];
        auto it = dictionary.find(key);
        if (it != dictionary.end()) {
            prefix = it->second;
            continue;
        }
        writer.put(prefix, width);
        if (next < static_cast<uint32_t>(MAX_CODES)) {
            dictionary[key] = next;
            /* the decoder is one entry behind, so the width grows once entry 2^width exists */
            if (next == (1u << width)) {
                width++;
            }
            next++;
        } else {
            writer.put(clear, width);
            dictionary.clear();
            next = end + 1;
            width = minCodeSize + 1;
        }
        prefix = indexes[i];
    }
    if (!indexes.empty()) {
        writer.put(prefix, width);
    }
    writer.put(end, width);
    writer.finish();
}

/* The frame as palette indexes, every pixel enlarged to a scale x scale block. */
std::vector<uint8_t> indexFrame(const std::vector<Rgba> &pixels, int width, int height, int scale,
                                const GifPalette &palette)
{
    std::vector<uint8_t> source(pixels.size());
    for (size_t i = 0; i < pixels.size(); i++) {
        source[i] =
            pixels[i].a < 128 ? 0 : static_cast<uint8_t>(palette.indexOf.at(packRgb(pixels[i])));
    }
    if (scale == 1) {
        return source;
    }
    size_t big = static_cast<size_t>(width) * static_cast<size_t>(scale);
    std::vector<uint8_t> out(big * static_cast<size_t>(height) * static_cast<size_t>(scale));
    for (size_t y = 0; y < static_cast<size_t>(height) * static_cast<size_t>(scale); y++) {
        size_t row = y / static_cast<size_t>(scale) * static_cast<size_t>(width);
        for (size_t x = 0; x < big; x++) {
            out[y * big + x] = source[row + x / static_cast<size_t>(scale)];
        }
    }
    return out;
}

int tableBits(size_t entries)
{
    int bits = 1;
    while ((static_cast<size_t>(1) << bits) < entries) {
        bits++;
    }
    return bits;
}

} // namespace

std::vector<uint8_t> gif_encode(const Sprite &sprite, const GifOptions &options)
{
    int last = options.last < 0 ? sprite.frameCount() - 1 : options.last;
    if (options.first < 0 || last >= sprite.frameCount() || options.first > last ||
        options.scale < 1 || options.scale > IMAGE_EXPORT_MAX_SCALE ||
        sprite.width() * options.scale > IMAGE_EXPORT_MAX_SIZE ||
        sprite.height() * options.scale > IMAGE_EXPORT_MAX_SIZE) {
        return {};
    }
    const int width = sprite.width();
    const int height = sprite.height();
    std::vector<int> order = play_order(options.first, last, options.direction);

    /* every frame in the range, flattened once however often it is shown */
    std::vector<std::vector<Rgba>> flat(static_cast<size_t>(last - options.first + 1));
    std::vector<const std::vector<Rgba> *> distinct;
    for (int f = options.first; f <= last; f++) {
        std::vector<Rgba> &pixels = flat[static_cast<size_t>(f - options.first)];
        pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        composite_sprite(sprite, f, Rect{0, 0, width, height}, pixels.data());
        distinct.push_back(&pixels);
    }
    GifPalette palette = buildPalette(distinct);
    int bits = tableBits(palette.entries.size());
    int minCodeSize = std::max(2, bits);

    std::vector<uint8_t> out = {'G', 'I', 'F', '8', '9', 'a'};
    putU16(out, width * options.scale);
    putU16(out, height * options.scale);
    out.push_back(static_cast<uint8_t>(0x80 | (7 << 4) | (bits - 1)));
    out.push_back(0); /* background color index */
    out.push_back(0); /* square pixels */
    for (int i = 0; i < (1 << bits); i++) {
        Rgba c = static_cast<size_t>(i) < palette.entries.size()
                     ? palette.entries[static_cast<size_t>(i)]
                     : Rgba{0, 0, 0, 255};
        out.insert(out.end(), {c.r, c.g, c.b});
    }
    if (options.loop && order.size() > 1) {
        const char *app = "NETSCAPE2.0";
        out.insert(out.end(), {uint8_t{0x21}, uint8_t{0xff}, uint8_t{11}});
        out.insert(out.end(), app, app + 11);
        out.insert(out.end(),
                   {uint8_t{3}, uint8_t{1}, uint8_t{0}, uint8_t{0}, uint8_t{0}}); /* forever */
    }
    std::map<int, std::vector<uint8_t>> encoded; /* a frame shown twice is compressed once */
    for (int f : order) {
        int delay = (sprite.frameDuration(f) + 5) / 10;
        delay = std::min(std::max(delay, MIN_DELAY_CS), 65535);
        /* disposal 2 (clear to background) keeps transparent frames from piling up */
        const int disposal = palette.transparent ? 2 : 1;
        out.insert(out.end(),
                   {uint8_t{0x21}, uint8_t{0xf9}, uint8_t{4},
                    static_cast<uint8_t>(disposal << 2 | (palette.transparent ? 1 : 0))});
        putU16(out, delay);
        out.push_back(0); /* the transparent entry, when there is one, is index 0 */
        out.push_back(0);
        out.push_back(0x2c);
        putU16(out, 0);
        putU16(out, 0);
        putU16(out, width * options.scale);
        putU16(out, height * options.scale);
        out.push_back(0);
        std::vector<uint8_t> &data = encoded[f];
        if (data.empty()) {
            lzwEncode(data,
                      indexFrame(flat[static_cast<size_t>(f - options.first)], width, height,
                                 options.scale, palette),
                      minCodeSize);
        }
        out.insert(out.end(), data.begin(), data.end());
    }
    out.push_back(0x3b);
    return out;
}

bool gif_export(const Sprite &sprite, const std::string &path, const GifOptions &options)
{
    if (!path_has_extension(path, ".gif")) {
        return false;
    }
    std::vector<uint8_t> data = gif_encode(sprite, options);
    return !data.empty() && file_write_atomic(path, std::string(data.begin(), data.end()));
}
