#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>

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

} // namespace

bool image_export(const Sprite &sprite, int frame, const std::string &path)
{
    if (frame < 0 || frame >= sprite.frameCount()) {
        return false;
    }
    const int width = sprite.width();
    const int height = sprite.height();
    std::vector<Rgba> flat(static_cast<size_t>(width) * static_cast<size_t>(height));
    composite_sprite(sprite, frame, Rect{0, 0, width, height}, flat.data());
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
