#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

#include "tinysprite/composite.hpp"
#include "tinysprite/io.hpp"
#include "tinysprite/playback.hpp"
#include "util.hpp"

namespace
{

constexpr int MAX_COLORS = 256;
constexpr int MIN_DELAY_CS = 2; /* viewers slow down anything shorter */
constexpr int MAX_CODES = 4096;

uint32_t packRgb(Rgba c)
{
    return static_cast<uint32_t>(c.r) << 16 | static_cast<uint32_t>(c.g) << 8 | c.b;
}

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

/* A group of colors for the median cut. */
using Box = std::vector<Counted>;

uint32_t channelOf(const Counted &c, int channel)
{
    return (c.rgb >> (16 - 8 * channel)) & 0xff;
}

/* How far apart the box's colors are along its widest channel, and which channel that is. */
struct Spread {
    uint32_t range = 0;
    int channel = 0;
};

Spread widestSpread(const Box &box)
{
    Spread best;
    for (int ch = 0; ch < 3; ch++) {
        uint32_t lo = 255;
        uint32_t hi = 0;
        for (const Counted &c : box) {
            lo = std::min(lo, channelOf(c, ch));
            hi = std::max(hi, channelOf(c, ch));
        }
        if (hi - lo >= best.range) {
            best = Spread{hi - lo, ch};
        }
    }
    return best;
}

/* The box with the widest spread (the first one on a tie) and that spread; `range` is 0 when
 * every box is a single color. */
struct Pick {
    size_t box = 0;
    Spread spread;
};

Pick widestBox(const std::vector<Spread> &spreads)
{
    Pick pick;
    for (size_t i = 0; i < spreads.size(); i++) {
        if (spreads[i].range > pick.spread.range) {
            pick = Pick{i, spreads[i]};
        }
    }
    return pick;
}

/* The spread of a box that is worth cutting: none for a single color. */
Spread cuttableSpread(const Box &box)
{
    return box.size() > 1 ? widestSpread(box) : Spread{};
}

/* Where to cut a box sorted along its widest channel: after the color that brings the pixel
 * count to half, keeping at least one color on each side. */
size_t medianCutIndex(const Box &box)
{
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
    return cut;
}

/* Splits the box along `channel` at the median, moving the upper half into a new box. */
Box splitBox(Box &box, int channel)
{
    std::sort(box.begin(), box.end(), [channel](const Counted &a, const Counted &b) {
        return channelOf(a, channel) < channelOf(b, channel);
    });
    size_t cut = medianCutIndex(box);
    Box upper(box.begin() + static_cast<std::ptrdiff_t>(cut), box.end());
    box.resize(cut);
    return upper;
}

/* The weighted average color of a box. */
Rgba averageColor(const Box &box)
{
    uint64_t total = 0;
    uint64_t sum[3] = {0, 0, 0};
    for (const Counted &c : box) {
        total += c.count;
        for (int ch = 0; ch < 3; ch++) {
            sum[ch] += static_cast<uint64_t>(channelOf(c, ch)) * c.count;
        }
    }
    auto avg = [&](int ch) { return static_cast<uint8_t>((sum[ch] + total / 2) / total); };
    return Rgba{avg(0), avg(1), avg(2), 255};
}

/* Median cut over the distinct colors: boxes are split along their widest channel at the median
 * (by pixel count) until there are `limit` of them; a box becomes its weighted average. */
std::vector<Box> medianCut(std::vector<Counted> colors, size_t limit)
{
    std::vector<Box> boxes;
    boxes.push_back(std::move(colors));
    std::vector<Spread> spreads = {cuttableSpread(boxes[0])}; /* of each box, kept up to date */
    while (boxes.size() < limit) {
        Pick pick = widestBox(spreads);
        if (pick.spread.range == 0) {
            break; /* every box is a single color */
        }
        boxes.push_back(splitBox(boxes[pick.box], pick.spread.channel));
        spreads[pick.box] = cuttableSpread(boxes[pick.box]);
        spreads.push_back(cuttableSpread(boxes.back()));
    }
    return boxes;
}

/* Counts pixels per color. A run of one color (pixel art has long ones) is added to the table once,
 * when it ends, so the table is touched per run while its size stays bounded by the number of
 * distinct colors, not by the number of pixels. */
struct ColorCounter {
    std::unordered_map<uint32_t, uint32_t> counts;
    uint32_t runRgb = 0;
    uint32_t runLength = 0; /* 0: no run open */

    void add(uint32_t rgb)
    {
        if (runLength != 0 && rgb == runRgb) {
            runLength++;
            return;
        }
        endRun();
        runRgb = rgb;
        runLength = 1;
    }
    void endRun()
    {
        if (runLength != 0) {
            counts[runRgb] += runLength;
        }
        runLength = 0;
    }
};

/* How many of the frames' pixels each opaque color covers, ascending by color so the palette is the
 * same every time; sets `transparent` when any pixel is see-through. */
std::vector<Counted> colorHistogram(const std::vector<const std::vector<Rgba> *> &frames,
                                    bool &transparent)
{
    ColorCounter counter;
    for (const std::vector<Rgba> *frame : frames) {
        for (const Rgba &p : *frame) {
            if (p.a < 128) {
                transparent = true;
            } else {
                counter.add(packRgb(p));
            }
        }
    }
    counter.endRun();
    std::vector<Counted> histogram;
    histogram.reserve(counter.counts.size());
    for (const auto &entry : counter.counts) {
        histogram.push_back(Counted{entry.first, entry.second});
    }
    std::sort(histogram.begin(), histogram.end(),
              [](const Counted &a, const Counted &b) { return a.rgb < b.rgb; });
    return histogram;
}

/* The squared distance between the color and the entry, or a partial sum that is already at least
 * `limit` as soon as one is: the caller only wants to know whether it beats `limit`. */
long colorDistance(uint32_t rgb, const Rgba &entry, long limit)
{
    const long dr = static_cast<long>((rgb >> 16) & 0xff) - entry.r;
    long distance = dr * dr;
    if (distance >= limit) {
        return distance;
    }
    const long dg = static_cast<long>((rgb >> 8) & 0xff) - entry.g;
    distance += dg * dg;
    if (distance >= limit) {
        return distance;
    }
    const long db = static_cast<long>(rgb & 0xff) - entry.b;
    return distance + db * db;
}

/* The index, from `first` on, of the entry closest to the color (itself, when it is there). */
int nearestEntry(const std::vector<Rgba> &entries, int first, uint32_t rgb)
{
    int best = first;
    long bestDistance = LONG_MAX;
    for (int i = first; i < static_cast<int>(entries.size()) && bestDistance != 0; i++) {
        const long d = colorDistance(rgb, entries[static_cast<size_t>(i)], bestDistance);
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

/* Gives every color its own entry, after the ones the palette has. */
void addExactEntries(GifPalette &palette, const std::vector<Counted> &colors)
{
    for (const Counted &c : colors) {
        palette.indexOf[c.rgb] = static_cast<int>(palette.entries.size());
        palette.entries.push_back(averageColor(Box{c}));
    }
}

/* Reduces the colors to at most `room` median-cut entries, after the ones the palette has. A color
 * maps to the nearest of them, not to its own box: one near the edge of its box can be closer to
 * the average of the next one. */
void addCutEntries(GifPalette &palette, const std::vector<Counted> &colors, size_t room)
{
    const int first = static_cast<int>(palette.entries.size());
    for (const Box &box : medianCut(colors, room)) {
        palette.entries.push_back(averageColor(box));
    }
    for (const Counted &c : colors) {
        palette.indexOf[c.rgb] = nearestEntry(palette.entries, first, c.rgb);
    }
}

/* The palette for the flattened frames: the exact colors when they fit, else a median cut. */
GifPalette buildPalette(const std::vector<const std::vector<Rgba> *> &frames)
{
    GifPalette palette;
    std::vector<Counted> colors = colorHistogram(frames, palette.transparent);
    size_t room = MAX_COLORS - (palette.transparent ? 1 : 0);
    if (palette.transparent) {
        palette.entries.push_back(Rgba{0, 0, 0, 0});
    }
    if (colors.size() > room) {
        addCutEntries(palette, colors, room);
    } else {
        addExactEntries(palette, colors);
    }
    if (palette.entries.empty()) {
        palette.entries.push_back(Rgba{0, 0, 0, 255}); /* a blank sprite still needs a table */
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

/* The last frame to write, or -1 when the options ask for something that cannot be encoded. */
int validLastFrame(const Sprite &sprite, const GifOptions &options)
{
    int last = frame_range_end(sprite, options.first, options.last);
    bool badScale = options.scale < 1 || options.scale > IMAGE_EXPORT_MAX_SCALE ||
                    sprite.width() * options.scale > IMAGE_EXPORT_MAX_SIZE ||
                    sprite.height() * options.scale > IMAGE_EXPORT_MAX_SIZE;
    return badScale ? -1 : last;
}

/* The header, the logical screen with the global color table, and the looping extension. */
void writeHeader(std::vector<uint8_t> &out, const Sprite &sprite, const GifOptions &options,
                 const GifPalette &palette, int bits, bool loops)
{
    out.insert(out.end(), {'G', 'I', 'F', '8', '9', 'a'});
    putU16(out, sprite.width() * options.scale);
    putU16(out, sprite.height() * options.scale);
    out.push_back(static_cast<uint8_t>(0x80 | (7 << 4) | (bits - 1)));
    out.push_back(0); /* background color index */
    out.push_back(0); /* square pixels */
    for (int i = 0; i < (1 << bits); i++) {
        Rgba c = static_cast<size_t>(i) < palette.entries.size()
                     ? palette.entries[static_cast<size_t>(i)]
                     : Rgba{0, 0, 0, 255};
        out.insert(out.end(), {c.r, c.g, c.b});
    }
    if (loops) {
        const char *app = "NETSCAPE2.0";
        out.insert(out.end(), {uint8_t{0x21}, uint8_t{0xff}, uint8_t{11}});
        out.insert(out.end(), app, app + 11);
        out.insert(out.end(),
                   {uint8_t{3}, uint8_t{1}, uint8_t{0}, uint8_t{0}, uint8_t{0}}); /* forever */
    }
}

/* A frame's graphic control extension and image descriptor, ahead of its compressed data. */
void writeFrameHeader(std::vector<uint8_t> &out, int delayMs, bool transparent, int width,
                      int height)
{
    int delay = std::min(std::max((delayMs + 5) / 10, MIN_DELAY_CS), 65535);
    /* disposal 2 (clear to background) keeps transparent frames from piling up */
    const int disposal = transparent ? 2 : 1;
    out.insert(out.end(), {uint8_t{0x21}, uint8_t{0xf9}, uint8_t{4},
                           static_cast<uint8_t>(disposal << 2 | (transparent ? 1 : 0))});
    putU16(out, delay);
    out.push_back(0); /* the transparent entry, when there is one, is index 0 */
    out.push_back(0);
    out.push_back(0x2c);
    putU16(out, 0);
    putU16(out, 0);
    putU16(out, width);
    putU16(out, height);
    out.push_back(0);
}

} // namespace

std::vector<uint8_t> gif_encode(const Sprite &sprite, const GifOptions &options)
{
    int last = validLastFrame(sprite, options);
    if (last < 0) {
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

    std::vector<uint8_t> out;
    writeHeader(out, sprite, options, palette, bits, options.loop && order.size() > 1);
    std::map<int, std::vector<uint8_t>> encoded; /* a frame shown twice is compressed once */
    for (int f : order) {
        writeFrameHeader(out, sprite.frameDuration(f), palette.transparent, width * options.scale,
                         height * options.scale);
        std::vector<uint8_t> &data = encoded[f];
        if (data.empty()) {
            lzwEncode(data,
                      indexFrame(flat[static_cast<size_t>(f - options.first)], width, height,
                                 options.scale, palette),
                      std::max(2, bits));
        }
        out.insert(out.end(), data.begin(), data.end());
    }
    out.push_back(0x3b);
    return out;
}

namespace
{

/* Moves `at` past the data sub-blocks that start there (each a length byte and that many bytes,
 * ended by a zero length); false if the data ends first. */
bool skipSubBlocks(const std::vector<uint8_t> &data, size_t &at)
{
    while (at < data.size()) {
        const size_t length = data[at++];
        if (length == 0) {
            return true;
        }
        at += length;
    }
    return false;
}

/* The size of a color table whose presence and size are in `flags` (bit 7, bits 0-2). */
size_t colorTableBytes(uint8_t flags)
{
    return (flags & 0x80) != 0 ? size_t{3} << ((flags & 7) + 1) : 0;
}

} // namespace

int gif_frame_count(const std::vector<uint8_t> &data)
{
    constexpr size_t SCREEN_END = 13; /* "GIF89a", the screen descriptor */
    if (data.size() < SCREEN_END || data[0] != 'G' || data[1] != 'I' || data[2] != 'F') {
        return 0;
    }
    size_t at = SCREEN_END + colorTableBytes(data[10]);
    int frames = 0;
    while (at < data.size() && data[at] != 0x3B) {
        const uint8_t kind = data[at++];
        if (kind == 0x21) { /* an extension: its label, then its sub-blocks */
            at++;
        } else if (kind == 0x2C) { /* an image: the descriptor, a color table, the LZW data */
            if (at + 9 > data.size()) {
                return 0;
            }
            at += 9 + colorTableBytes(data[at + 8]) + 1;
            frames++;
        } else {
            return 0;
        }
        if (!skipSubBlocks(data, at)) {
            return 0;
        }
    }
    return frames;
}

bool gif_export(const Sprite &sprite, const std::string &path, const GifOptions &options)
{
    file_write_error_clear();
    if (!path_has_extension(path, ".gif")) {
        return false;
    }
    std::vector<uint8_t> data = gif_encode(sprite, options);
    return !data.empty() && file_write_atomic(path, data.data(), data.size());
}
