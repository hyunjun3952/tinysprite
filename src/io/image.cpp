#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "stb_image.h"
#include "stb_image_write.h"
#include "tinysprite/composite.h"
#include "tinysprite/io.h"

namespace
{

constexpr int JPEG_QUALITY = 95;

bool hasExtension(const std::string &path, const std::string &extension)
{
    return path.size() >= extension.size() &&
           std::equal(extension.rbegin(), extension.rend(), path.rbegin(), [](char a, char b) {
               return a == std::tolower(static_cast<unsigned char>(b));
           });
}

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
    const char *file = path.c_str();
    const int stride = width * static_cast<int>(sizeof(Rgba));

    if (hasExtension(path, ".png")) {
        return stbi_write_png(file, width, height, 4, flat.data(), stride) != 0;
    }
    if (hasExtension(path, ".bmp")) {
        return stbi_write_bmp(file, width, height, 4, flat.data()) != 0;
    }
    if (hasExtension(path, ".tga")) {
        return stbi_write_tga(file, width, height, 4, flat.data()) != 0;
    }
    if (hasExtension(path, ".jpg") || hasExtension(path, ".jpeg")) {
        flattenOnWhite(flat);
        return stbi_write_jpg(file, width, height, 4, flat.data(), JPEG_QUALITY) != 0;
    }
    return false;
}

std::optional<Sprite> image_import(const std::string &path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.c_str(), &width, &height, &channels, 4), &stbi_image_free);
    if (!pixels || width > CEL_MAX_DIMENSION || height > CEL_MAX_DIMENSION) {
        return std::nullopt;
    }
    Sprite sprite(width, height);
    Cel &cel = *sprite.layer(0)->cel(0);
    std::memcpy(cel.pixels(), pixels.get(),
                static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(Rgba));
    return sprite;
}
