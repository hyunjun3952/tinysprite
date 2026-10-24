#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "path_util.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include "tinysprite/composite.hpp"
#include "tinysprite/io.hpp"
#include "write_error.hpp"

namespace
{

constexpr int JPEG_QUALITY = 95;

/* Puts the pixels over an opaque white background. */
void flattenOnWhite(std::vector<Rgba> &pixels)
{
    for (Rgba &p : pixels) {
        p = composite_over(Rgba{255, 255, 255, 255}, p, 255);
    }
}

/* Every pixel of `source` (`width` wide) becomes a scale x scale block. */
std::vector<Rgba> enlarge(const std::vector<Rgba> &source, int width, int scale)
{
    const size_t big = static_cast<size_t>(width) * static_cast<size_t>(scale);
    const size_t rows = source.size() / static_cast<size_t>(width) * static_cast<size_t>(scale);
    std::vector<Rgba> out(big * rows);
    for (size_t y = 0; y < rows; y++) {
        const size_t sourceRow = y / static_cast<size_t>(scale) * static_cast<size_t>(width);
        for (size_t x = 0; x < big; x++) {
            out[y * big + x] = source[sourceRow + x / static_cast<size_t>(scale)];
        }
    }
    return out;
}

} // namespace

bool image_export(const Sprite &sprite, int frame, const std::string &path, int scale)
{
    file_write_error_clear();
    if (frame < 0 || frame >= sprite.frameCount() || scale < 1 || scale > IMAGE_EXPORT_MAX_SCALE ||
        sprite.width() * scale > IMAGE_EXPORT_MAX_SIZE ||
        sprite.height() * scale > IMAGE_EXPORT_MAX_SIZE) {
        return false;
    }
    const int sourceWidth = sprite.width();
    const int sourceHeight = sprite.height();
    std::vector<Rgba> source(static_cast<size_t>(sourceWidth) * static_cast<size_t>(sourceHeight));
    composite_sprite(sprite, frame, Rect{0, 0, sourceWidth, sourceHeight}, source.data());
    const int width = sourceWidth * scale;
    const int height = sourceHeight * scale;
    std::vector<Rgba> flat = scale == 1 ? std::move(source) : enlarge(source, sourceWidth, scale);
    return image_write_rgba(path, width, height, flat);
}

namespace
{

/* One raster format the exporters write: the extension that selects it and the encoder. The first
 * extension of each format is the one listed in messages; an alias follows its format. */
struct RasterFormat {
    const char *extension;
    bool listed; /* shown by image_write_formats() */
    bool (*write)(const char *file, int width, int height, std::vector<Rgba> &pixels);
};

void appendChunk(void *context, void *data, int size)
{
    static_cast<std::string *>(context)->append(static_cast<const char *>(data),
                                                static_cast<size_t>(size));
}

/* Hands the encoded bytes to file_write_atomic, so a failure has a reason and never leaves a
 * half-written image. */
bool writeEncoded(const char *file, bool encoded, const std::string &bytes)
{
    return encoded && !bytes.empty() && file_write_atomic(file, bytes);
}

bool writePng(const char *file, int width, int height, std::vector<Rgba> &pixels)
{
    std::string bytes;
    const bool ok = stbi_write_png_to_func(appendChunk, &bytes, width, height, 4, pixels.data(),
                                           width * static_cast<int>(sizeof(Rgba))) != 0;
    return writeEncoded(file, ok, bytes);
}

bool writeBmp(const char *file, int width, int height, std::vector<Rgba> &pixels)
{
    std::string bytes;
    const bool ok =
        stbi_write_bmp_to_func(appendChunk, &bytes, width, height, 4, pixels.data()) != 0;
    return writeEncoded(file, ok, bytes);
}

bool writeTga(const char *file, int width, int height, std::vector<Rgba> &pixels)
{
    std::string bytes;
    const bool ok =
        stbi_write_tga_to_func(appendChunk, &bytes, width, height, 4, pixels.data()) != 0;
    return writeEncoded(file, ok, bytes);
}

/* JPEG has no alpha: the transparent parts show as white. */
bool writeJpg(const char *file, int width, int height, std::vector<Rgba> &pixels)
{
    flattenOnWhite(pixels);
    std::string bytes;
    const bool ok = stbi_write_jpg_to_func(appendChunk, &bytes, width, height, 4, pixels.data(),
                                           JPEG_QUALITY) != 0;
    return writeEncoded(file, ok, bytes);
}

const RasterFormat RASTER_FORMATS[] = {
    {".png", true, writePng}, {".bmp", true, writeBmp},   {".tga", true, writeTga},
    {".jpg", true, writeJpg}, {".jpeg", false, writeJpg},
};

} // namespace

bool image_write_rgba(const std::string &path, int width, int height, std::vector<Rgba> &pixels)
{
    file_write_error_clear();
    for (const RasterFormat &format : RASTER_FORMATS) {
        if (path_has_extension(path, format.extension)) {
            return format.write(path.c_str(), width, height, pixels);
        }
    }
    return false;
}

std::string image_write_formats()
{
    std::string out;
    std::string last;
    for (const RasterFormat &format : RASTER_FORMATS) {
        if (!format.listed) {
            continue;
        }
        if (!last.empty()) {
            out += out.empty() ? last : ", " + last;
        }
        last = format.extension;
    }
    return out + " or " + last;
}

namespace
{

/* Whether the header says the image fits a cel; read before decoding, which allocates the whole
 * image. */
bool fitsCel(int width, int height)
{
    return width >= 1 && height >= 1 && width <= CEL_MAX_DIMENSION && height <= CEL_MAX_DIMENSION;
}

/* The frames of an animated GIF as a sprite (one layer, a cel and duration per frame); nothing if
 * the file is not a GIF with more than one frame, so the caller falls back to the plain loader. */
std::optional<Sprite> importAnimatedGif(const std::string &path)
{
    std::ifstream file(path_from_utf8(path), std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    if (data.size() < 6 || std::memcmp(data.data(), "GIF8", 4) != 0) {
        return std::nullopt;
    }
    int headerWidth = 0;
    int headerHeight = 0;
    int headerChannels = 0;
    if (stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &headerWidth,
                              &headerHeight, &headerChannels) == 0 ||
        !fitsCel(headerWidth, headerHeight)) {
        return std::nullopt;
    }
    int *delays = nullptr;
    int width = 0;
    int height = 0;
    int frames = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_gif_from_memory(data.data(), static_cast<int>(data.size()), &delays, &width,
                                  &height, &frames, &channels, 4),
        &stbi_image_free);
    std::unique_ptr<int, decltype(&stbi_image_free)> delayHolder(delays, &stbi_image_free);
    if (!pixels || frames < 2 || !fitsCel(width, height)) {
        return std::nullopt;
    }
    frames = std::min(frames, SPRITE_MAX_FRAMES); /* the timeline has room for no more */
    Sprite sprite(width, height);
    const size_t frameBytes =
        static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(Rgba);
    for (int f = 1; f < frames; f++) {
        sprite.insertFrame(f, sprite.makeFrame());
    }
    for (int f = 0; f < frames; f++) {
        std::memcpy(sprite.layer(0)->cel(f)->pixels(),
                    pixels.get() + static_cast<size_t>(f) * frameBytes, frameBytes);
        /* a delay of 0 means "as fast as possible", which viewers show as 100 ms */
        int ms = delays[f] > 0 ? delays[f] : FRAME_DURATION_DEFAULT;
        sprite.setFrameDuration(f, std::min(ms, FRAME_DURATION_MAX));
    }
    return sprite;
}

} // namespace

std::optional<Sprite> image_import(const std::string &path)
{
    if (std::optional<Sprite> aseprite = aseprite_load(path)) {
        return aseprite;
    }
    if (std::optional<Sprite> animation = importAnimatedGif(path)) {
        return animation;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(path.c_str(), &width, &height, &channels) == 0 || !fitsCel(width, height)) {
        return std::nullopt;
    }
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.c_str(), &width, &height, &channels, 4), &stbi_image_free);
    if (!pixels || !fitsCel(width, height)) {
        return std::nullopt;
    }
    Sprite sprite(width, height);
    Cel &cel = *sprite.layer(0)->cel(0);
    std::memcpy(cel.pixels(), pixels.get(),
                static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(Rgba));
    return sprite;
}
